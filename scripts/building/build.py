#!/usr/bin/env python3
"""Forward to the single maintained preset workflow (use --preset/--stage)."""
from pathlib import Path
import runpy

if __name__ == "__main__":
    runpy.run_path(str(Path(__file__).resolve().parents[1] / "ci/ci_build.py"), run_name="__main__")
