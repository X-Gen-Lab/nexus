#!/usr/bin/env python3
"""Verify the maintained configuration schema through the strict generator.

The parser follows the same platform -> SoC -> controller catalogs used by
CMake. Parsing does not prove provider execution or hardware qualification.
"""
import os
from pathlib import Path
import sys

from generate_config import resolve_config

ROOT = Path(__file__).resolve().parents[2]


def main():
    os.chdir(ROOT)
    try:
        kconf = resolve_config(str(ROOT / "Kconfig"))
    except (OSError, ValueError) as error:
        print(f"Kconfig verification failed: {error}", file=sys.stderr)
        return 1
    platforms = ("PLATFORM_NATIVE", "PLATFORM_STM32", "PLATFORM_GD32F470")
    for name in platforms:
        symbol = kconf.syms.get(name)
        if symbol is None or not symbol.nodes:
            print(f"Missing maintained platform {name}", file=sys.stderr)
            return 1
    print(f"Verified strict Kconfig schema: {len(kconf.kconfig_filenames)} input files; "
          "Native, STM32F407 and GD32F470 catalogs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
