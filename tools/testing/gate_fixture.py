"""Small external-command fixture for gate sequencing and failure tests."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from scripts.ci import tdd_gate


class GateFixture(unittest.TestCase):
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

    def run_gate(self, *, execute=None, arguments=None):
        output = io.StringIO()
        with patch.object(tdd_gate, "ROOT", self.root), \
                patch.object(tdd_gate, "environment", return_value={}), \
                patch.object(tdd_gate, "execute", side_effect=execute or self.execute), \
                redirect_stdout(output), redirect_stderr(output):
            status = tdd_gate.main(arguments or ["--all"])
        return status, output.getvalue()
