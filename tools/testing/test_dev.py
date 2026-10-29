"""The development entry point must propagate real gate and tool failures."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.dev import dev
from tools.dev import qualification
sys.path.insert(0, str(dev.ROOT / "cmake/package"))
import package_source_sdk as source_sdk


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

    def test_workflow_doctor_does_not_require_cmake_or_sdk(self):
        with patch.object(dev, "execute", return_value=0) as execute, \
                patch.object(dev, "workspace_identity") as identity:
            self.assertEqual(dev.main(["doctor", "--target", "workflows"]), 0)
        identity.assert_not_called()
        self.assertEqual(execute.call_args.args[0][1:3],
                         ["scripts/ci/lint_workflows.py", "--doctor"])

    def test_doctor_rejects_python_without_stdlib_toml(self):
        with patch.object(dev.sys, "version_info", (3, 10, 14)), \
                patch.object(dev.subprocess, "run", side_effect=self.probe), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = dev.main(["doctor"])
        self.assertEqual(result, 2)

    def test_doctor_reports_python_and_locked_tool_versions(self):
        output = io.StringIO()
        with patch.object(dev.subprocess, "run", side_effect=self.probe), \
                patch.object(dev, "executable", side_effect=lambda name: name), \
                patch.object(dev, "execute", return_value=0), \
                redirect_stdout(output), redirect_stderr(io.StringIO()):
            result = dev.main(["doctor"])
        self.assertEqual(result, 0)
        self.assertIn("Python:", output.getvalue())
        self.assertIn("tomllib", output.getvalue())

    def sdk_fixture(self, directory):
        root = Path(directory) / "share/nexus/src"
        (root / "dependencies").mkdir(parents=True)
        shutil.copyfile(dev.ROOT / "dependencies/environment.lock.json",
                        root / "dependencies/environment.lock.json")
        (root / ".nexus-source-sdk.json").write_text("{}")
        return root

    def test_doctor_verifies_sdk_before_reporting_tool_only_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.sdk_fixture(directory)
            output = io.StringIO()
            with patch.object(dev, "ROOT", root), \
                    patch.object(source_sdk, "verify", return_value={
                        "source_revision": "a" * 40}) as verify, \
                    patch.object(dev, "executable",
                                 side_effect=lambda name: name), \
                    patch.object(dev.subprocess, "run",
                                 side_effect=self.probe), \
                    patch.object(dev, "execute", return_value=0) as execute, \
                    redirect_stdout(output), redirect_stderr(io.StringIO()):
                result = dev.main(["doctor"])
            self.assertEqual(result, 0)
            verify.assert_called_once_with(root)
            execute.assert_not_called()
            self.assertIn("Verified source SDK", output.getvalue())
            self.assertIn("governance materials are not packaged",
                          output.getvalue())

    def test_manifest_presence_cannot_bypass_full_sdk_verification(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.sdk_fixture(directory)
            output = io.StringIO()
            with patch.object(dev, "ROOT", root), \
                    patch.object(dev.subprocess, "run",
                                 side_effect=self.probe), \
                    patch.object(dev, "execute", return_value=0) as execute, \
                    redirect_stdout(io.StringIO()), redirect_stderr(output):
                result = dev.main(["doctor"])
            self.assertEqual(result, 2)
            execute.assert_not_called()
            self.assertIn("source SDK identity", output.getvalue())

    def test_check_explicitly_requires_development_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.sdk_fixture(directory)
            output = io.StringIO()
            with patch.object(dev, "ROOT", root), \
                    patch.object(source_sdk, "verify", return_value={
                        "source_revision": "a" * 40}), \
                    patch.object(dev, "execute", return_value=0) as execute, \
                    redirect_stdout(io.StringIO()), redirect_stderr(output):
                result = dev.main(["check"])
            self.assertEqual(result, 2)
            execute.assert_not_called()
            self.assertIn("development checkout", output.getvalue())


class QualificationDeadlineTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.environment = {key: value for key, value in os.environ.items()
                            if key != "NEXUS_COMMAND_TIMEOUT"}

    def plan(self, name="plan.json", *, first=900, second=1400):
        stages = [{"name": "contracts", "checker": "tool_tests", "args": [],
                   "timeout_s": first},
                  {"name": "verify", "checker": "candidate_verify",
                   "args": ["--candidate", "{output}/candidate/candidate.json"]}]
        if second is not None:
            stages[1]["timeout_s"] = second
        path = self.root / name
        path.write_text(json.dumps({"schema_version": 1, "stages": stages}))
        return path

    def forwarded(self, arguments, *, environment=None):
        with patch.object(dev, "ROOT", self.root), \
                patch.dict(os.environ, self.environment if environment is None
                           else environment, clear=True), \
                patch.object(dev, "execute", return_value=7) as execute:
            result = dev.main(arguments)
        self.assertEqual(result, 7)
        return execute.call_args.args

    def test_wrapper_covers_the_complete_declared_plan_budget(self):
        plan = self.plan()
        for action in ("qualify", "candidate"):
            with self.subTest(action=action):
                command, environment = self.forwarded(
                    [action, "--plan", str(plan), "--output", "result"])
                self.assertGreaterEqual(
                    dev.process.command_timeout(environment), 900 + 1400)
                self.assertGreater(dev.process.command_timeout(environment),
                                   1800)
                self.assertEqual(command[-4:],
                                 ["--plan", str(plan), "--output", "result"])
                self.assertEqual("--require-candidate" in command,
                                 action == "candidate")

    def test_omitted_stage_timeout_retains_the_owned_stage_default(self):
        plan = self.plan(second=None)
        _, environment = self.forwarded(["qualify", "--plan", str(plan)])
        self.assertGreaterEqual(dev.process.command_timeout(environment),
                                900 + 1800)

    def test_last_explicit_plan_argument_wins_with_relative_and_equal_forms(self):
        self.plan("first.json", first=100, second=100)
        self.plan("last.json", first=2400, second=1200)
        for arguments in (("--plan", "first.json", "--plan=last.json"),
                          ("--plan=first.json", "--plan", "last.json")):
            with self.subTest(arguments=arguments):
                _, environment = self.forwarded(["qualify", *arguments])
                self.assertGreaterEqual(dev.process.command_timeout(environment),
                                        2400 + 1200)

    def test_explicit_short_deadline_keeps_precedence_without_reading_plan(self):
        environment = self.environment | {"NEXUS_COMMAND_TIMEOUT": "3.5"}
        with patch.object(qualification, "validate_plan",
                          side_effect=AssertionError("plan must not be read")):
            _, forwarded = self.forwarded(
                ["candidate", "--plan", "absent.json"],
                environment=environment)
        self.assertEqual(forwarded["NEXUS_COMMAND_TIMEOUT"], "3.5")

    def test_invalid_explicit_deadline_reaches_the_owned_process_validator(self):
        environment = self.environment | {"NEXUS_COMMAND_TIMEOUT": "nan"}
        with patch.dict(os.environ, environment, clear=True), \
                patch.object(dev.process.subprocess, "Popen") as launch, \
                redirect_stdout(io.StringIO()), \
                redirect_stderr(io.StringIO()) as errors:
            result = dev.main(["qualify", "--plan", "absent.json"])
        self.assertEqual(result, 2)
        launch.assert_not_called()
        self.assertIn("NEXUS_COMMAND_TIMEOUT", errors.getvalue())

    def test_other_forwarded_actions_keep_their_existing_deadline(self):
        for action, arguments in (("prepare", ["--scope", "preset"]),
                                  ("cache", ["inspect"]),
                                  ("docs", ["check"])):
            with self.subTest(action=action):
                _, environment = self.forwarded([action, *arguments])
                self.assertNotIn("NEXUS_COMMAND_TIMEOUT", environment)
                self.assertEqual(dev.process.command_timeout(environment), 1800)

    def owned_cli(self, arguments):
        def forward(command, environment):
            del environment
            try:
                return qualification.main(command[2:])
            except SystemExit as error:
                return error.code

        with patch.dict(os.environ, self.environment, clear=True), \
                patch.object(dev, "execute", side_effect=forward), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            try:
                return dev.main(arguments)
            except SystemExit as error:
                return error.code

    def test_missing_and_invalid_plans_keep_original_cli_rejection(self):
        broken = self.root / "broken.json"
        broken.write_text("{not json")
        invalid = self.root / "invalid.json"
        invalid.write_text('{"schema_version": 1, "stages": []}')
        for arguments in (("--plan", str(self.root / "absent.json")),
                          ("--plan", str(broken)),
                          ("--plan=" + str(invalid),), (), ("--plan",)):
            with self.subTest(arguments=arguments):
                output = self.root / "not-executed"
                self.assertNotEqual(self.owned_cli(
                    ["qualify", *arguments, "--output", str(output)]), 0)
                self.assertFalse(output.exists())

    def test_help_does_not_read_plan_and_uses_original_cli(self):
        with patch.object(qualification, "validate_plan",
                          side_effect=AssertionError("help must not read plan")):
            self.assertEqual(self.owned_cli(
                ["qualify", "--plan", str(self.root / "absent.json"),
                 "--help"]), 0)


if __name__ == "__main__":
    unittest.main()
