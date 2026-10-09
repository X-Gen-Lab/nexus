#!/usr/bin/env python3
"""Run the maintained preset workflow. Historical ad-hoc configuration is removed."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main(arguments=None):
    args = sys.argv[1:] if arguments is None else arguments
    try:
        return subprocess.run([sys.executable, str(ROOT / "scripts/ci/ci_build.py"),
                               *args], cwd=ROOT).returncode
    except OSError as error:
        print(f"Validation command failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
