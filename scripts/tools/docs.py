#!/usr/bin/env python3
"""Forward legacy documentation commands to the maintained developer CLI.

The former Doxygen/Sphinx targets now select one complete, strict documentation
build. This wrapper never creates configuration or reuses separately built XML.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("legacy_target", nargs="?",
                        choices=("all", "doxygen", "sphinx"), default="all")
    parser.add_argument("-t", "--target", choices=("all", "doxygen", "sphinx"))
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--language", choices=("en", "zh_CN", "all"),
                        default="all")
    args = parser.parse_args(argv)
    print("Legacy docs entry forwards to: tools/dev/dev.py docs build; "
          "all legacy targets perform the same strict build.", file=sys.stderr)
    command = [sys.executable, str(ROOT / "tools/dev/dev.py"), "docs", "build",
               "--language", args.language]
    if args.output:
        command.extend(["--output", str(args.output.resolve())])
    return subprocess.call(command, cwd=ROOT)


if __name__ == "__main__":
    raise SystemExit(main())
