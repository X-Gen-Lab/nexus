"""Exercise the production evidence checker through its real CLI subprocess."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "scripts/validation/junit.py"
GOOD = '<testsuite tests="1" failures="0" errors="0" skipped="0"><testcase name="real.case"/></testsuite>'


class JunitEvidenceContracts(unittest.TestCase):
    def run_checker(self, contents, *, stale=False, suffix=()):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.xml"
            start_ns = time.time_ns()
            if contents is not None:
                report.write_text(contents)
                # Synthetic checker inputs need controlled timestamps; filesystem
                # clock ticks must not determine fixture validity. Real execution
                # reports keep their actual timestamps in production callers.
                stamp_ns = start_ns - 10_000_000_000 if stale else start_ns
                os.utime(report, ns=(stamp_ns,) * 2)
            return subprocess.run([sys.executable, str(CHECKER), "--report", str(report),
                                   "--not-before-ns", str(start_ns), *suffix],
                                  capture_output=True, text=True, timeout=10)

    def test_actual_case_identity_and_hash_retained(self):
        result = self.run_checker(GOOD)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual((data["tests"], data["passed"], data["skipped"]), (1, 1, 0))
        self.assertEqual(data["cases"][0]["name"], "real.case")
        self.assertEqual(len(data["report_sha256"]), 64)

    def test_fixture_freshness_is_independent_of_filesystem_clock_lag(self):
        future_ns = time.time_ns() + 10_000_000_000
        with patch.object(time, "time_ns", return_value=future_ns):
            fresh = self.run_checker(GOOD)
            stale = self.run_checker(GOOD, stale=True)
        self.assertEqual(fresh.returncode, 0, fresh.stderr)
        self.assertNotEqual(stale.returncode, 0)
        self.assertIn("predates this test execution", stale.stderr)

    def test_missing_report_rejected(self):
        self.assertNotEqual(self.run_checker(None).returncode, 0)

    def test_stale_previous_success_rejected(self):
        self.assertNotEqual(self.run_checker(GOOD, stale=True).returncode, 0)

    def test_malformed_xml_rejected(self):
        self.assertNotEqual(self.run_checker("<testsuite>").returncode, 0)

    def test_zero_test_report_rejected(self):
        self.assertNotEqual(self.run_checker('<testsuite tests="0"/>').returncode, 0)

    def test_all_skipped_report_rejected(self):
        data = '<testsuite tests="1" skipped="1"><testcase name="unused"><skipped/></testcase></testsuite>'
        self.assertNotEqual(self.run_checker(data).returncode, 0)

    def test_false_counts_rejected(self):
        self.assertNotEqual(self.run_checker(GOOD.replace('tests="1"', 'tests="2"')).returncode, 0)

    def test_failures_and_errors_rejected(self):
        for tag in ("failure", "error"):
            with self.subTest(tag=tag):
                data = f'<testsuite><testcase name="broken"><{tag}/></testcase></testsuite>'
                self.assertNotEqual(self.run_checker(data).returncode, 0)

    def test_duplicate_and_missing_names_rejected(self):
        for data in ('<testsuite><testcase/><testcase name="other"/></testsuite>',
                     '<testsuite><testcase name="same"/><testcase name="same"/></testsuite>'):
            with self.subTest(data=data):
                self.assertNotEqual(self.run_checker(data).returncode, 0)

    def test_nested_suite_totals_are_validated(self):
        good = '<testsuites tests="1">' + GOOD + '</testsuites>'
        self.assertEqual(self.run_checker(good).returncode, 0)
        bad = good.replace('<testsuites tests="1">', '<testsuites tests="2">')
        self.assertNotEqual(self.run_checker(bad).returncode, 0)

    def test_partial_skip_is_reported_without_becoming_passed(self):
        data = '<testsuite tests="2" skipped="1"><testcase name="ran"/><testcase name="did.not.run"><skipped/></testcase></testsuite>'
        result = self.run_checker(data)
        self.assertEqual(result.returncode, 0, result.stderr)
        counts = json.loads(result.stdout)
        self.assertEqual((counts["tests"], counts["passed"], counts["skipped"]), (2, 1, 1))

    def test_orphan_error_and_nonexecuted_status_rejected(self):
        for data in ('<testsuite><testcase name="looks.good"/><error/></testsuite>',
                     '<testsuite><testcase name="did.not.run" status="notrun"/></testsuite>'):
            with self.subTest(data=data):
                self.assertNotEqual(self.run_checker(data).returncode, 0)


if __name__ == "__main__":
    unittest.main()
