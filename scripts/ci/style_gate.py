#!/usr/bin/env python3
"""Read-only checks shared by Git's staged hook and a clean CI checkout."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/ci"))
sys.path.insert(0, str(ROOT / "scripts/tools"))
sys.path.insert(0, str(ROOT / "scripts/setup"))
import format as formatting
import comment_style
import install_dev_tools

BASELINE_PATH = "dependencies/style-baseline.json"
# This is the reviewed pre-automation source, not an adjustable debt budget.
BOOTSTRAP_SOURCE = "b68922d1b29291734a2b056f3b8229a0bebd654c"
CONTROL_FILES = {
    ".clang-format", ".clang-format-dirs", ".editorconfig",
    ".pre-commit-config.yaml", BASELINE_PATH,
    "dependencies/development-tools.txt", "scripts/tools/format.py",
    "scripts/ci/style_gate.py", "scripts/ci/comment_style.py",
    ".kiro/steering/comment-standards.md",
    "docs/sphinx/development/coding_standards.rst",
}
SOURCE_SUFFIXES = {".c", ".h", ".cpp", ".hpp", ".cc", ".hh", ".inc"}
TEXT_SUFFIXES = SOURCE_SUFFIXES | {
    ".py", ".cmake", ".txt", ".rst", ".json", ".yml", ".yaml",
    ".sh", ".ps1", ".bat", ".md", ".cfg", ".ini",
}


def git(root, *arguments, optional=False):
    result = subprocess.run(
        ["git", *arguments], cwd=root, capture_output=True, check=False,
    )
    if result.returncode:
        if optional:
            return None
        raise ValueError(result.stderr.decode("utf-8", "replace").strip())
    return result.stdout


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(contents):
    return json.loads(contents, object_pairs_hook=unique_keys)


def digest(contents):
    return hashlib.sha256(contents).hexdigest()


def blob(root, revision, path):
    if git(root, "cat-file", "-e", f"{revision}:{path}", optional=True) is None:
        return None
    return git(root, "show", f"{revision}:{path}")


def locked_versions(root):
    return install_dev_tools.read_lock(root / "dependencies/development-tools.txt")


def validate_baseline(data, expected_version):
    if not isinstance(data, dict) or set(data) != {
        "schema_version", "source_revision", "formatter_version", "format", "comments",
    } or type(data["schema_version"]) is not int or data["schema_version"] != 1:
        raise ValueError("Invalid style baseline schema")
    if data["source_revision"] != BOOTSTRAP_SOURCE:
        raise ValueError("The initial debt source cannot be changed")
    if data["formatter_version"] != expected_version:
        raise ValueError("Formatter version differs from the sealed baseline")
    if not isinstance(data["format"], dict):
        raise ValueError("Format baseline must be a mapping")
    for path, value in data["format"].items():
        if (
            not isinstance(path, str) or not path or "\\" in path
            or any(part in {"", ".", ".."} for part in path.split("/"))
            or Path(path).is_absolute() or not isinstance(value, str)
            or not re.fullmatch(r"[0-9a-f]{64}", value)
        ):
            raise ValueError(f"Invalid format baseline entry: {path}")
    comment_style.validate_baseline(data["comments"])
    return data


def check_baseline_change(root, data, base_ref):
    """Allow removals and lower counts, never new debt or a changed identity."""
    previous = blob(root, base_ref, BASELINE_PATH)
    if previous is None:
        # Initial installation can only record bytes from this fixed old source.
        for path, source_hash in data["format"].items():
            old_source = blob(root, BOOTSTRAP_SOURCE, path)
            if old_source is None or digest(old_source) != source_hash:
                raise ValueError(f"Baseline cannot exempt new/modified source: {path}")
        for path, record in data["comments"]["files"].items():
            old_source = blob(root, BOOTSTRAP_SOURCE, path)
            if old_source is None or digest(old_source) != record["source_sha256"]:
                raise ValueError(f"Comment baseline cannot exempt new source: {path}")
            actual = comment_style.inspect_source(path, old_source)["rules"]
            if record["rules"] != actual:
                raise ValueError(f"Baseline counts differ from actual old source: {path}")
        return BOOTSTRAP_SOURCE
    old = validate_baseline(load_json(previous), data["formatter_version"])
    for path, source_hash in data["format"].items():
        if old["format"].get(path) != source_hash:
            raise ValueError(f"Format debt cannot be added/rebased: {path}")
    for path, record in data["comments"]["files"].items():
        old_record = old["comments"]["files"].get(path)
        if not old_record or record["source_sha256"] != old_record["source_sha256"]:
            raise ValueError(f"Comment debt cannot be added/rebased: {path}")
        for rule, count in record["rules"].items():
            if count > old_record["rules"].get(rule, 0):
                raise ValueError(f"Comment debt cannot increase: {path}: {rule}")
    return base_ref


def check_text(path, relative):
    contents = path.read_bytes()
    if b"\0" in contents:
        raise ValueError(f"Text file contains NUL bytes: {relative}")
    text = contents.decode("utf-8")
    errors = []
    if b"\r" in contents:
        errors.append(f"{relative}: use LF line endings")
    if contents and not contents.endswith(b"\n"):
        errors.append(f"{relative}: missing final newline")
    if path.suffix != ".md":
        for number, line in enumerate(text.splitlines(), 1):
            if line.rstrip(" \t") != line:
                errors.append(f"{relative}:{number}: trailing whitespace")
    if path.suffix == ".py":
        try:
            compile(text, relative, "exec", dont_inherit=True)
        except SyntaxError as error:
            errors.append(f"{relative}:{error.lineno}: {error.msg}")
    return errors


def check_ownership_change(root, tracked, owned, base_ref):
    """Do not hide previously owned source with an exclusion or a rename."""
    previous = blob(root, base_ref, ".clang-format-dirs")
    if previous is None:
        raise ValueError("Trusted base has no source ownership manifest")
    includes, excludes, extensions = formatting.parse_format_text(
        previous.decode("utf-8"), f"{base_ref}:.clang-format-dirs",
    )

    def was_owned(name):
        path = root / name
        return (
            path.suffix in extensions
            and any(path.is_relative_to(root / directory) for directory in includes)
            and not formatting.should_exclude(path, root, excludes)
        )

    old_names = {
        item.decode("utf-8") for item in git(
            root, "ls-tree", "-r", "--name-only", "-z", base_ref,
        ).split(b"\0") if item
    }
    for name in old_names & tracked:
        if was_owned(name) and name not in owned:
            raise ValueError(f"Previously owned source cannot be excluded: {name}")
    entries = iter(git(
        root, "diff", "--find-renames", "--name-status", "-z", base_ref, "--",
    ).split(b"\0")[:-1])
    for status in entries:
        old_name = next(entries).decode("utf-8")
        if status.startswith((b"R", b"C")):
            new_name = next(entries).decode("utf-8")
            if was_owned(old_name) and new_name not in owned:
                raise ValueError(f"Owned source cannot move outside ownership: {old_name} -> {new_name}")


def run_checks(root, filenames, *, all_files=False, base_ref="HEAD"):
    if base_ref == "0" * 40:
        # A new remote ref has no previous commit. Its only reviewed debt source
        # is the fixed pre-automation snapshot, not a candidate's parent.
        base_ref = BOOTSTRAP_SOURCE
    git(root, "rev-parse", "--verify", f"{base_ref}^{{commit}}")
    versions = locked_versions(root)
    baseline = validate_baseline(
        load_json((root / BASELINE_PATH).read_bytes()), versions["clang-format"],
    )
    debt_base = check_baseline_change(root, baseline, base_ref)
    tool, version = formatting.check_tool(formatting.default_tool(root), root / ".clang-format")
    if not re.search(rf"\bversion {re.escape(versions['clang-format'])}\b", version):
        raise ValueError(f"Expected clang-format {versions['clang-format']}: {version.strip()}")
    tracked = {
        item.decode("utf-8") for item in git(root, "ls-files", "-z").split(b"\0") if item
    }
    names = set(filenames)
    if not names <= tracked:
        raise ValueError(f"Files are not in the Git index: {sorted(names - tracked)}")
    owned_paths = formatting.find_source_files(root, root / ".clang-format-dirs")
    owned = {path.relative_to(root).as_posix(): path for path in owned_paths}
    if not owned:
        raise ValueError("Full owned source set is empty")
    check_ownership_change(root, tracked, owned, base_ref)
    changed = {
        item.decode("utf-8") for item in git(
            root, "diff", "--name-only", "-z", base_ref, "--",
        ).split(b"\0") if item
    }
    full = all_files or bool((names | changed) & CONTROL_FILES)
    selected = set(owned) if full else names & set(owned)
    includes, excludes, extensions = formatting.parse_format_config(root / ".clang-format-dirs")
    for name in (tracked if full else names):
        path = root / name
        if (
            path.suffix in SOURCE_SUFFIXES and name not in owned
            and not formatting.should_exclude(path, root, excludes)
        ):
            raise ValueError(f"Unclassified source must be added to the owned manifest: {name}")
    errors = []
    records = []
    for name in sorted(selected):
        path = owned[name]
        contents = path.read_bytes()
        source_hash = digest(contents)
        base_contents = blob(root, debt_base, name)
        unchanged = base_contents is not None and digest(base_contents) == source_hash
        # Checking every file still validates the formatter/configuration actually run.
        formatted = formatting.formatted_source(path, tool, root / ".clang-format")
        if isinstance(formatted, str):
            formatted = formatted.encode("utf-8")
        format_debt = formatted != contents
        format_allowed = (
            format_debt and unchanged and baseline["format"].get(name) == source_hash
        )
        if format_debt and not format_allowed:
            errors.append(f"{name}: run python scripts/tools/format.py --files {name}")
        facts = comment_style.inspect_source(name, contents)
        allowed_comments = baseline["comments"] if unchanged else None
        comment_result = comment_style.apply_baseline(name, facts, allowed_comments)
        for issue in comment_result["failures"]:
            if isinstance(issue, dict):
                errors.append(f"{name}:{issue.get('line', 1)}: {issue.get('rule')}: {issue.get('message', '')}")
            else:
                errors.append(f"{name}: {issue}")
        records.append({
            "path": name, "source_sha256": source_hash,
            "legacy_format_debt": format_allowed,
            "comment_rules": facts["rules"],
            "comment_status": comment_result["baseline_status"],
        })
    # Full CI source checks do not relabel unrelated historical text as new debt.
    text_names = (changed & tracked) if full else names
    for name in sorted(text_names):
        path = root / name
        text_exclusions = [pattern for pattern in excludes if pattern not in {"docs", "scripts"}]
        if formatting.should_exclude(path, root, text_exclusions):
            continue
        if path.suffix in TEXT_SUFFIXES or name in CONTROL_FILES or name in {"AGENTS.md", ".gitignore"}:
            errors.extend(check_text(path, name))
    return {
        "schema_version": 1, "status": "failed" if errors else "passed",
        "base_ref": base_ref, "debt_source_ref": debt_base, "full_source_check": full,
        "source_files_checked": len(selected), "text_files_selected": len(text_names),
        "legacy_format_files": sum(item["legacy_format_debt"] for item in records),
        "legacy_comment_files": sum(bool(item["comment_rules"]) for item in records),
        "tool_version": version.strip(), "records": records, "errors": errors,
        "limits": [
            "Mechanical comment rules do not prove API contract semantics.",
            "Unchanged sealed debt is reported, not declared style-conformant.",
        ],
    }


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*")
    parser.add_argument("--all", action="store_true", dest="all_files")
    parser.add_argument("--base-ref", default=os.environ.get("NEXUS_STYLE_BASE_REF", "HEAD"))
    parser.add_argument("--report", type=Path)
    args = parser.parse_args(arguments)
    try:
        result = run_checks(
            ROOT, args.files,
            all_files=args.all_files or os.environ.get("NEXUS_STYLE_FULL") == "1",
            base_ref=args.base_ref,
        )
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(
            f"Nexus style: {result['status']}; checked {result['source_files_checked']} source files; "
            f"unchanged legacy format/comment files: "
            f"{result['legacy_format_files']}/{result['legacy_comment_files']}"
        )
        for error in result["errors"][:60]:
            print(error, file=sys.stderr)
        if len(result["errors"]) > 60:
            print(f"{len(result['errors']) - 60} further errors; use --report for the full record", file=sys.stderr)
        return int(bool(result["errors"]))
    except (
        OSError, UnicodeError, ValueError, subprocess.SubprocessError,
        formatting.FormatError, install_dev_tools.InstallError,
    ) as error:
        if args.report:
            try:
                args.report.parent.mkdir(parents=True, exist_ok=True)
                args.report.write_text(json.dumps({
                    "schema_version": 1, "status": "failed",
                    "base_ref": args.base_ref, "errors": [str(error)],
                }, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            except OSError as report_error:
                print(f"Cannot write failure report: {report_error}", file=sys.stderr)
        print(f"Style check failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
