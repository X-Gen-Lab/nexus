#!/usr/bin/env python3
"""One documentation entry point for checks, strict builds and verified delivery."""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.dev import process
from tools.documentation import check
from tools.evidence.identity import observe_git_source

OWNER = ".nexus-documentation-output.json"


def prepare_output(root, destination):
    """Only reuse a directory whose ownership was explicitly established."""
    root, destination = Path(root).resolve(), Path(destination).absolute()
    if destination.is_symlink() or any(path.is_symlink() for path in destination.parents):
        raise ValueError("documentation output cannot use a symlink")
    destination = destination.resolve()
    if (destination == root or root.is_relative_to(destination)
            or destination.is_relative_to(root / "docs")
            or (destination.is_relative_to(root) and
                destination.relative_to(root).parts[0] != "build")):
        raise ValueError("documentation output cannot overlap source inputs")
    if any(path.is_relative_to(destination) for path in
           check.dependencies(root, require_files=False)):
        raise ValueError("documentation output cannot remove rendered inputs")
    marker = destination / OWNER
    if destination.exists():
        if not marker.is_file() or marker.is_symlink():
            raise ValueError("existing output lacks documentation ownership")
        record = json.loads(marker.read_text())
        if record != {"schema_version": 1, "owner": "nexus-documentation", "root": str(root)}:
            raise ValueError("output ownership differs from this checkout")
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    marker.write_text(json.dumps({"schema_version": 1,
        "owner": "nexus-documentation", "root": str(root)}) + "\n")
    return destination


def _record(path, record):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    temporary.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n")
    temporary.replace(path)


def _staged_sources(root):
    """A repair outside the index cannot validate the committed document."""
    tree = subprocess.run(["git", "write-tree"], cwd=root, check=True,
                          capture_output=True, text=True, timeout=30).stdout.strip()
    def relevant(path):
        name = path.relative_to(root).as_posix()
        if name.startswith("docs/"):
            parts = path.relative_to(root / "docs").parts
            return (parts[0] != "api"
                    and not set(parts).intersection(check.EXCLUDED - {"locale"})
                    and (parts[0] == "assets" or path.suffix in {
                        ".md", ".rst", ".dot", ".svg", ".csv", ".json",
                        ".toml", ".po", ".py", ".html", ".css", ".js"}))
        return (name == "Doxyfile" or name.startswith("dependencies/docs-")
                or name.startswith(("tools/documentation/", "tools/configure/"))
                or (name.startswith(("boards/", "soc/")) and path.suffix == ".json"))
    indexed_names = subprocess.run(["git", "ls-files", "-z", "--", "docs/", "Doxyfile",
                                   "dependencies/docs-*", "tools/documentation/",
                                   "tools/configure/", "boards/", "soc/"],
                                   cwd=root, check=True, capture_output=True,
                                   timeout=30).stdout.split(b"\0")
    indexed_paths = {root / os.fsdecode(name) for name in indexed_names if name}
    candidates = {path for path in indexed_paths if relevant(path)}
    for directory in ("docs", "tools/documentation", "tools/configure", "boards", "soc"):
        candidates.update(path for path in (root / directory).rglob("*")
                          if path.is_file() and relevant(path)
                          and "__pycache__" not in path.parts)
    candidates.update(check.dependencies(root))
    for path in sorted(candidates):
        relative = path.relative_to(root).as_posix()
        indexed = subprocess.run(["git", "show", ":" + relative], cwd=root,
                                 capture_output=True, timeout=30)
        if not path.is_file() or indexed.returncode or indexed.stdout != path.read_bytes():
            raise ValueError("document differs from staged index: " + relative)
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


def normalize_generated_svg(folder):
    """Drop a trusted generator's external DTD without changing SVG elements."""
    ET.register_namespace("", "http://www.w3.org/2000/svg")
    ET.register_namespace("xlink", "http://www.w3.org/1999/xlink")
    for path in Path(folder).rglob("*.svg"):
        raw = path.read_bytes()
        if b"<!ENTITY" in raw.upper():
            raise ValueError("generated SVG contains entity definitions")
        tree = ET.fromstring(raw)
        path.write_bytes(ET.tostring(tree, encoding="utf-8", xml_declaration=True) + b"\n")


def _execute(command, root, environment, workspace, stage, timeout):
    result = process.run(command, cwd=root, env=environment,
                         log=workspace / (stage + ".log"), timeout=timeout,
                         stage="documentation-" + stage)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    if result.returncode:
        raise ValueError(f"{stage} failed with exit {result.returncode}; "
                         f"receipt: {result.receipt_path}")
    return str(result.receipt_path)


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


def _check_status_views(root, report):
    """Admit maintained views from their source, even when output is missing."""
    from tools.documentation import status
    page = root / "docs/index.md"
    current = (page.is_file()
               and check.metadata(page.read_text()).get("id") == "index")
    inputs = [root / "docs/status/ledgers"] + [
        root / "docs/design" / filename
        for filename, _, _ in status.SPECIFICATIONS]
    if current or any(path.exists() or path.is_symlink() for path in inputs):
        for result in (status.generate(root, check=True),
                       status.check_mapping(root)):
            report["errors"].extend(result["errors"])
        report["status"] = "failed" if report["errors"] else "passed"


def build(root, destination, *, language="all", tool_prefix=None, timeout=600):
    """Build fresh XML and HTML; no previous success can survive a failure."""
    from tools.documentation import install
    root = Path(root).resolve()
    output = prepare_output(root, destination)
    workspace_parent = output.parent / "documentation-attempts"
    if any(path.is_symlink() for path in
           (workspace_parent, *workspace_parent.parents)):
        raise ValueError("documentation workspace cannot use a symlink")
    workspace = workspace_parent / uuid.uuid4().hex
    workspace.mkdir(parents=True)
    environment = install.environment(tool_prefix)
    observed_tools = install.diagnose(tool_prefix)
    prefix = Path(observed_tools["prefix"])
    python = prefix / "venv/bin/python"
    source = observe_git_source(root)
    languages = ["en", "zh_CN"] if language == "all" else [language]
    receipts = []
    metadata = {"schema_version": 1, "kind": "nexus_documentation_build",
                "status": "running", "source": source,
                "tools": observed_tools, "languages": languages,
                "qualification": False}
    try:
        report = check.scan(root)
        _check_status_views(root, report)
        _record(workspace / "source-check.json", report)
        if report["errors"]:
            raise ValueError("documentation source check failed: " + "\n".join(report["errors"]))
        from tools.documentation import diagrams
        diagram_report = diagrams.check(root, env=environment, full=True)
        if diagram_report["errors"]:
            raise ValueError("diagram verification failed: " + "\n".join(diagram_report["errors"]))
        policy = json.loads((root / "dependencies/platform-compatibility.json").read_text())
        api_work = workspace / "api"
        doxygen = workspace / "Doxyfile"
        def quoted(path):
            return '"' + str(path).replace("\\", "\\\\").replace('"', '\\"') + '"'
        doxygen.write_text('@INCLUDE = ' + quoted(root / "Doxyfile") + '\n'
                           + 'OUTPUT_DIRECTORY = ' + quoted(api_work) + '\n'
                           + 'PROJECT_NUMBER = "' + policy["semantic_version"] + '"\n')
        receipts.append(_execute([str(prefix / "bin/doxygen"), str(doxygen)],
                                 root, environment, workspace, "doxygen", timeout))
        environment.update({"NEXUS_DOCS_REVISION": source["commit"],
                            "NEXUS_DOCS_CODE_REVISION": source["commit"],
                            "NEXUS_DOCS_VERSION": policy["semantic_version"],
                            "NEXUS_DOCS_LANGUAGES": ",".join(languages),
                            "NEXUS_DOCS_LANGUAGE": languages[0],
                            "NEXUS_DOCS_DOXYGEN_XML": str(api_work / "xml"),
                            "NEXUS_DOCS_GETTEXT_ROOT": str(workspace / "gettext")})
        base = [str(python), "-m", "sphinx", "-n", "-W", "--keep-going",
                "-c", str(root / "docs/sphinx"), "-d", str(workspace / "doctrees")]
        receipts.append(_execute(base + ["-b", "gettext", str(root / "docs"),
                                  str(workspace / "gettext")], root, environment,
                                 workspace, "gettext", timeout))
        translation_report = check.validate_translations(root,
            pot_root=workspace / "gettext", languages=languages)
        if translation_report["errors"]:
            raise ValueError("translation gate failed: " + "\n".join(translation_report["errors"]))
        metadata["translations"] = translation_report
        for selected in languages:
            environment["NEXUS_DOCS_LANGUAGE"] = selected
            command = base + ["-E", "-b", "html", "-D", "language=" + selected,
                              str(root / "docs"), str(output / selected)]
            receipts.append(_execute(command, root, environment, workspace,
                                     "html-" + selected, timeout))
        shutil.copytree(api_work / "html", output / "api")
        normalize_generated_svg(output / "api")
        choices = ' · '.join('<a href="' + item + '/index.html">' +
                             ('English' if item == 'en' else '中文') + '</a>'
                             for item in languages)
        (output / "index.html").write_text('<!doctype html><html lang="en"><meta charset="utf-8">'
            '<title>Nexus documentation</title><h1>Nexus documentation</h1>'
            '<p>' + choices + ' · <a href="api/index.html">Public C API</a></p>'
            '<p>Source ' + source["commit"] + '. Physical HIL: not executed.</p></html>')
        licenses = output / "licenses"
        licenses.mkdir()
        shutil.copyfile(root / "LICENSE", licenses / "LICENSE.txt")
        shutil.copytree(prefix / "licenses", licenses / "documentation-tools")
        shutil.copytree(root / "docs/assets/diagrams", output / "sources/diagrams")
        if languages == ["en", "zh_CN"]:
            from tools.documentation import delivery
            delivery.install_alias_catalog(output, root)
        after = observe_git_source(root)
        if after != source:
            raise ValueError("source identity changed during documentation build")
        metadata.update({"status": "passed", "command_receipts": receipts,
                         "source_check": report, "workspace": str(workspace)})
        _record(output / "documentation-build.json", metadata)
        return metadata
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        metadata.update({"status": "failed", "error": str(error), "command_receipts": receipts})
        _record(workspace / "documentation-build-failure.json", metadata)
        raise


def main(arguments=None, *, root=ROOT):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    for name in ("check", "build", "doctor", "package", "preview", "verify", "restore", "test"):
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
            command.add_argument("--timeout", type=float, default=600)
        elif name == "package":
            command.add_argument("--build", type=Path, default=Path(root) / "build/documentation")
            command.add_argument("--output", type=Path,
                default=Path(root).parent / ".nexus-documentation-artifacts" / Path(root).name / "packages")
            command.add_argument("--release-id")
        elif name == "preview":
            command.add_argument("--build", type=Path, default=Path(root) / "build/documentation")
            command.add_argument("--host", default="127.0.0.1")
            command.add_argument("--port", type=int, default=8000)
        elif name in {"verify", "restore"}:
            command.add_argument("path", type=Path)
            if name == "restore":
                command.add_argument("--output", type=Path, required=True)
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
            _check_status_views(root, report)
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
            build(root, args.output, language=args.language,
                  tool_prefix=args.tool_prefix, timeout=args.timeout)
        elif args.action == "package":
            from tools.documentation import delivery
            revision = observe_git_source(root)["commit"]
            release_id = args.release_id or "docs-" + revision[:12]
            print(json.dumps(delivery.package(args.build, args.output,
                source_root=root, release_id=release_id), indent=2))
        elif args.action in {"verify", "restore"}:
            from tools.documentation import delivery
            record = (delivery.verify(args.path) if args.action == "verify" else
                      delivery.restore(args.path, args.output))
            print(json.dumps(record, indent=2))
        elif args.action == "preview":
            if not (args.build / "documentation-build.json").is_file():
                raise ValueError("preview requires a completed documentation build")
            if json.loads((args.build / "documentation-build.json").read_text()).get("status") != "passed":
                raise ValueError("preview requires a successful documentation build")
            handler = partial(SimpleHTTPRequestHandler, directory=str(args.build))
            server = ThreadingHTTPServer((args.host, args.port), handler)
            print(f"Preview: http://{args.host}:{server.server_port}", flush=True)
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                pass
            finally:
                server.server_close()
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        if failure_report is not None:
            try:
                _record(failure_report, {
                    "schema_version": 1,
                    "kind": "nexus_documentation_source_check",
                    "status": "failed", "errors": [str(error)],
                    "admission_completed": False, "qualification": False})
            except OSError as report_error:
                print("Failure report could not be written: "
                      + str(report_error), file=sys.stderr)
        print("Documentation command rejected: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
