#!/usr/bin/env python3
"""Lint scaffold Kconfig names; this does not resolve a product configuration."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from kconfig_tools import KconfigValidator


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args(argv)
    if args.path.is_file():
        files = [args.path]
    elif args.path.is_dir():
        files = sorted(p for p in args.path.rglob("*") if p.is_file() and
                       (p.name == "Kconfig" or p.name.startswith("Kconfig.") or p.suffix == ".kconfig"))
    else:
        parser.error(f"file or directory does not exist: {args.path}")
    if not files:
        print("validation failed: no Kconfig files found", file=sys.stderr)
        return 1
    try:
        validator = KconfigValidator()
        results = {str(path): validator.validate_file(str(path)) for path in files}
        report = validator.generate_report(results)
        if args.report:
            args.report.write_text(report, encoding="utf-8")
        print(report)
    except (OSError, ValueError, TypeError) as error:
        print(f"validation failed: {error}", file=sys.stderr)
        return 1
    errors = sum(issue.severity == "error" for issues in results.values() for issue in issues)
    print(f"Naming lint: {len(files)} files, {errors} errors")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
