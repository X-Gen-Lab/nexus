#!/usr/bin/env python3
"""Validate real CTest/JUnit evidence; never infer success from console text."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def validate_junit(report, *, not_before_ns=None):
    """Return actual case identities/counts, or raise ValueError on bad evidence.

    Callers delete the old report before launching CTest, require its subprocess
    return code to be zero, then supply their start time as not_before_ns. This
    parser cannot establish which source/configuration executed the cases; the
    enclosing candidate manifest must bind those identities separately.
    """
    path = Path(report)
    try:
        if not path.is_file() or path.is_symlink():
            raise ValueError("JUnit report is missing or is not a regular file")
        if not_before_ns is not None and path.stat().st_mtime_ns < not_before_ns:
            raise ValueError("JUnit report predates this test execution")
        contents = path.read_bytes()
        root = ET.fromstring(contents)
    except (OSError, ET.ParseError) as error:
        raise ValueError(f"Cannot read JUnit report: {error}") from error
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError("JUnit root must be testsuite or testsuites")

    def cases_for(element):
        return list(element.iter("testcase"))

    def counts_for(elements):
        return {
            "tests": len(elements),
            "failures": sum(c.find("failure") is not None for c in elements),
            "errors": sum(c.find("error") is not None for c in elements),
            "skipped": sum(c.find("skipped") is not None for c in elements),
        }

    cases = cases_for(root)
    counts = counts_for(cases)
    if any(element.tag in ("failure", "error") for element in root.iter()):
        raise ValueError("JUnit contains failed or errored execution evidence")
    if counts["tests"] == 0:
        raise ValueError("JUnit contains zero test cases")
    for suite in root.iter():
        if suite.tag not in ("testsuite", "testsuites"):
            continue
        actual = counts_for(cases_for(suite))
        for field, expected in actual.items():
            if field not in suite.attrib:
                continue
            try:
                declared = int(suite.attrib[field])
            except ValueError as error:
                raise ValueError(f"Invalid JUnit {field} count") from error
            if declared != expected:
                raise ValueError(f"JUnit {field} count does not match actual cases")
    identities = set()
    records = []
    for case in cases:
        name = case.get("name", "").strip()
        classname = case.get("classname", "")
        if not name or (classname, name) in identities:
            raise ValueError("JUnit case names must be nonempty and unique within a class")
        if case.get("status", "run") not in ("run", "passed") and case.find("skipped") is None:
            raise ValueError("JUnit case status does not establish execution")
        identities.add((classname, name))
        statuses = [kind for kind in ("failure", "error", "skipped") if case.find(kind) is not None]
        if len(statuses) > 1:
            raise ValueError("JUnit case has contradictory terminal states")
        records.append({"name": name, "classname": classname,
                        "status": statuses[0] if statuses else "passed"})
    if counts["failures"] or counts["errors"]:
        raise ValueError("JUnit contains failed or errored test cases")
    passed = counts["tests"] - counts["skipped"]
    if passed == 0:
        raise ValueError("JUnit contains only skipped tests; no test executed")
    return {**counts, "passed": passed, "cases": records,
            "report_sha256": hashlib.sha256(contents).hexdigest()}


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--not-before-ns", type=int)
    args = parser.parse_args(arguments)
    try:
        result = validate_junit(args.report, not_before_ns=args.not_before_ns)
    except ValueError as error:
        print(f"JUnit rejected: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
