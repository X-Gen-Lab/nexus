"""Required analyzers: findings, missing tools and zero scope all fail.

Uses actual compile commands; excludes third-party code by source ownership.
Reports preserve exact invocations and exit codes, including execution errors.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

OWNED = {"hal", "osal", "framework", "services", "platforms", "boards", "soc"}


def commands(root: Path, build: Path) -> list[dict]:
    raw = json.loads((build / "compile_commands.json").read_text())
    if not isinstance(raw, list):
        raise ValueError("compilation database must be a list")
    selected = []
    for entry in raw:
        if not isinstance(entry, dict) or not isinstance(entry.get("file"), str):
            raise ValueError("invalid compilation database entry")
        directory = Path(entry.get("directory", "")).resolve()
        source = Path(entry["file"])
        source = (directory / source).resolve()
        try:
            relative = source.relative_to(root)
        except ValueError:
            continue
        if relative.parts and relative.parts[0] in OWNED:
            if not source.is_file():
                raise ValueError(f"missing compilation source: {source}")
            selected.append({**entry, "file": str(source), "directory": str(directory)})
    if not selected:
        raise ValueError("zero owned translation units; analysis cannot pass")
    return selected


def run(kind: str, root: Path, build: Path, tool: str, report: Path) -> int:
    report.parent.mkdir(parents=True, exist_ok=True)
    try:
        selected = commands(root.resolve(), build.resolve())
        with tempfile.TemporaryDirectory(prefix="nexus-analysis-") as temp:
            (Path(temp) / "compile_commands.json").write_text(json.dumps(selected))
            if kind == "tidy":
                invocations = []
                for index, entry in enumerate(selected):
                    # One database per invocation: repeated source files can
                    # have different provider/config definitions. An analyzer
                    # must not silently reuse the first command for every one.
                    directory = Path(temp) / str(index)
                    directory.mkdir()
                    (directory / "compile_commands.json").write_text(json.dumps([entry]))
                    invocations.append([tool, entry["file"], "-p", str(directory),
                     "--checks=-*,clang-analyzer-*,bugprone-*,cert-*",
                     "--warnings-as-errors=*",
                     "--header-filter=" + str(root.resolve()) + "/(hal|osal|framework|services|platforms|boards|soc)/.*"])
            else:
                invocations = [[tool, "--project=" + str(Path(temp) / "compile_commands.json"),
                                "--enable=warning,performance,portability",
                                "--error-exitcode=2", "--xml", "--xml-version=2",
                                "--suppress=missingIncludeSystem"]]
            failed = False
            with report.open("w", encoding="utf-8") as output:
                output.write(f"Owned translation units: {len(selected)}\n")
                output.flush()
                version = subprocess.run([tool, "--version"], cwd=root,
                                         stdout=output, stderr=subprocess.STDOUT,
                                         timeout=30)
                if version.returncode != 0:
                    output.write(f"Version command failed: {version.returncode}\n")
                    return 1
                for argv in invocations:
                    output.write("Invocation: " + json.dumps(argv) + "\n")
                    output.flush()
                    result = subprocess.run(argv, cwd=root, stdout=output,
                                            stderr=subprocess.STDOUT, timeout=180)
                    output.write(f"Exit code: {result.returncode}\n")
                    failed |= result.returncode != 0
            return 1 if failed else 0
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        with report.open("a", encoding="utf-8") as output:
            output.write(f"Analysis execution failed: {error}\n")
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("tidy", "cppcheck"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    return run(args.kind, args.root, args.build_dir, args.tool, args.report)


if __name__ == "__main__":
    raise SystemExit(main())
