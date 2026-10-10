#!/usr/bin/env python3
"""Run real tool tests and reject empty, incomplete or skipped execution."""

import argparse
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))


def execute(suite):
    """Return actual runner counts; loading or skipping tests is not a pass."""
    tests = unittest.defaultTestLoader.discover(
        str(ROOT / "tools" / suite), pattern="test_*.py"
    )
    expected = tests.countTestCases()
    if expected == 0:
        raise ValueError("Tool suite discovers zero tests")
    result = unittest.TextTestRunner(verbosity=1).run(tests)
    record = {"suite": suite, "discovered": expected,
              "executed": result.testsRun, "skipped": len(result.skipped),
              "failures": len(result.failures), "errors": len(result.errors),
              "expected_failures": len(result.expectedFailures),
              "unexpected_successes": len(result.unexpectedSuccesses)}
    if (not result.wasSuccessful() or result.testsRun != expected
            or result.skipped or result.expectedFailures):
        raise ValueError("Tool suite did not completely pass: " + json.dumps(record))
    return {**record, "status": "passed"}


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=("testing", "configure"), required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(arguments)
    args.report.unlink(missing_ok=True)
    try:
        record = execute(args.suite)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(record, indent=2) + "\n")
    except (ValueError, OSError) as error:
        print(f"Tool test execution rejected: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
