#!/usr/bin/env python3
"""Format owned C/C++ files using the repository's root .clang-format.

All wrappers delegate here. Default/--all selection uses the Git index (including
new staged files). Relative --config/--files paths use the caller's working
directory. Explicit files must belong to the configured owned roots; they never
override exclusions and may be used in non-Git fixtures. Exit 0 means every file passed, 1
means a formatter failure, and 2 means invalid input or failed preflight.
--show-config only reports selection; it does not require clang-format.
"""

import argparse
import fnmatch
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


class FormatError(Exception):
    """A configuration, selection or tool preflight error."""


def get_project_root():
    return Path(__file__).resolve().parents[2]


def parse_format_config(config_path):
    """Read the directory allowlist without falling back to another policy."""
    if not config_path.is_file():
        raise FormatError(f"Format directory configuration not found: {config_path}")
    return parse_format_text(config_path.read_text(encoding="utf-8"), config_path)


def parse_format_text(contents, label):
    """Parse the same policy from a trusted Git blob without a temporary file."""
    config_path = label
    includes, excludes, extensions = [], [], []
    in_extensions = False
    for number, raw in enumerate(contents.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line == "[extensions]" and not in_extensions:
            in_extensions = True
        elif line.startswith("["):
            raise FormatError(f"{config_path}:{number}: unknown or repeated section")
        elif in_extensions:
            if not re.fullmatch(r"\.[A-Za-z0-9]+", line):
                raise FormatError(f"{config_path}:{number}: invalid extension {line!r}")
            extensions.append(line)
        elif line.startswith("!"):
            pattern = line[1:].strip().replace("\\", "/")
            if not pattern:
                raise FormatError(f"{config_path}:{number}: empty exclusion")
            excludes.append(pattern)
        else:
            directory = Path(line.replace("\\", "/"))
            if directory.is_absolute() or ".." in directory.parts:
                raise FormatError(f"{config_path}:{number}: directory must stay inside project")
            includes.append(directory.as_posix())
    if not includes or not extensions:
        raise FormatError(f"{config_path}: include directories and extensions are required")
    return includes, excludes, extensions


def should_exclude(file_path, root, exclude_patterns):
    """Match unqualified patterns at any depth, qualified patterns from root."""
    try:
        parts = file_path.relative_to(root).parts
    except ValueError:
        return True
    prefixes = ["/".join(parts[:index]) for index in range(1, len(parts) + 1)]
    for pattern in exclude_patterns:
        candidates = prefixes if "/" in pattern else parts
        if any(fnmatch.fnmatchcase(candidate, pattern) for candidate in candidates):
            return True
    return False


def owned_roots(root, include_dirs):
    roots = []
    for directory in include_dirs:
        path = root / directory
        resolved = path.resolve()
        if not resolved.is_relative_to(root) or path.is_symlink():
            raise FormatError(f"Configured directory is not a project-owned directory: {path}")
        if not path.is_dir():
            raise FormatError(f"Configured directory not found: {path}")
        roots.append(resolved)
    return roots


def validate_source(file_path, root, roots, exclude_patterns, extensions):
    resolved = file_path.resolve()
    if not resolved.is_relative_to(root):
        raise FormatError(f"Source escapes the project root: {file_path}")
    unresolved = file_path.absolute()
    if unresolved.is_symlink() or any(
        parent.is_symlink() for parent in unresolved.parents if parent.is_relative_to(root)
    ):
        raise FormatError(f"Source must not traverse symlinks: {file_path}")
    if not resolved.is_file():
        raise FormatError(f"Source file not found: {file_path}")
    if should_exclude(resolved, root, exclude_patterns):
        raise FormatError(f"Source is excluded by formatting policy: {file_path}")
    if not any(resolved.is_relative_to(directory) for directory in roots):
        raise FormatError(f"Source is outside configured owned directories: {file_path}")
    if resolved.suffix not in extensions:
        raise FormatError(f"Source extension is not configured: {file_path}")
    return resolved


def find_source_files(root, config_path=None, explicit_files=None):
    root = Path(root).resolve()
    config_path = config_path or root / ".clang-format-dirs"
    includes, excludes, extensions = parse_format_config(config_path)
    roots = owned_roots(root, includes)
    files = set()
    if explicit_files is not None:
        for name in explicit_files:
            files.add(validate_source(Path(name), root, roots, excludes, extensions))
    else:
        for path in tracked_source_files(root):
            if path.suffix not in extensions or should_exclude(path, root, excludes):
                continue
            if any(path.is_relative_to(directory) for directory in roots):
                files.add(validate_source(path, root, roots, excludes, extensions))
    if not files:
        raise FormatError("No owned source files selected; no format check was performed")
    return sorted(files)


def tracked_source_files(root):
    """Read index paths, never untracked files or a filesystem-scan fallback."""
    try:
        result = subprocess.run(["git", "-C", str(root), "ls-files", "--cached", "-z"],
                                capture_output=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise FormatError(f"Failed to read Git index: {error}") from error
    if result.returncode:
        raise FormatError(f"Failed to read Git index: {os.fsdecode(result.stderr).strip()}")
    return [root / os.fsdecode(name) for name in result.stdout.split(b"\0") if name]


def run_tool(command, timeout=30):
    try:
        return subprocess.run(command, capture_output=True, text=True,
                              encoding="utf-8", errors="replace", timeout=timeout)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise FormatError(f"Failed to execute {command[0]}: {error}") from error


def default_tool(root):
    """Prefer the repository's locked development environment over PATH."""
    windows_tool = root / ".venv/Scripts/clang-format.exe"
    posix_tool = root / ".venv/bin/clang-format"
    candidates = (windows_tool, posix_tool) if os.name == "nt" else (posix_tool, windows_tool)
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    return "clang-format"


def check_tool(tool, style_path):
    executable = shutil.which(tool)
    if executable is None:
        raise FormatError(f"clang-format executable not found: {tool}")
    version = run_tool([executable, "--version"])
    if version.returncode or "clang-format" not in version.stdout:
        detail = version.stderr.strip() or version.stdout.strip() or "empty version output"
        raise FormatError(f"clang-format version check failed: {detail}")
    # Validate the root style before any in-place edits. Explicit file: prevents
    # nested .clang-format files or an implicit LLVM fallback changing policy.
    style = run_tool([executable, f"--style=file:{style_path}",
                      "--fallback-style=none", "--dump-config"])
    if style.returncode or not style.stdout.strip():
        detail = style.stderr.strip() or "empty style output"
        raise FormatError(f"Root clang-format configuration rejected: {detail}")
    return executable, version.stdout.strip()


def formatted_source(file_path, tool, style_path):
    """Return formatter output; callers compare it without editing the source."""
    try:
        result = subprocess.run(
            [tool, f"--style=file:{style_path}", "--fallback-style=none", str(file_path)],
            capture_output=True, timeout=120,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise FormatError(f"Failed to execute {tool}: {error}") from error
    if result.returncode:
        detail = result.stderr.decode("utf-8", "replace").strip()
        raise FormatError(f"clang-format failed for {file_path}: {detail}")
    return result.stdout


def format_file(file_path, check_only, verbose, tool, style_path):
    if verbose:
        print(f"Processing: {file_path}")
    mode = ["--dry-run", "--Werror"] if check_only else ["-i"]
    result = run_tool([tool, f"--style=file:{style_path}",
                       "--fallback-style=none", *mode, str(file_path)], timeout=120)
    if result.returncode:
        print(f"FAIL: {file_path}", file=sys.stderr)
        diagnostics = result.stderr.strip() or result.stdout.strip()
        if diagnostics:
            print(diagnostics, file=sys.stderr)
    return result.returncode == 0


def print_config_info(root, config_path, files):
    includes, excludes, extensions = parse_format_config(config_path)
    print(f"Project root: {root}")
    print(f"Style file: {root / '.clang-format'}")
    print(f"Directory configuration: {config_path}")
    print(f"Include directories: {', '.join(includes)}")
    print(f"Exclude patterns: {', '.join(excludes)}")
    print(f"File extensions: {', '.join(extensions)}")
    print(f"Selected owned files: {len(files)}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", nargs="?", choices=("check",),
                        help="Legacy wrapper alias for --check")
    parser.add_argument("-c", "--check", action="store_true",
                        help="Check only; do not modify files")
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("-f", "--config", type=Path,
                        help="Directory policy (default: repository .clang-format-dirs)")
    parser.add_argument("--show-config", action="store_true",
                        help="Validate and show selection; do not run clang-format")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--all", action="store_true",
                           help="Use all owned Git-index files (the default)")
    selection.add_argument("--files", nargs="+", action="append",
                           help="Explicit owned files instead of Git selection (repeatable)")
    parser.add_argument("--tool", help="clang-format executable path or command name")
    args = parser.parse_args(argv)
    root = get_project_root()
    config_path = (args.config or root / ".clang-format-dirs").resolve()
    style_path = root / ".clang-format"
    try:
        if not style_path.is_file() or not style_path.stat().st_size:
            raise FormatError(f"Root clang-format configuration missing or empty: {style_path}")
        explicit_files = [name for group in args.files for name in group] if args.files else None
        files = find_source_files(root, config_path, explicit_files)
        if args.show_config:
            print_config_info(root, config_path, files)
            return 0
        tool, version = check_tool(args.tool or default_tool(root), style_path)
        check_only = args.check or args.mode == "check"
        print(f"{version}\nMode: {'Check' if check_only else 'Format'}; files: {len(files)}")
        if args.verbose:
            print_config_info(root, config_path, files)
        failed = sum(not format_file(path, check_only, args.verbose, tool, style_path)
                     for path in files)
        if failed:
            print(f"FAILED: {failed}/{len(files)} selected files", file=sys.stderr)
            return 1
        print(f"{'Checked' if check_only else 'Formatted'} {len(files)} owned files")
        return 0
    except (FormatError, OSError, UnicodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
