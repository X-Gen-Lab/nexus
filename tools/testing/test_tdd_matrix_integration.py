"""Gate orchestration fixtures do not establish provider behavior or HIL."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from scripts.ci import io_lifecycle_matrix as matrix
from scripts.ci import tdd_gate


class TDDMatrixIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / "build/native-debug"
        self.binary = self.build / "bin/nexus_provider_test"
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(b"gate orchestration fixture")
        (self.root / "CMakePresets.json").write_text(json.dumps({
            "version": 6,
            "configurePresets": [{"name": "native-debug",
                                  "binaryDir": "${sourceDir}/build/native-debug"}]
        }))
        (self.build / "google-contracts.json").write_text(json.dumps({
            "schema": 1, "executables": [str(self.binary)]
        }))
        self.events = []

    def execute(self, command, _environment, log):
        if "--report" in command:
            Path(command[command.index("--report") + 1]).write_text(
                json.dumps({"passed": 1}))
        elif "build" in command:
            self.events.append("preflight-build" if
                               log.name == "preflight-build.txt" else "build")
        elif "--gtest_list_tests" in command:
            self.events.append("discovery")
            return "Provider.\n  Retains\n"
        elif any(item.startswith("--gtest_output=") for item in command):
            self.events.append("execution")
            name = next(item.split("xml:", 1)[1] for item in command
                        if item.startswith("--gtest_output="))
            Path(name).write_text(
                '<testsuites name="AllTests" tests="1" failures="0" errors="0"'
                ' skipped="0"><testsuite name="Provider" tests="1">'
                '<testcase classname="Provider" name="Retains" status="run"'
                ' result="completed"/></testsuite></testsuites>')
        return ""

    def capture(self, *arguments):
        self.events.append("capture")
        self.assertEqual(self.events[-2:], ["build", "capture"])
        self.assertFalse((self.build / "tdd/execution.json").exists())

    def audit(self, *arguments, **keywords):
        self.events.append("audit")
        self.assertFalse(keywords.get("allow_subset", False))
        self.assertEqual(arguments[0], self.root)
        self.assertEqual(arguments[1], self.build)
        summary = json.loads((self.build / "tdd/execution.json").read_text())
        self.assertEqual(summary["suites"][0]["binary"], self.binary.name)
        return {"kind": "bounded-io-property-execution", "formal_proof": False}

    def run_gate(self, capture=None, audit=None, execute=None):
        output = io.StringIO()
        with patch.object(tdd_gate, "ROOT", self.root), \
                patch.object(tdd_gate, "environment", return_value={}), \
                patch.object(tdd_gate, "execute", side_effect=execute or self.execute), \
                patch.object(matrix, "capture", side_effect=capture or self.capture) as before, \
                patch.object(matrix, "audit", side_effect=audit or self.audit) as after, \
                redirect_stdout(output), redirect_stderr(output):
            status = tdd_gate.main(["--all"])
        return status, output.getvalue(), before, after

    def test_build_capture_execution_summary_strict_audit_order(self):
        status, output, before, after = self.run_gate()
        self.assertEqual(status, 0, output)
        before.assert_called_once()
        after.assert_called_once()
        self.assertEqual(self.events, ["preflight-build", "build", "capture",
                                       "discovery", "execution", "audit"])
        report = self.build / "tdd/io-property-report.json"
        self.assertEqual(json.loads(report.read_text())["formal_proof"], False)

    def test_gate_runs_all_four_maintained_tool_domains(self):
        scopes = []

        def execute(command, environment, log):
            if "--suite" in command:
                scopes.append(command[command.index("--suite") + 1])
            return self.execute(command, environment, log)

        status, output, _, _ = self.run_gate(execute=execute)
        self.assertEqual(status, 0, output)
        self.assertEqual(scopes, ["testing", "configure", "ci", "workflows"])

    def test_capture_failure_stops_before_discovery(self):
        status, output, before, after = self.run_gate(
            capture=ValueError("missing actual compiled context"))
        self.assertEqual(status, 1, output)
        before.assert_called_once()
        after.assert_not_called()
        self.assertNotIn("discovery", self.events)

    def test_audit_failure_rejects_successful_google_execution(self):
        status, output, _, after = self.run_gate(
            audit=ValueError("changed provider source"))
        self.assertEqual(status, 1, output)
        after.assert_called_once()
        self.assertFalse((self.build / "tdd/io-property-report.json").exists())

    def test_prior_matrix_success_is_removed_when_new_tools_fail(self):
        report = self.build / "tdd/io-property-report.json"
        report.parent.mkdir()
        report.write_text('{"previous": "unrelated success"}\n')
        status, output, _, after = self.run_gate(
            execute=ValueError("new tool execution failed"))
        self.assertEqual(status, 1, output)
        after.assert_not_called()
        self.assertFalse(report.exists())


if __name__ == "__main__":
    unittest.main()
