#!/usr/bin/env python3
"""Check a scoped LCOV trace; capture/filtering remain the maintained CI workflow."""
import argparse
import math
from pathlib import Path
import sys


def parse_lcov_info(path, *, not_before_ns=None):
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise ValueError("LCOV trace is missing or not a regular file")
    if not_before_ns is not None and path.stat().st_mtime_ns < not_before_ns:
        raise ValueError("LCOV trace predates this capture")
    records = []
    current = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("SF:"):
            if current is not None or not line[3:]:
                raise ValueError("LCOV source record is missing its terminator or name")
            current = {"source": line[3:], "lines": {}, "declared": {}}
        elif line.startswith("DA:"):
            if current is None:
                raise ValueError("LCOV line data has no source record")
            try:
                fields = line[3:].split(",")
                number, hits = int(fields[0]), int(fields[1])
            except (ValueError, IndexError) as error:
                raise ValueError("Invalid LCOV line data") from error
            if number <= 0 or hits < 0 or number in current["lines"]:
                raise ValueError("Invalid or duplicate LCOV line data")
            current["lines"][number] = hits
        elif line.startswith(("LF:", "LH:")):
            if current is None or line[:2] in current["declared"]:
                raise ValueError("Duplicate or misplaced LCOV line counts")
            try:
                current["declared"][line[:2]] = int(line[3:])
            except ValueError as error:
                raise ValueError("Invalid LCOV line count") from error
        elif line == "end_of_record":
            if current is None:
                raise ValueError("LCOV record terminator has no source")
            found = len(current["lines"])
            hit = sum(count > 0 for count in current["lines"].values())
            if current["declared"] != {"LF": found, "LH": hit}:
                raise ValueError("LCOV declared line counts disagree with line data")
            records.append({"source": current["source"], "found": found, "hit": hit})
            current = None
    if current is not None or not records:
        raise ValueError("LCOV trace is empty or has an unterminated record")
    found = sum(record["found"] for record in records)
    hit = sum(record["hit"] for record in records)
    if found == 0:
        raise ValueError("LCOV contains no measured source lines")
    return {"lines_found": found, "lines_hit": hit, "line_coverage": hit / found,
            "source_records": records}


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coverage-file", required=True, type=Path)
    parser.add_argument("--threshold", type=float, default=0.80)
    parser.add_argument("--type", choices=["line"], default="line",
                        help="This gate validates line coverage only")
    parser.add_argument("--not-before-ns", type=int)
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args(arguments)
    if not math.isfinite(args.threshold) or not 0 <= args.threshold <= 1:
        parser.error("--threshold must be finite and between zero and one")
    try:
        data = parse_lcov_info(args.coverage_file, not_before_ns=args.not_before_ns)
        if data["line_coverage"] < args.threshold:
            raise ValueError(f"line coverage {data['line_coverage']:.2%} is below {args.threshold:.2%}")
    except (OSError, UnicodeError, ValueError) as error:
        print(f"Coverage rejected: {error}", file=sys.stderr)
        return 1
    print(f"Scoped line coverage: {data['lines_hit']}/{data['lines_found']} = {data['line_coverage']:.2%}")
    if args.verbose:
        for record in data["source_records"]:
            print(f"{record['source']}: {record['hit']}/{record['found']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
