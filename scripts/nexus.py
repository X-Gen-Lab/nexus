#!/usr/bin/env python3
"""Dispatch Nexus commands; build/CI use --preset and --stage from CMakePresets."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
COMMANDS = {
    "build": ("ci/ci_build.py", ()),
    "test": ("ci/ci_build.py", ("--stage", "test")),
    "ci": ("ci/ci_build.py", ()),
    "setup": ("setup/setup.py", ()),
    "format": ("tools/format.py", ()),
    "clean": ("tools/clean.py", ()),
    "docs": ("tools/docs.py", ()),
}


def main(arguments=None):
    args = list(sys.argv[1:] if arguments is None else arguments)
    if not args or args[0] in ("help", "--help", "-h", "--list"):
        print("Usage: python scripts/nexus.py <" + "|".join(COMMANDS) + "> [arguments]")
        print("Build: python scripts/nexus.py build --preset linux-gcc-debug --stage all")
        print("Test: python scripts/nexus.py test --preset linux-gcc-debug")
        return 0
    if args[0] not in COMMANDS:
        print("Unknown command: " + args[0], file=sys.stderr)
        return 2
    script, defaults = COMMANDS[args.pop(0)]
    try:
        return subprocess.run([sys.executable, str(ROOT / script), *defaults, *args], cwd=ROOT.parent).returncode
    except OSError as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
