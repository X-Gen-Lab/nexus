#!/usr/bin/env python3
"""Configure, build and (for Native test presets) test through one maintained entry.

Prerequisites and pinned dependencies must already be installed. This command
never installs packages, selects a platform alias, or claims MCU execution.
"""
from pathlib import Path
import subprocess
import sys

if __name__ == "__main__":
    runner = Path(__file__).resolve().parents[1] / "ci/ci_build.py"
    sys.exit(subprocess.run([sys.executable, str(runner), *sys.argv[1:]]).returncode)
