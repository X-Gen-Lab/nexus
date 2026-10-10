"""The development entry point must propagate real gate and tool failures."""

from contextlib import redirect_stderr, redirect_stdout
import io
import subprocess
import unittest
from unittest.mock import patch

from tools.dev import dev


class DeveloperCommandTests(unittest.TestCase):
    def test_check_runs_tdd_for_requested_preset_and_propagates_failure(self):
        with patch.object(dev, "execute", side_effect=[0, 0, 0, 7]) as execute:
            result = dev.main(["check", "--preset", "native-release"])
        self.assertEqual(result, 7)
        self.assertEqual(execute.call_args_list[3].args[0],
                         [dev.sys.executable, "scripts/ci/tdd_gate.py", "--all",
                          "--preset", "native-release"])

    def test_maintenance_failure_stops_before_contract_execution(self):
        with patch.object(dev, "execute", side_effect=[0, 6]) as execute:
            result = dev.main(["check"])
        self.assertEqual(result, 6)
        self.assertEqual(execute.call_count, 2)
        self.assertEqual(execute.call_args_list[0].args[0][1],
                         "scripts/ci/style_gate.py")
        self.assertEqual(execute.call_args_list[1].args[0][1],
                         "tools/maintenance/capabilities.py")

    def test_style_failure_stops_before_contract_execution(self):
        with patch.object(dev, "execute", return_value=8) as execute:
            result = dev.main(["check"])
        self.assertEqual(result, 8)
        self.assertEqual(execute.call_count, 1)

    def probe(self, command, **kwargs):
        del kwargs
        if "cmake" in command[0]:
            output = "cmake version 3.31.6\n"
        elif "ninja" in command[0]:
            output = "1.13.2\n"
        else:
            output = "14.2.0\n"
        return subprocess.CompletedProcess(command, 0, output, "")

    def test_doctor_rejects_python_without_stdlib_toml(self):
        with patch.object(dev.sys, "version_info", (3, 10, 14)), \
                patch.object(dev.subprocess, "run", side_effect=self.probe), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = dev.main(["doctor"])
        self.assertEqual(result, 2)

    def test_doctor_reports_python_and_locked_tool_versions(self):
        output = io.StringIO()
        with patch.object(dev.subprocess, "run", side_effect=self.probe), \
                redirect_stdout(output), redirect_stderr(io.StringIO()):
            result = dev.main(["doctor"])
        self.assertEqual(result, 0)
        self.assertIn("Python:", output.getvalue())
        self.assertIn("tomllib", output.getvalue())


if __name__ == "__main__":
    unittest.main()
