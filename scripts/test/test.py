#!/usr/bin/env python3
"""Run CTest through the maintained preset workflow."""
from pathlib import Path
import runpy
import sys

if __name__ == "__main__":
    sys.argv[1:1] = ["--stage", "test"]
    runpy.run_path(str(Path(__file__).resolve().parents[1] / "ci/ci_build.py"), run_name="__main__")
