"""One build precedes complete tool domains and fresh GoogleTest execution."""

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
from tools.configure import authored
from tools.testing.gate_fixture import GateFixture


class TDDToolPreflightTests(unittest.TestCase):
    def setUp(self):
        self.fixture = GateFixture()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def command(self, target):
        return [sys.executable, "tools/dev/dev.py", "doctor", "--target", target]

    def test_native_gate_configures_and_builds_the_frozen_inputs_once(self):
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        status, output = self.fixture.run_gate(execute=execute)
        self.assertEqual(status, 0, output)
        configure = [sys.executable, "tools/dev/dev.py", "configure",
                     "--preset", "native-debug"]
        build = [sys.executable, "tools/dev/dev.py", "build", "--preset",
                 "native-debug", "--target", "nexus_google_contracts"]
        self.assertEqual(commands.count(configure), 1)
        self.assertEqual(commands.count(build), 1)
        self.assertEqual(commands[:3], [self.command("contracts"), configure, build])
        first_suite = next(i for i, command in enumerate(commands)
                           if "--suite" in command)
        self.assertGreater(first_suite, commands.index(build))
        self.assertEqual([command[command.index("--suite") + 1]
                          for command in commands if "--suite" in command],
                         ["testing", "configure", "ci", "workflows"])

    def test_failed_preflight_never_runs_tools_or_reuses_previous_success(self):
        for action in ("doctor", "configure", "build"):
            with self.subTest(action=action):
                directory = self.fixture.build / "tdd"
                directory.mkdir(exist_ok=True)
                summary = directory / "execution.json"
                summary.write_text('{"previous": "unrelated success"}')
                commands = []

                def execute(command, environment, log):
                    commands.append(command)
                    if command[2] == action:
                        raise ValueError("current preflight failed")
                    return self.fixture.execute(command, environment, log)

                status, output = self.fixture.run_gate(execute=execute)
                self.assertEqual(status, 1, output)
                self.assertIn("current preflight failed", output)
                self.assertFalse(any("--suite" in c for c in commands))
                self.assertFalse(summary.exists())

    def test_documentation_only_staged_change_does_not_probe_compilers(self):
        with patch.object(tdd_gate, "environment", return_value={}), \
                patch.object(tdd_gate, "staged_paths", return_value=["README.md"]), \
                patch.object(tdd_gate, "execute") as execute, \
                redirect_stdout(io.StringIO()):
            self.assertEqual(tdd_gate.main(["--staged"]), 0)
        execute.assert_not_called()


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

    def test_stale_editor_schema_stops_before_compilers_or_long_suites(self):
        with patch.object(authored, "editor_schema", return_value={"stale": True}):
            status, output, checks = self.doctor()
        self.assertEqual(status, 2, output)
        self.assertIn("editor schema", output)
        self.assertEqual(self.commands, [])
        checks.assert_not_called()

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
