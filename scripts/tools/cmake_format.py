#!/usr/bin/env python3
"""Format tracked, owned CMake files with the locked gersemi installation."""

import argparse
import importlib.metadata
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/tools"))
import tool_lock
EXCLUDED = {"vendors", "vendor", "ext", "build", ".venv", ".git"}


def owned_file(name):
    path = Path(name)
    return (not any(part in EXCLUDED or part.startswith(("build-", "cmake-build-"))
                    for part in path.parts)
            and (path.name in {"CMakeLists.txt", "CMakeLists.txt.in"}
                 or name.endswith((".cmake", ".cmake.in"))))


def _run(root, paths, mode):
    if not paths:
        return subprocess.CompletedProcess([], 0, "", "")
    pins = tool_lock.read_lock(root / "dependencies/development-tools.txt")
    try:
        version = importlib.metadata.version("gersemi")
    except importlib.metadata.PackageNotFoundError as error:
        raise ValueError("Install locked development tools before formatting CMake") from error
    if version != pins.get("gersemi"):
        raise ValueError("gersemi version differs from the development tool lock")
    return subprocess.run(
        [sys.executable, "-I", "-m", "gersemi", "--safe", mode,
         "--config", str(root / ".gersemirc"), "--workers", "1",
         "--no-warn-about-unknown-commands", "--", *map(str, paths)],
        cwd=root, text=True, capture_output=True, check=False)


def _ruler_errors(path):
    text = path.read_text(encoding="utf-8")
    # Skip quoted/bracket arguments and bracket comments before line comments.
    tokens = re.finditer(
        r'\\[\s\S]|"(?:\\[\s\S]|[^"\\])*"|'
        r'#?\[(?P<bracket>=*)\[[\s\S]*?\](?P=bracket)\]|'
        r'(?P<comment>#[^\n]*)', text)
    errors = []
    for token in tokens:
        comment = token.group("comment")
        if (comment is None or not re.fullmatch(
                r"#[ \t]*(?:[-=][ \t]*){5,}", comment)):
            continue
        prefix = text[text.rfind("\n", 0, token.start()) + 1:token.start()]
        if prefix.strip():
            continue
        if "=" in comment or len(prefix + comment) != 77:
            line = text.count("\n", 0, token.start()) + 1
            errors.append(f"{path}:{line}: CMake section rulers must use "
                          "77 characters with hyphens; equals rulers are forbidden")
    return errors


def check_files(root, paths):
    errors = [error for path in paths for error in _ruler_errors(path)]
    result = _run(root, paths, "--check")
    if result.returncode:
        errors.append("CMake format: " +
                      (result.stdout.strip() or result.stderr.strip()))
    return errors


def format_files(root, paths):
    result = _run(root, paths, "--in-place")
    if result.returncode:
        raise ValueError(result.stdout.strip() or result.stderr.strip())


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("files", nargs="*")
    args = parser.parse_args(arguments)
    names = subprocess.check_output(
        ["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
    selected = args.files or [name for name in names if owned_file(name)]
    if any(not owned_file(name) or name not in names for name in selected):
        parser.error("CMake format selection must contain only tracked owned files")
    paths = [ROOT / name for name in selected if (ROOT / name).is_file()]
    if args.check:
        errors = check_files(ROOT, paths)
        if errors:
            print("\n".join(errors), file=sys.stderr)
        return int(bool(errors))
    format_files(ROOT, paths)
    return 0


if __name__ == "__main__":
    sys.exit(main())
