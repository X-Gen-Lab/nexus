#!/usr/bin/env python3
"""Install locked commit tools into the repository .venv without replacing hooks.

After installation, activate .venv/bin/activate (POSIX) or
.venv\\Scripts\\Activate.ps1 (PowerShell). Run `python -m pre_commit run --all-files`
or `.venv/bin/clang-format --version` (.venv\\Scripts\\clang-format.exe on Windows).
"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import venv


ROOT = Path(__file__).resolve().parents[2]
PACKAGES = ("pre-commit", "clang-format")
LOCK_NAME = "dependencies/development-tools.txt"
PIN_PATTERN = re.compile(
    r"(pre-commit|clang-format)=="
    r"((?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*))"
)
PROBE = """
import importlib.metadata
import json
import sys

versions = {}
for name in ('pre-commit', 'clang-format'):
    try:
        versions[name] = importlib.metadata.version(name)
    except importlib.metadata.PackageNotFoundError:
        versions[name] = None
print(json.dumps({
    'python': list(sys.version_info[:3]),
    'prefix': sys.prefix,
    'base_prefix': sys.base_prefix,
    'versions': versions,
}))
"""


class InstallError(Exception):
    """An actionable prerequisite or validation failure."""


def read_lock(path):
    """Accept exactly one stable semantic-version pin per allowed package."""
    try:
        contents = path.read_text(encoding="utf-8")
    except OSError as error:
        raise InstallError(f"Cannot read tool lock {path}: {error}") from error
    pins = {}
    for number, raw in enumerate(contents.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        # pip treats an inline comment as such only after whitespace.
        line = re.sub(r"\s+#.*$", "", line).strip()
        match = PIN_PATTERN.fullmatch(line)
        if not match:
            raise InstallError(
                f"Invalid tool lock {path}:{number}: expected an allowed name==X.Y.Z pin"
            )
        name, version = match.groups()
        if name in pins:
            raise InstallError(f"Duplicate tool pin {name} in {path}:{number}")
        pins[name] = version
    missing = set(PACKAGES) - set(pins)
    if missing:
        raise InstallError(f"Tool lock {path} is missing: {', '.join(sorted(missing))}")
    return pins


def venv_paths(directory, platform_name=None):
    platform_name = os.name if platform_name is None else platform_name
    if platform_name == "nt":
        return directory / "Scripts/python.exe", directory / "Scripts/clang-format.exe"
    return directory / "bin/python", directory / "bin/clang-format"


def run(command, root, capture=False):
    return subprocess.run(
        [str(part) for part in command], cwd=root, check=True,
        capture_output=capture, text=True,
    )


def check_hook_location(root):
    """Read effective Git settings; leave existing hooks and config untouched."""
    checkout = run(["git", "rev-parse", "--show-toplevel"], root, capture=True)
    if Path(checkout.stdout.strip()).resolve() != root.resolve():
        raise InstallError("Hook installation requires this repository's Git checkout root")
    setting = subprocess.run(
        ["git", "config", "--get", "core.hooksPath"], cwd=root,
        capture_output=True, text=True, check=False,
    )
    if setting.returncode == 0:
        raise InstallError(
            f"core.hooksPath is explicitly configured ({setting.stdout.strip()!r}); "
            "its hooks will not be changed. Use --skip-hooks to install only tools, "
            "or reconcile that hook configuration before installing repository hooks."
        )
    if setting.returncode != 1:
        raise InstallError(
            f"Cannot inspect core.hooksPath (git exit {setting.returncode}): "
            f"{setting.stderr.strip()}"
        )


def ensure_venv(root):
    directory = root / ".venv"
    if directory.exists():
        if not directory.is_dir() or not (directory / "pyvenv.cfg").is_file():
            raise InstallError(
                f"Existing {directory} is not a complete virtual environment; "
                "repair it explicitly before retrying. It was not overwritten."
            )
    else:
        venv.EnvBuilder(with_pip=True).create(directory)
    python, formatter = venv_paths(directory)
    if not python.is_file():
        raise InstallError(f"Virtual environment Python is missing: {python}; repair .venv")
    return directory, python, formatter


def inspect_venv(directory, python, root):
    result = run([python, "-I", "-c", PROBE], root, capture=True)
    try:
        data = json.loads(result.stdout)
        version = tuple(data["python"])
        prefix = Path(data["prefix"]).resolve()
        base_prefix = Path(data["base_prefix"]).resolve()
        versions = data["versions"]
        if len(version) != 3 or not all(isinstance(part, int) for part in version):
            raise ValueError("invalid Python version")
        if not isinstance(versions, dict) or set(versions) != set(PACKAGES):
            raise ValueError("invalid package version report")
        if any(value is not None and not isinstance(value, str) for value in versions.values()):
            raise ValueError("invalid installed package version")
    except (KeyError, TypeError, ValueError) as error:
        raise InstallError(f"Cannot inspect virtual environment {directory}: {error}") from error
    if version < (3, 10, 0):
        raise InstallError(f"{python} requires Python >=3.10; repair the existing .venv")
    if prefix != directory.resolve() or prefix == base_prefix:
        raise InstallError(f"{python} is not running inside the repository .venv")
    return versions


def verify_tools(python, formatter, pins, root):
    pre_commit = run([python, "-I", "-m", "pre_commit", "--version"], root, capture=True)
    if pre_commit.stdout.strip() != f"pre-commit {pins['pre-commit']}":
        raise InstallError("pre-commit CLI version does not match the development tool lock")
    if not formatter.is_file():
        raise InstallError(f"Locked clang-format executable is missing: {formatter}")
    result = run([formatter, "--version"], root, capture=True)
    match = re.search(r"\bclang-format version\s+([0-9]+\.[0-9]+\.[0-9]+)(?:\s|$)", result.stdout)
    if not match or match.group(1) != pins["clang-format"]:
        raise InstallError("clang-format executable version does not match the development tool lock")


def main(arguments=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--skip-hooks", action="store_true", help="install/verify tools without Git hooks (CI)")
    args = parser.parse_args(arguments)
    try:
        if sys.version_info < (3, 10):
            raise InstallError("Development tool installation requires Python >=3.10")
        root = ROOT.resolve()
        lock = root / LOCK_NAME
        pins = read_lock(lock)
        if not args.skip_hooks:
            check_hook_location(root)
        directory, python, formatter = ensure_venv(root)
        installed = inspect_venv(directory, python, root)
        if all(installed[name] == pins[name] for name in PACKAGES):
            print("Locked development tools already installed; skipping pip installation.")
        else:
            run([python, "-m", "pip", "install", "-r", lock], root)
            installed = inspect_venv(directory, python, root)
        mismatches = [
            f"{name}: expected {pins[name]}, got {installed[name] or 'missing'}"
            for name in PACKAGES if installed[name] != pins[name]
        ]
        if mismatches:
            raise InstallError("Installed tools do not match lock: " + "; ".join(mismatches))
        verify_tools(python, formatter, pins, root)
        if not args.skip_hooks:
            run([
                python, "-I", "-m", "pre_commit", "install", "--install-hooks",
                "--hook-type", "pre-commit", "--hook-type", "commit-msg",
            ], root)
            print("Repository hooks installed using pre-commit's default hook migration.")
        else:
            print("Git hook installation skipped.")
        print(f"Development tools verified in {directory}.")
        print("Run this .venv's Python with -m pre_commit run --all-files; see --help for activation.")
        return 0
    except (InstallError, OSError, subprocess.CalledProcessError, UnicodeError) as error:
        print(f"Development tool setup failed: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr.strip(), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
