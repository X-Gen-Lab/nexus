"""Actual unittest result state determines acceptance of tool regressions."""

from contextlib import redirect_stderr
import io
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.testing.run_tool_tests import execute, ROOT
from tools.testing import run_tool_tests


class ToolRunnerTests(unittest.TestCase):
    def test_documentation_directory_uses_complete_execution_admission(self):
        fixture = unittest.TestSuite([unittest.FunctionTestCase(lambda: None)])
        with patch("unittest.TestLoader.discover", return_value=fixture) as discover, \
                redirect_stderr(io.StringIO()):
            record = run_tool_tests.execute_directory(ROOT / "tools/documentation",
                                                       "documentation")
        self.assertEqual(record["executed"], 1)
        self.assertEqual(record["status"], "passed")
        self.assertEqual(Path(discover.call_args.args[0]), ROOT / "tools/documentation")

    def run_fixture(self, tests):
        with patch("unittest.TestLoader.discover", return_value=tests), \
                redirect_stderr(io.StringIO()):
            return execute("testing")

    def test_ci_and_workflows_discover_their_real_maintained_directories(self):
        for scope, directory in (("ci", "scripts/ci"),
                                 ("workflows", ".github/workflows")):
            tests = unittest.TestSuite([unittest.FunctionTestCase(lambda: None)])
            with patch("unittest.TestLoader.discover", return_value=tests) as discover, \
                    redirect_stderr(io.StringIO()):
                result = execute(scope)
            self.assertEqual(result["executed"], 1)
            self.assertEqual(Path(discover.call_args.args[0]), ROOT / directory)

    def test_ci_only_domains_discover_their_complete_maintained_directories(self):
        for scope, directory in (("contracts", "tests/contracts"),
                                 ("freertos", "os/freertos")):
            tests = unittest.TestSuite([unittest.FunctionTestCase(lambda: None)])
            with patch("unittest.TestLoader.discover", return_value=tests) as discover, \
                    redirect_stderr(io.StringIO()):
                result = execute(scope)
            self.assertEqual(result["executed"], 1)
            self.assertEqual(Path(discover.call_args.args[0]), ROOT / directory)
            self.assertNotIn(scope, run_tool_tests.SUITES)

    def test_ci_only_domain_uses_real_cli_and_fresh_nonzero_report(self):
        with tempfile.TemporaryDirectory(prefix="tool domain ") as name:
            root = Path(name)
            tests = root / "os/freertos"
            tests.mkdir(parents=True)
            (tests / "test_fixture.py").write_text(
                "import unittest\n"
                "class Domain(unittest.TestCase):\n"
                "    def test_complete(self):\n"
                "        self.assertEqual(2 + 2, 4)\n")
            runner = root / "tools/testing/run_tool_tests.py"
            runner.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "tools/testing/run_tool_tests.py", runner)
            report = root / "execution.json"
            report.write_text('{"stale": true}\n')
            command = [sys.executable, "-B", str(runner), "--suite",
                       "freertos", "--report", str(report)]
            completed = subprocess.run(command, capture_output=True,
                                       text=True, timeout=30, check=False)
            self.assertEqual(completed.returncode, 0,
                             completed.stdout + completed.stderr)
            self.assertEqual(json.loads(report.read_text()), {
                "suite": "freertos", "discovered": 1, "executed": 1,
                "skipped": 0, "failures": 0, "errors": 0,
                "expected_failures": 0, "unexpected_successes": 0,
                "status": "passed"})

    def test_ci_only_domain_skip_cannot_reuse_a_previous_success(self):
        def skipped():
            raise unittest.SkipTest("fixture")

        tests = unittest.TestSuite([unittest.FunctionTestCase(skipped)])
        with tempfile.TemporaryDirectory() as name:
            report = Path(name) / "execution.json"
            report.write_text('{"status": "passed"}\n')
            with patch("unittest.TestLoader.discover", return_value=tests), \
                    redirect_stderr(io.StringIO()):
                result = run_tool_tests.main([
                    "--suite", "contracts", "--report", str(report)])
            self.assertEqual(result, 1)
            self.assertFalse(report.exists())

    def test_actual_execution_reports_nonzero_count(self):
        observed = []
        fixture = unittest.TestSuite([
            unittest.FunctionTestCase(lambda: observed.append("first")),
            unittest.FunctionTestCase(lambda: observed.append("second"))])
        record = self.run_fixture(fixture)
        self.assertEqual(observed, ["first", "second"])
        self.assertEqual(record["executed"], 2)
        self.assertEqual(record["discovered"], 2)
        self.assertEqual(record["status"], "passed")

    def assert_first_failure_stops_later_case(self, kind):
        with tempfile.TemporaryDirectory() as name:
            directory = Path(name)
            sentinel = directory / "later-case-ran"
            module = directory / ("test_failfast_" + directory.name + ".py")
            first = ("self.fail('first real failure')" if kind == "failure"
                     else "raise RuntimeError('first real error')")
            module.write_text(
                "from pathlib import Path\nimport unittest\n"
                "class ActualPlan(unittest.TestCase):\n"
                "    def test_00_first(self):\n"
                f"        {first}\n"
                "    def test_01_later(self):\n"
                f"        Path({str(sentinel)!r}).write_text('executed')\n")
            trace = io.StringIO()
            try:
                with redirect_stderr(trace), self.assertRaises(ValueError) as caught:
                    run_tool_tests.execute_directory(directory, "failure-fixture")
            finally:
                sys.modules.pop(module.stem, None)
                if str(directory) in sys.path:
                    sys.path.remove(str(directory))
            self.assertFalse(sentinel.exists())
            self.assertIn("first real " + kind, trace.getvalue())
            record = json.loads(str(caught.exception).split(": ", 1)[1])
            self.assertEqual(record["discovered"], 2)
            self.assertEqual(record["executed"], 1)
            self.assertEqual(record["failures"], int(kind == "failure"))
            self.assertEqual(record["errors"], int(kind == "error"))

    def test_first_actual_failure_keeps_trace_and_stops_later_case(self):
        self.assert_first_failure_stops_later_case("failure")

    def test_first_actual_error_keeps_trace_and_stops_later_case(self):
        self.assert_first_failure_stops_later_case("error")

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
