#!/usr/bin/env python3
"""One documentation entry point for checks, strict builds and verified delivery."""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.dev import process
from tools.documentation import check
from tools.documentation import build as builder
from tools.documentation.build import prepare_output, record as _record, build

def _staged_sources(root):
    """A repair outside the index cannot validate the committed document."""
    tree = subprocess.run(["git", "write-tree"], cwd=root, check=True,
                          capture_output=True, text=True, timeout=30).stdout.strip()
    def relevant(path):
        name = path.relative_to(root).as_posix()
        if check.is_adjacent_source(root, path):
            return True
        if name.startswith("docs/"):
            parts = path.relative_to(root / "docs").parts
            return (not set(parts).intersection(check.EXCLUDED - {"locale"})
                    and (parts[0] == "assets" or path.suffix in {
                        ".md", ".rst", ".dot", ".svg", ".csv", ".json",
                        ".toml", ".po", ".py", ".html", ".css", ".js"}))
        return (name in {"Doxyfile", "tools/__init__.py"}
                or name.startswith("dependencies/docs-")
                or name.startswith(("tools/documentation/", "tools/configure/"))
                or (name.startswith("boards/") and path.suffix == ".json")
                or (name.startswith("soc/") and path.suffix == ".json"))
    indexed_names = subprocess.run(["git", "ls-files", "-z", "--", "docs/", "Doxyfile",
                                   "dependencies/docs-*", "tools/__init__.py",
                                   "tools/documentation/",
                                   "tools/configure/", *check.OWNED_MARKDOWN_ROOTS,
                                   *check.ROOT_MARKDOWN],
                                   cwd=root, check=True, capture_output=True,
                                   timeout=30).stdout.split(b"\0")
    indexed_paths = {root / os.fsdecode(name) for name in indexed_names if name}
    candidates = {path for path in indexed_paths if relevant(path)}
    if (root / "tools/__init__.py").is_file():
        candidates.add(root / "tools/__init__.py")
    for directory in ("docs", "tools/documentation", "tools/configure", "boards", "soc"):
        candidates.update(path for path in (root / directory).rglob("*")
                          if path.is_file() and relevant(path)
                          and "__pycache__" not in path.parts)
    candidates.update(check.dependencies(root))
    candidates.update(check.adjacent_sources(root))
    candidates.update(root / name for name in builder.inputs(root))
    raw = subprocess.run(["git", "ls-files", "--stage", "-z"], cwd=root,
                         capture_output=True, check=True, timeout=30).stdout
    indexed = {}
    for entry in raw.split(b"\0"):
        if entry:
            fields, name = entry.split(b"\t", 1)
            _mode, object_id, stage = fields.split()
            if stage != b"0":
                raise ValueError("unmerged documentation input")
            indexed[os.fsdecode(name)] = object_id
    names = sorted(path.relative_to(root).as_posix() for path in candidates)
    identities = list(dict.fromkeys(indexed[name] for name in names if name in indexed))
    payload = subprocess.run(["git", "cat-file", "--batch"], cwd=root,
        input=b"".join(value + b"\n" for value in identities),
        capture_output=True, check=True, timeout=30).stdout
    blobs, position = {}, 0
    for expected in identities:
        finish = payload.find(b"\n", position)
        if finish < 0:
            raise ValueError("incomplete Git documentation batch")
        header = payload[position:finish].split()
        if len(header) != 3 or header[:2] != [expected, b"blob"]:
            raise ValueError("invalid Git documentation object")
        length = int(header[2]); position = finish + 1
        blobs[expected] = payload[position:position + length]
        position += length
        if payload[position:position + 1] != b"\n":
            raise ValueError("incomplete Git documentation blob")
        position += 1
    for name in names:
        path = root / name
        if (not path.is_file() or name not in indexed
                or path.read_bytes() != blobs[indexed[name]]):
            raise ValueError("document differs from staged index: " + name)
    return tree


def _safe_report(root, path):
    path = path.absolute()
    if path.is_symlink() or any(parent.is_symlink() for parent in path.parents):
        raise ValueError("report cannot use a symlink")
    path = path.resolve()
    if path.is_relative_to(root) and not path.is_relative_to(root / "build"):
        raise ValueError("report cannot overwrite source inputs")
    if path.suffix != ".json":
        raise ValueError("documentation report must be JSON")
    if path in check.dependencies(root, require_files=False):
        raise ValueError("report cannot overwrite a rendered document input")
    return path

def run_tests(root, report, tool_prefix=None):
    """Run the complete suite in the verified tuple, without installing it."""
    from tools.documentation import install
    from tools.testing.run_tool_tests import execute_directory
    report.unlink(missing_ok=True)
    environment = install.environment(tool_prefix)
    prefix = Path(environment["NEXUS_DOCS_TOOL_PREFIX"])
    try:
        if Path(sys.prefix).resolve() == (prefix / "venv").resolve():
            record = execute_directory(root / "tools/documentation", "documentation")
        else:
            command = [str(prefix / "venv/bin/python"),
                       str(root / "tools/documentation/cli.py"), "test",
                       "--tool-prefix", str(prefix), "--report", str(report)]
            result = process.run(command, cwd=root, env=environment,
                log=root / "build/documentation-attempts" / uuid.uuid4().hex / "tests.log",
                stage="documentation-tests")
            print(result.stdout, end="")
            print(result.stderr, end="", file=sys.stderr)
            if result.returncode:
                report.unlink(missing_ok=True)
                return result.returncode
            record = json.loads(report.read_text())
        # The existing runner rejects incomplete execution. A child exit alone
        # cannot replace its fresh, nonempty complete-suite result.
        counts = ("discovered", "executed", "skipped", "failures", "errors",
                  "expected_failures", "unexpected_successes")
        if (not isinstance(record, dict) or record.get("suite") != "documentation"
                or record.get("status") != "passed"
                or any(type(record.get(name)) is not int for name in counts)
                or record["discovered"] <= 0
                or record["executed"] != record["discovered"]
                or any(record[name] != 0 for name in counts[2:])):
            raise ValueError("documentation suite lacks a complete passing result")
        _record(report, record)
        print(json.dumps(record, indent=2))
        return 0
    except (OSError, ValueError, subprocess.SubprocessError):
        report.unlink(missing_ok=True)
        raise

def serve_output(root, destination):
    """Serve only the declared site of one current, complete documentation view."""
    destination = Path(destination).absolute()
    if any(path.is_symlink() for path in (destination, *destination.parents)):
        raise ValueError("documentation serve path cannot use a symlink")
    marker = destination / builder.OWNER
    expected = {"schema_version": builder.SCHEMA, "owner": "nexus-documentation",
                "root": str(Path(root).resolve())}
    if not marker.is_file() or marker.is_symlink() or json.loads(marker.read_text()) != expected:
        raise ValueError("serve requires current documentation ownership")
    path = destination / builder.REPORT
    if not path.is_file() or path.is_symlink():
        raise ValueError("serve requires a completed documentation build")
    value = json.loads(path.read_text())
    if not isinstance(value, dict):
        raise ValueError("documentation build report must be an object")
    modes = {"preview": "ready", "verify": "passed"}
    languages = value.get("languages")
    if (value.get("schema_version") != builder.SCHEMA
            or value.get("kind") != "nexus_documentation_build"
            or value.get("mode") not in modes
            or value.get("status") != modes[value["mode"]]
            or not isinstance(languages, list) or not languages
            or len(languages) != len(set(languages))
            or any(language not in {"en", "zh_CN"} for language in languages)
            or value.get("scope") != ("all" if set(languages) == {"en", "zh_CN"}
                                      else "locale:" + languages[0])):
        raise ValueError("serve requires a complete current mode/scope result")
    site = destination / "site"
    if (not (site / "index.html").is_file()
            or builder.inventory(site) != value.get("files")):
        raise ValueError("documentation site differs from its build result")
    return site


def main(arguments=None, *, root=ROOT):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    for name in ("check", "build", "doctor", "serve", "test"):
        command = sub.add_parser(name)
        command.add_argument("--tool-prefix", type=Path)
        if name == "check":
            command.add_argument("--staged", action="store_true")
            command.add_argument("--external", action="store_true")
            command.add_argument("--full", action="store_true")
            command.add_argument("--report", type=Path)
        elif name == "test":
            command.add_argument("--report", type=Path,
                                 default=Path(root) / "build/quality/docs-tests.json")
        elif name == "build":
            command.add_argument("--output", type=Path, default=Path(root) / "build/documentation")
            command.add_argument("--language", choices=("en", "zh_CN", "all"), default="all")
            command.add_argument("--mode", choices=("preview", "verify"), default="verify")
            command.add_argument("--timeout", type=float, default=600)
        elif name == "serve":
            command.add_argument("--build", type=Path, default=Path(root) / "build/documentation")
            command.add_argument("--host", default="127.0.0.1")
            command.add_argument("--port", type=int, default=8000)
    args = parser.parse_args(arguments)
    root = Path(root).resolve()
    failure_report = None
    try:
        if getattr(args, "report", None):
            args.report = _safe_report(root, args.report)
            if args.action == "check":
                failure_report = args.report
                args.report.unlink(missing_ok=True)
        if args.action == "check":
            staged_tree = _staged_sources(root) if args.staged else None
            report = check.scan(root, external=args.external)
            page = root / "docs/index.md"
            if page.is_file() and check.metadata(page.read_text()).get("id"):
                translation_report = check.validate_translations(root)
                report["errors"].extend(translation_report["errors"])
                report["translations"] = translation_report
            if (root / "dependencies/docs-tools.lock.json").is_file():
                from tools.documentation import install
                locked = json.loads((root / "dependencies/docs-tools.lock.json").read_text())
                install.validate_lock(locked)
                install.verify_requirements(root / "dependencies/docs-python-linux-cp312.txt", locked)
            if args.full:
                from tools.documentation import diagrams, install
                diagram_report = diagrams.check(root,
                    env=install.environment(args.tool_prefix), full=True)
                report["errors"].extend(diagram_report["errors"])
                report["diagrams"] = diagram_report
            if staged_tree and _staged_sources(root) != staged_tree:
                raise ValueError("Git index changed during documentation check")
            report["status"] = "failed" if report["errors"] else "passed"
            if args.report:
                _record(args.report, report)
            for error in report["errors"]:
                print(error, file=sys.stderr)
            print(f"Documentation source check: {report['status']}; {report['pages']} pages")
            return 1 if report["errors"] else 0
        if args.action == "doctor":
            from tools.documentation import install
            print(json.dumps(install.diagnose(args.tool_prefix), indent=2))
        elif args.action == "test":
            return run_tests(root, args.report, args.tool_prefix)
        elif args.action == "build":
            build(root, args.output, mode=args.mode, language=args.language,
                  tool_prefix=args.tool_prefix, timeout=args.timeout)
        elif args.action == "serve":
            site = serve_output(root, args.build)
            handler = partial(SimpleHTTPRequestHandler, directory=str(site))
            server = ThreadingHTTPServer((args.host, args.port), handler)
            print(f"Documentation: http://{args.host}:{server.server_port}", flush=True)
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                # Ctrl-C ends preview normally; finally closes the socket.
                pass
            finally:
                server.server_close()
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        if failure_report is not None:
            try:
                record = {
                    "schema_version": 1,
                    "kind": "nexus_documentation_source_check",
                    "status": "failed", "errors": [str(error)],
                    "admission_completed": False, "qualification": False}
                _record(failure_report, record)
            except OSError as report_error:
                print("Failure report could not be written: "
                      + str(report_error), file=sys.stderr)
        print("Documentation command rejected: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
