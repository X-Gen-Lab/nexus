#!/usr/bin/env python3
"""Check mechanical owned C/C++ comment rules; never infer API correctness.

Selection comes from the formatter's shared .clang-format-dirs implementation.
A sealed baseline permits debt only when the entire source file is unchanged.
This tool does not create or update that baseline.
"""

from __future__ import annotations

import argparse
from bisect import bisect_left, bisect_right
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys


SCHEMA_VERSION = 1
RULESET_VERSION = 1
RULES = frozenset({
    "line_comment",
    "at_doxygen_tag",
    "equals_separator",
    "file_header_missing",
    "file_header_field_missing",
    "source_api_documentation",
    "lexical_error",
})
HEADER_SUFFIXES = frozenset({".h", ".hh", ".hpp", ".hxx"})
SOURCE_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx"})
FRAGMENT_SUFFIXES = frozenset({".inc"})
HEADER_FIELDS = ("file", "brief", "author")
SOURCE_FIELDS = HEADER_FIELDS + ("version", "date", "copyright")
RAW_STRING = re.compile(r'(?<!\w)(?:u8|u|U|L)?R"([^ ()\\\t\v\f\r\n]{0,16})\(')
AT_TAG = re.compile(r"(?<![\w@])@(?:[A-Za-z][A-Za-z0-9_]*|[{}])")
BACKSLASH_TAG = re.compile(r"(?<!\\)\\([A-Za-z][A-Za-z0-9_]*)")
SOURCE_API_TAGS = frozenset({"param", "return", "retval"})
LIMITATIONS = [
    "Mechanical comment syntax and header fields only; no authorship verification.",
    "No proof of ownership, ISR safety, deadlines, API coverage or contract correctness.",
    "Baseline authenticity and changes require the separate review/CI policy.",
]


class StyleError(ValueError):
    """Invalid input or checker configuration."""


@dataclass(frozen=True)
class Issue:
    rule: str
    line: int
    column: int
    message: str


@dataclass(frozen=True)
class Comment:
    kind: str
    text: str
    offsets: tuple[int, ...]
    leading: bool

    @property
    def doxygen(self) -> bool:
        return self.kind == "block" and self.text.startswith(("/**", "/*!"))


def _splice(source: str) -> tuple[str, list[int]]:
    """Apply line splicing while retaining physical positions for diagnostics."""
    text = []
    offsets = []
    index = 0
    while index < len(source):
        if source.startswith("\\\r\n", index):
            index += 3
        elif source.startswith("\\\n", index):
            index += 2
        else:
            text.append(source[index])
            offsets.append(index)
            index += 1
    return "".join(text), offsets


def _digit_separator(text: str, index: int) -> bool:
    if index == 0 or index + 1 >= len(text):
        return False
    if not text[index - 1].isalnum() or not text[index + 1].isalnum():
        return False
    start = index - 1
    while start > 0 and (text[start - 1].isalnum() or text[start - 1] in "._'"):
        start -= 1
    token = text[start:index]
    return token[0].isdigit() or (
        token.startswith(".") and len(token) > 1 and token[1].isdigit()
    )


def lex_comments(source: str) -> tuple[list[Comment], list[tuple[int, str]]]:
    """Identify comments without treating literal contents as C/C++ comments.

    Raw string bodies use physical source, where line splicing is not applied.
    Other tokens use spliced text, so continued line comments and split comment
    delimiters have the same boundaries as the compiler's preprocessing phase.
    """
    text, offsets = _splice(source)
    comments = []
    errors = []
    index = 0
    leading = True
    while index < len(text):
        if text[index].isspace():
            index += 1
            continue
        if text.startswith("//", index):
            end = text.find("\n", index + 2)
            if end < 0:
                end = len(text)
            comments.append(Comment("line", text[index:end],
                                    tuple(offsets[index:end]), leading))
            index = end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            if end < 0:
                errors.append((offsets[index], "Unterminated block comment"))
                end = len(text)
            else:
                end += 2
            comments.append(Comment("block", text[index:end],
                                    tuple(offsets[index:end]), leading))
            index = end
            continue
        raw = RAW_STRING.match(text, index)
        if raw:
            leading = False
            terminator = ")" + raw.group(1) + '"'
            physical_start = offsets[raw.end() - 1] + 1
            physical_end = source.find(terminator, physical_start)
            if physical_end < 0:
                errors.append((offsets[index], "Unterminated raw string"))
                break
            index = bisect_left(offsets, physical_end + len(terminator))
            continue
        quote = text[index]
        if quote == "'" and _digit_separator(text, index):
            leading = False
            index += 1
            continue
        if quote in ('"', "'"):
            leading = False
            start = index
            index += 1
            closed = False
            while index < len(text):
                if text[index] == "\\":
                    index += 2
                elif text[index] == quote:
                    index += 1
                    closed = True
                    break
                elif text[index] in "\r\n":
                    break
                else:
                    index += 1
            if not closed:
                errors.append((offsets[start], "Unterminated string/character literal"))
            continue
        leading = False
        index += 1
    return comments, errors


def _location(line_starts: list[int], offset: int) -> tuple[int, int]:
    line = bisect_right(line_starts, offset)
    return line, offset - line_starts[line - 1] + 1


def inspect_source(path: str, contents: bytes) -> dict:
    """Return raw facts for one source, including debt that a baseline may allow."""
    suffix = PurePosixPath(path).suffix.lower()
    if suffix not in HEADER_SUFFIXES | SOURCE_SUFFIXES | FRAGMENT_SUFFIXES:
        raise StyleError(f"Unsupported C/C++ extension: {path}")
    digest = hashlib.sha256(contents).hexdigest()
    diagnostics = []
    try:
        source = contents.decode("utf-8-sig")
    except UnicodeDecodeError as error:
        return {
            "source_sha256": digest,
            "rules": {"lexical_error": 1},
            "diagnostics": [{
                "rule": "lexical_error", "line": 1, "column": 1,
                "message": f"Source must be UTF-8: {error.reason}",
            }],
        }
    line_starts = [0] + [
        index + 1 for index, char in enumerate(source) if char == "\n"
    ]

    def add(rule: str, offset: int, message: str) -> None:
        line, column = _location(line_starts, offset)
        diagnostics.append({
            "rule": rule, "line": line, "column": column, "message": message,
        })

    comments, lexical_errors = lex_comments(source)
    for offset, message in lexical_errors:
        add("lexical_error", offset, message)
    for comment in comments:
        if comment.kind == "line":
            add("line_comment", comment.offsets[0], "Use /* ... */ instead of //")
        for tag in AT_TAG.finditer(comment.text):
            add("at_doxygen_tag", comment.offsets[tag.start()],
                f"Use backslash Doxygen syntax instead of {tag.group()}")
        for line in re.finditer(r"(?:^|\n)[ \t]*(?:/\*+|//+|\*)?[ \t]*(={3,})",
                                comment.text):
            add("equals_separator", comment.offsets[line.start(1)],
                "Use the established hyphen section separator")
        if suffix in SOURCE_SUFFIXES and comment.doxygen:
            for tag in BACKSLASH_TAG.finditer(comment.text):
                if tag.group(1) in SOURCE_API_TAGS:
                    add("source_api_documentation", comment.offsets[tag.start()],
                        f"Keep \\{tag.group(1)} API documentation in the header")

    headers = [comment for comment in comments if comment.leading and comment.doxygen]
    if not headers:
        add("file_header_missing", 0, "Add the standard leading Doxygen file header")
    else:
        header = next((comment for comment in headers if any(
            tag.group(1) == "file" for tag in BACKSLASH_TAG.finditer(comment.text)
        )), headers[0])
        fields = SOURCE_FIELDS if suffix in SOURCE_SUFFIXES else HEADER_FIELDS
        present = {
            tag.group(1) for tag in BACKSLASH_TAG.finditer(header.text)
        }
        for field in fields:
            if field not in present:
                add("file_header_field_missing", header.offsets[0],
                    f"File header is missing \\{field}")
    diagnostics.sort(key=lambda item: (item["line"], item["column"], item["rule"]))
    return {
        "source_sha256": digest,
        "rules": dict(sorted(Counter(item["rule"] for item in diagnostics).items())),
        "diagnostics": diagnostics,
    }


def analyze_source(text: str, relative_path: str) -> list[Issue]:
    """Public in-memory API for the staged-content gate.

    The caller selects owned files and hashes original bytes for the baseline.
    For exact byte hashes and rule counts, inspect_source(path, bytes) returns
    the same diagnostics along with source_sha256 and rules.
    """
    return [
        Issue(**item) for item in inspect_source(relative_path, text.encode("utf-8"))[
            "diagnostics"
        ]
    ]


def _json_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise StyleError(f"Duplicate baseline key: {key}")
        result[key] = value
    return result


def validate_baseline(data: object) -> dict:
    if not isinstance(data, dict):
        raise StyleError("Baseline must be a JSON object")
    if set(data) != {"schema_version", "ruleset_version", "files"}:
        raise StyleError("Invalid baseline fields")
    if type(data.get("schema_version")) is not int or (
            data["schema_version"] != SCHEMA_VERSION):
        raise StyleError("Unsupported baseline schema_version")
    if type(data.get("ruleset_version")) is not int or (
            data["ruleset_version"] != RULESET_VERSION):
        raise StyleError("Unsupported baseline ruleset_version")
    files = data.get("files")
    if not isinstance(files, dict):
        raise StyleError("Baseline files must be an object")
    for name, entry in files.items():
        if (not isinstance(name, str) or not name or "\\" in name or
                PurePosixPath(name).is_absolute() or
                any(part in ("", ".", "..") for part in name.split("/"))):
            raise StyleError(f"Baseline path is not canonical repo-relative: {name}")
        if not isinstance(entry, dict) or set(entry) != {"source_sha256", "rules"}:
            raise StyleError(f"Invalid baseline file entry: {name}")
        if not isinstance(entry["source_sha256"], str) or not re.fullmatch(
                r"[0-9a-f]{64}", entry["source_sha256"]):
            raise StyleError(f"Invalid baseline source_sha256: {name}")
        rules = entry["rules"]
        if not isinstance(rules, dict):
            raise StyleError(f"Invalid baseline rule counts: {name}")
        for rule, count in rules.items():
            if rule not in RULES or isinstance(count, bool) or not isinstance(
                    count, int) or count <= 0:
                raise StyleError(f"Invalid baseline rule/count: {name}: {rule}")
    return data


def load_baseline(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_json_object)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise StyleError(f"Cannot read baseline {path}: {error}") from error
    return validate_baseline(data)


def apply_baseline(path: str, facts: dict, baseline: dict | None) -> dict:
    entry = baseline["files"].get(path) if baseline is not None else None
    unchanged = bool(entry and entry["source_sha256"] == facts["source_sha256"])
    allowance = entry["rules"] if unchanged else {}
    blocked = {
        rule for rule, count in facts["rules"].items()
        if count > allowance.get(rule, 0)
    }
    return {
        **facts,
        "baseline_status": ("unchanged_frozen_debt" if unchanged else
                            "changed_strict" if entry else "new_or_unlisted_strict"),
        "failures": [item for item in facts["diagnostics"] if item["rule"] in blocked],
    }


def check_files(root: Path, paths: list[Path], baseline: dict | None = None) -> dict:
    root = root.resolve()
    if not paths:
        raise StyleError("Cannot pass an empty C/C++ comment-check scope")
    if baseline is not None:
        validate_baseline(baseline)
    files = {}
    for path in sorted(set(paths)):
        path = path if path.is_absolute() else root / path
        try:
            relative = path.relative_to(root).as_posix()
            if path.resolve(strict=True) != path or not path.is_file():
                raise StyleError(f"Source must be a regular, non-symlink file: {path}")
            files[relative] = apply_baseline(
                relative, inspect_source(relative, path.read_bytes()), baseline
            )
        except (ValueError, OSError) as error:
            raise StyleError(f"Cannot check source {path}: {error}") from error
    failed = sum(len(entry["failures"]) for entry in files.values())
    return {
        "schema_version": SCHEMA_VERSION,
        "ruleset_version": RULESET_VERSION,
        "status": "failed" if failed else "passed",
        "checked_files": len(files),
        "failing_diagnostics": failed,
        "files": files,
        "limitations": LIMITATIONS,
    }


def _select(root: Path, explicit: list[str] | None) -> list[Path]:
    # The same selector used by formatting owns the directory/exclusion policy.
    repository = Path(__file__).resolve().parents[2]
    if str(repository) not in sys.path:
        sys.path.insert(0, str(repository))
    from scripts.tools import format as formatter
    try:
        paths = [Path(path) if Path(path).is_absolute() else root / path
                 for path in explicit] if explicit is not None else None
        return formatter.find_source_files(root, explicit_files=paths)
    except formatter.FormatError as error:
        raise StyleError(str(error)) from error


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    scope = parser.add_mutually_exclusive_group(required=True)
    scope.add_argument("--all", action="store_true", help="Check all selected owned sources")
    scope.add_argument("--files", nargs="+", help="Check explicit selected owned sources")
    parser.add_argument("--baseline", type=Path, help="Read reviewed frozen debt; never update it")
    parser.add_argument("--json", action="store_true", help="Emit facts and results as JSON")
    args = parser.parse_args(argv)
    try:
        root = args.root.resolve(strict=True)
        if not root.is_dir():
            raise StyleError("Root must be a directory")
        baseline_path = args.baseline
        if baseline_path is not None and not baseline_path.is_absolute():
            baseline_path = root / baseline_path
        baseline = load_baseline(baseline_path) if baseline_path else None
        result = check_files(root, _select(root, args.files), baseline)
    except (StyleError, OSError) as error:
        parser.print_usage(sys.stderr)
        print(f"{parser.prog}: error: {error}", file=sys.stderr)
        raise SystemExit(2) from error
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        for path, entry in result["files"].items():
            for item in entry["failures"]:
                print(f"{path}:{item['line']}:{item['column']}: "
                      f"{item['rule']}: {item['message']}")
        debt_files = sum(entry["baseline_status"] == "unchanged_frozen_debt"
                         and bool(entry["diagnostics"]) for entry in result["files"].values())
        print(f"Comment style {result['status']}: {result['checked_files']} files, "
              f"{result['failing_diagnostics']} failing diagnostics, "
              f"{debt_files} unchanged files with frozen debt")
    return 1 if result["status"] == "failed" else 0


if __name__ == "__main__":
    raise SystemExit(main())
