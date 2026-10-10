"""Reject fabricated, skipped, stale or empty GoogleTest execution reports."""

from pathlib import Path
import json
import tempfile
import unittest
from unittest.mock import patch

from scripts.ci.tdd_gate import (check_staged_snapshot, discovered_count,
                                environment, execute, relevant,
                                validate_google_report)


class GoogleReportTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.report = Path(self.directory.name) / "google.xml"

    def write(self, cases, *, tests="1", skipped="0"):
        self.report.write_text(
            '<testsuites tests="' + tests + '" failures="0" errors="0"'
            ' disabled="0" name="AllTests"><testsuite name="Request"'
            ' tests="' + tests + '" failures="0" errors="0" skipped="'
            + skipped + '">' + cases + '</testsuite></testsuites>'
        )

    def test_executed_google_case_passes(self):
        self.write('<testcase name="Drain" classname="Request"'
                   ' status="run" result="completed"/>')
        self.assertEqual(validate_google_report(self.report)["passed"], 1)

    def test_empty_report_fails(self):
        self.write("", tests="0")
        with self.assertRaises(ValueError):
            validate_google_report(self.report)

    def test_generic_junit_is_not_google_execution(self):
        self.write('<testcase name="Drain" classname="Request"/>')
        with self.assertRaises(ValueError):
            validate_google_report(self.report)

    def test_suppressed_or_disabled_case_fails(self):
        for attributes in ('status="notrun" result="suppressed"',
                           'status="run" result="suppressed"'):
            with self.subTest(attributes=attributes):
                self.write('<testcase name="Drain" classname="Request" '
                           + attributes + '/>')
                with self.assertRaises(ValueError):
                    validate_google_report(self.report)

    def test_skipped_case_fails_even_with_one_pass(self):
        self.write('<testcase name="Drain" classname="Request"'
                   ' status="run" result="completed"/>'
                   '<testcase name="Cancel" classname="Request"'
                   ' status="run" result="skipped"><skipped/></testcase>',
                   tests="2", skipped="1")
        with self.assertRaises(ValueError):
            validate_google_report(self.report)

    def test_failure_is_not_a_pass(self):
        self.write('<testcase name="Drain" classname="Request"'
                   ' status="run" result="completed"><failure/>'
                   '</testcase>')
        with self.assertRaises(ValueError):
            validate_google_report(self.report)

    def test_stale_execution_fails(self):
        self.write('<testcase name="Drain" classname="Request"'
                   ' status="run" result="completed"/>')
        with self.assertRaises(ValueError):
            validate_google_report(
                self.report, not_before_ns=self.report.stat().st_mtime_ns + 1
            )

    def test_expected_discovery_count_is_required(self):
        self.write('<testcase name="Drain" classname="Request"'
                   ' status="run" result="completed"/>')
        with self.assertRaises(ValueError):
            validate_google_report(self.report, expected_cases=2)


class GateBoundaryTests(unittest.TestCase):
    def test_all_automation_paths_require_behavioral_execution(self):
        for name in ("tools/hil/runner.py", "tools/measurement/resources.py",
                     "scripts/ci/new_gate.py", "scripts/setup/new_installer.py"):
            with self.subTest(name=name):
                self.assertTrue(relevant(name))
        self.assertFalse(relevant("docs/manual.md"))

    @patch("scripts.ci.tdd_gate.subprocess.run")
    def test_command_and_actual_exit_are_recorded_alongside_raw_log(self, run):
        run.return_value = type("Result", (), {
            "stdout": "actual output\n", "stderr": "", "returncode": 0
        })()
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "execution.txt"
            self.assertEqual(execute(["binary", "--gtest_filter=*"], {}, log),
                             "actual output\n")
            record = log.with_suffix(".command.json")
            self.assertTrue(record.is_file())
            metadata = json.loads(record.read_text())
            self.assertEqual(metadata["command"], ["binary", "--gtest_filter=*"])
            self.assertEqual(metadata["exit_code"], 0)

    def test_discovery_uses_framework_cases_not_banner(self):
        output = ("Running main() from gmock_main.cc\n"
                  "Ownership.\n  Drain\n  Cancel\n"
                  "Provider/0.  # TypeParam = unsigned int\n"
                  "  Isolated/0  # GetParam() = 0\n")
        self.assertEqual(discovered_count(output), 3)

    def test_discovery_without_cases_fails(self):
        with self.assertRaises(ValueError):
            discovered_count("Running main() from gmock_main.cc\n")

    def test_environment_strips_filters_and_shards(self):
        with patch.dict("os.environ", {"GTEST_FILTER": "*Missing*",
                                      "GTEST_TOTAL_SHARDS": "2"}, clear=True):
            actual = environment()
        self.assertNotIn("GTEST_FILTER", actual)
        self.assertNotIn("GTEST_TOTAL_SHARDS", actual)

    def test_bypass_environment_cannot_create_evidence(self):
        with patch.dict("os.environ", {"SKIP": "nexus-tdd-gate"}, clear=True):
            with self.assertRaises(ValueError):
                environment()

    @patch("scripts.ci.tdd_gate.subprocess.run")
    def test_untracked_behavior_cannot_certify_staged_snapshot(self, run):
        first = type("Result", (), {"stdout": b""})()
        second = type("Result", (), {"stdout": b"io/src/not-staged.c\0"})()
        run.side_effect = [first, second]
        with self.assertRaises(ValueError):
            check_staged_snapshot(Path("/fixture"))


if __name__ == "__main__":
    unittest.main()
