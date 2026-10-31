"""Tool and Native link failures stop the gate before complete suites."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

from scripts.ci import tdd_gate
from tools.dev import dev
from tools.testing import test_tdd_matrix_integration as integration


class TDDToolPreflightTests(unittest.TestCase):
    def setUp(self):
        self.fixture = integration.TDDMatrixIntegrationTests(
            "test_build_capture_execution_summary_strict_audit_order")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def command(self, target):
        return [sys.executable, "tools/dev/dev.py", "doctor",
                "--target", target]

    def test_combined_check_precedes_all_complete_tool_suites(self):
        commands, logs = [], []

        def execute(command, environment, log):
            commands.append(command)
            logs.append(log)
            return self.fixture.execute(command, environment, log)

        status, output, _, _ = self.fixture.run_gate(execute=execute)
        self.assertEqual(status, 0, output)
        configure = [sys.executable, "tools/dev/dev.py", "configure",
                     "--preset", "native-debug"]
        build = [sys.executable, "tools/dev/dev.py", "build", "--preset",
                 "native-debug", "--target", "nexus_google_contracts"]
        self.assertEqual(commands[:3], [self.command("contracts"),
                                       configure, build])
        self.assertEqual([log.name for log in logs[:3]],
                         ["preflight-contracts.txt", "preflight-configure.txt",
                          "preflight-build.txt"])
        first_suite = next(index for index, command in enumerate(commands)
                           if "--suite" in command)
        last_suite = max(index for index, command in enumerate(commands)
                         if "--suite" in command)
        self.assertLess(commands.index(build), first_suite)
        self.assertEqual(commands.count(configure), 2)
        self.assertEqual(commands.count(build), 2)
        self.assertEqual(commands[last_suite + 1:last_suite + 3],
                         [configure, build])
        self.assertEqual([log.name for log in logs[last_suite + 1:
                                                 last_suite + 3]],
                         ["configure.txt", "build.txt"])
        self.assertEqual([command[command.index("--suite") + 1]
                          for command in commands if "--suite" in command],
                         ["testing", "configure", "ci", "workflows"])
        self.assertFalse(any("candidate" in command for command in commands))

    def assert_failed_compile_preflight(self, action):
        commands = []
        directory = self.fixture.build / "tdd"
        directory.mkdir()
        summary = directory / "execution.json"
        summary.write_text('{"previous": "unrelated execution"}\n')

        def execute(command, environment, log):
            commands.append(command)
            if command[:3] == [sys.executable, "tools/dev/dev.py", action]:
                raise ValueError("Native " + action + " preflight failed")
            return self.fixture.execute(command, environment, log)

        status, output, before, after = self.fixture.run_gate(execute=execute)
        self.assertEqual(status, 1, output)
        self.assertEqual(commands[0], self.command("contracts"))
        self.assertIn("Native " + action + " preflight failed", output)
        self.assertFalse(any("--suite" in command for command in commands),
                         "Compile/link failures must precede tool suites")
        before.assert_not_called()
        after.assert_not_called()
        self.assertFalse(summary.exists())

    def test_native_configure_failure_stops_before_any_complete_suite(self):
        self.assert_failed_compile_preflight("configure")

    def test_native_link_failure_stops_before_any_complete_suite(self):
        self.assert_failed_compile_preflight("build")

    def assert_failed_preflight(self, target):
        commands = []
        directory = self.fixture.build / "tdd"
        directory.mkdir()
        summary = directory / "execution.json"
        summary.write_text('{"previous": "unrelated execution"}\n')

        def execute(command, environment, log):
            commands.append(command)
            if command == self.command("contracts"):
                raise ValueError("Missing locked compiler for " + target)
            return self.fixture.execute(command, environment, log)

        status, output, before, after = self.fixture.run_gate(execute=execute)
        self.assertEqual(status, 1, output)
        self.assertEqual(commands, [self.command("contracts")])
        self.assertIn("Missing locked compiler", output)
        before.assert_not_called()
        after.assert_not_called()
        self.assertFalse(summary.exists())
        self.assertFalse(any("--suite" in command for command in commands))

    def test_missing_native_tools_stop_before_arm_or_any_suite(self):
        self.assert_failed_preflight("native")

    def test_missing_arm_tools_stop_before_any_suite_or_google_build(self):
        self.assert_failed_preflight("arm")

    def test_documentation_only_staged_change_does_not_probe_compilers(self):
        output = io.StringIO()
        with patch.object(tdd_gate, "environment", return_value={}), \
                patch.object(tdd_gate, "staged_paths",
                             return_value=["README.md"]), \
                patch.object(tdd_gate, "execute") as execute, \
                patch.object(tdd_gate, "staged_tree") as tree, \
                redirect_stdout(output), redirect_stderr(output):
            status = tdd_gate.main(["--staged"])
        self.assertEqual(status, 0, output.getvalue())
        execute.assert_not_called()
        tree.assert_not_called()
        self.assertIn("No staged behavioral changes", output.getvalue())


class ContractDoctorProfileTests(unittest.TestCase):
    def doctor(self, *, missing=None, wrong=None):
        lock_path = dev.ROOT / "dependencies/environment.lock.json"
        lock = json.loads(lock_path.read_text())
        self.commands = []

        def probe(command, **keywords):
            del keywords
            self.commands.append(command)
            name = Path(command[0]).name
            if name in ("cmake", "ninja"):
                version = lock["tools"][name]
            else:
                version = lock["arm_compiler" if name.startswith("arm-")
                               else "native_compiler"]["version"]
            if name == wrong:
                version = "0.0.0"
            return subprocess.CompletedProcess(command, 0, version + "\n", "")

        output = io.StringIO()
        with patch.object(dev, "workspace_identity", return_value=None), \
                patch.object(dev, "executable",
                             side_effect=lambda name: name), \
                patch.object(dev.shutil, "which", side_effect=lambda name, **kw:
                             None if name == missing else "/locked/" + name), \
                patch.object(dev.subprocess, "run", side_effect=probe), \
                patch.object(dev, "maintenance_checks",
                             return_value=0) as checks, \
                redirect_stdout(output), redirect_stderr(output):
            status = dev.main(["doctor", "--target", "contracts"])
        return status, output.getvalue(), checks

    def test_contract_profile_checks_all_compilers_once_without_docker(self):
        status, output, checks = self.doctor()
        self.assertEqual(status, 0, output)
        self.assertEqual([Path(command[0]).name for command in self.commands],
                         ["cmake", "ninja", "gcc", "g++", "arm-none-eabi-gcc"])
        checks.assert_called_once()
        self.assertFalse(any("docker" in word for command in self.commands
                             for word in command))

    def test_every_native_and_arm_compiler_is_required_before_maintenance(self):
        for name in ("gcc", "g++", "arm-none-eabi-gcc"):
            with self.subTest(missing=name):
                status, output, checks = self.doctor(missing=name)
                self.assertEqual(status, 2, output)
                self.assertIn("Missing " + name + " required for contracts",
                              output)
                checks.assert_not_called()

    def test_wrong_compiler_version_cannot_start_maintenance_or_suites(self):
        for name in ("gcc", "g++", "arm-none-eabi-gcc"):
            with self.subTest(wrong=name):
                status, output, checks = self.doctor(wrong=name)
                self.assertEqual(status, 2, output)
                self.assertIn(name + " differs from locked", output)
                checks.assert_not_called()


if __name__ == "__main__":
    unittest.main()
