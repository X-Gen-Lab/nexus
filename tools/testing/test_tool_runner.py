"""Actual unittest result state determines acceptance of tool regressions."""

from contextlib import redirect_stderr
import io
import unittest
from unittest.mock import patch

from tools.testing.run_tool_tests import execute


class ToolRunnerTests(unittest.TestCase):
    def run_fixture(self, tests):
        with patch("unittest.defaultTestLoader.discover", return_value=tests), \
                redirect_stderr(io.StringIO()):
            return execute("testing")

    def test_actual_execution_reports_nonzero_count(self):
        fixture = unittest.TestSuite([unittest.FunctionTestCase(lambda: None)])
        record = self.run_fixture(fixture)
        self.assertEqual(record["executed"], 1)
        self.assertEqual(record["discovered"], 1)
        self.assertEqual(record["status"], "passed")

    def test_empty_discovery_is_not_success(self):
        with self.assertRaises(ValueError):
            self.run_fixture(unittest.TestSuite())

    def test_skip_cannot_replace_actual_contract_execution(self):
        def skipped():
            raise unittest.SkipTest("Dependency is missing")

        tests = unittest.TestSuite([unittest.FunctionTestCase(lambda: None),
                                    unittest.FunctionTestCase(skipped)])
        with self.assertRaises(ValueError):
            self.run_fixture(tests)

    def test_failed_case_is_not_accepted(self):
        def failed():
            raise AssertionError("Required behavior is absent")

        with self.assertRaises(ValueError):
            self.run_fixture(unittest.TestSuite([unittest.FunctionTestCase(failed)]))

    def test_expected_failure_is_not_delivered_behavior(self):
        class ExpectedFailure(unittest.TestCase):
            @unittest.expectedFailure
            def runTest(self):
                self.fail("Required behavior is absent")

        with self.assertRaises(ValueError):
            self.run_fixture(unittest.TestSuite([ExpectedFailure()]))


if __name__ == "__main__":
    unittest.main()
