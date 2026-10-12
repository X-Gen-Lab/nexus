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


class ToolRunnerTests(unittest.TestCase):
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


class ToolContractBridgeTests(unittest.TestCase):
    def identifiers(self, directory, pattern="test_*.py"):
        script = """import json,sys,unittest
loader=unittest.TestLoader()
suite=loader.discover(sys.argv[1],pattern=sys.argv[2],top_level_dir=sys.argv[1])
def identifiers(value):
    if isinstance(value,unittest.TestSuite):
        return [name for child in value for name in identifiers(child)]
    return [value.id()]
if loader.errors:
    raise ValueError(str(loader.errors))
print(json.dumps(identifiers(suite)))
"""
        result = subprocess.run([sys.executable, "-B", "-c", script,
                                 str(directory), pattern], cwd=ROOT,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def test_ci_discovers_the_whole_original_memory_contract_module_once(self):
        expected = self.identifiers(ROOT / "tests/contracts",
                                    "test_gd32_memory_matrix.py")
        actual = self.identifiers(ROOT / "scripts/ci")
        self.assertGreaterEqual(len(expected), 8)
        self.assertEqual([name for name in actual if name in expected], expected)
        self.assertEqual(len(actual), len(set(actual)))

    def test_changed_external_module_updates_the_complete_ci_discovery(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            ci = root / "scripts/ci"
            contracts = root / "tests/contracts"
            ci.mkdir(parents=True)
            contracts.mkdir(parents=True)
            shutil.copyfile(ROOT / "scripts/ci/test_gd32_memory_contracts.py",
                            ci / "test_gd32_memory_contracts.py")
            original = contracts / "test_gd32_memory_matrix.py"
            shutil.copyfile(ROOT / "tests/contracts" / original.name, original)
            before = self.identifiers(ci)
            original.write_text(original.read_text() + "\n\n"
                "class AddedContract(unittest.TestCase):\n"
                "    def test_live_external_input(self):\n"
                "        self.assertTrue(True)\n")
            after = self.identifiers(ci)
            self.assertEqual(len(before), 8)
            self.assertEqual(set(before), set(self.identifiers(
                ROOT / "tests/contracts", original.name)))
            self.assertEqual(set(after) - set(before), {
                "test_gd32_memory_matrix.AddedContract.test_live_external_input"})
            self.assertEqual(len(after), 9)


if __name__ == "__main__":
    unittest.main()
