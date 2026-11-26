"""Real path boundaries in the existing gate, without running provider tests."""

import json
import unittest
from unittest.mock import patch

from scripts.ci import tdd_gate
from tools.testing.support import gate_fixture as integration


class TDDBinaryPlanBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.fixture = integration.GateFixture()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def reject_additional_binary(self, path):
        plan_path = self.fixture.build / "google-contracts.json"
        plan = json.loads(plan_path.read_text())
        plan["executables"].append(str(path))
        plan_path.write_text(json.dumps(plan))
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        status, output = self.fixture.run_gate(execute=execute)
        self.assertEqual(status, 1, output)
        self.assertFalse(any(command[0] == str(path) for command in commands))
        self.assertFalse((self.fixture.build / "tdd/execution.json").exists())

    def test_uncatalogued_parent_escape_rejected_before_subprocess(self):
        foreign = self.fixture.root / "foreign/nexus_extra_contract"
        foreign.parent.mkdir()
        foreign.write_bytes(self.fixture.binary.read_bytes())
        escaped = self.fixture.build / "../../foreign/nexus_extra_contract"
        self.assertTrue(escaped.is_file())
        self.assertFalse(escaped.is_symlink())
        self.assertFalse(escaped.resolve().is_relative_to(self.fixture.build))
        self.reject_additional_binary(escaped)

    def test_uncatalogued_file_symlink_rejected_before_subprocess(self):
        alias = self.fixture.binary.parent / "nexus_extra_contract"
        alias.symlink_to(self.fixture.binary)
        self.assertTrue(alias.is_file())
        self.assertTrue(alias.is_symlink())
        self.reject_additional_binary(alias)

    def test_uncatalogued_parent_symlink_rejected_before_subprocess(self):
        directory = self.fixture.build / "alias-bin"
        directory.symlink_to(self.fixture.binary.parent, target_is_directory=True)
        alias = directory / self.fixture.binary.name
        self.assertTrue(alias.is_file())
        self.assertFalse(alias.is_symlink())
        self.assertTrue(directory.is_symlink())
        self.reject_additional_binary(alias)

    def test_firmware_change_runs_full_google_without_tool_domains(self):
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        status, output = self.fixture.run_gate(
            execute=execute, arguments=["core/src/time.c"])
        self.assertEqual(status, 0, output)
        self.assertFalse(any("--suite" in command for command in commands))
        self.assertEqual(commands[0][-2:], ["--target", "native"])
        self.assertIn("discovery", self.fixture.events)
        self.assertIn("execution", self.fixture.events)
        record = json.loads((self.fixture.build / "tdd/execution.json").read_text())
        self.assertEqual(record["passed"], 1)
        self.assertEqual(record["tool_suites"], [])

    def test_gate_executes_c_runtime_and_google_in_one_ctest_run(self):
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        status, output = self.fixture.run_gate(
            execute=execute, arguments=["core/src/time.c"])
        self.assertEqual(status, 0, output)
        record = json.loads((self.fixture.build / "tdd/execution.json").read_text())
        self.assertEqual(record.get("ctest", {}).get("passed"), 2)
        self.assertEqual(sum("--output-junit" in command
                             for command in commands), 1)
        self.assertFalse(any("--gtest_output=" in str(argument)
                             for command in commands for argument in command))
        build = next(command for command in commands if "build" in command)
        self.assertNotIn("nexus_google_contracts", build)

    def test_equal_google_count_with_changed_case_identity_is_rejected(self):
        self.fixture.executed_case = "WrongIdentity"
        status, output = self.fixture.run_gate(arguments=["core/src/time.c"])
        self.assertEqual(status, 1, output)
        self.assertFalse((self.fixture.build / "tdd/execution.json").exists())

    def test_warm_tree_skips_configure_but_runs_contracts_again(self):
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            if "configure" in command:
                entry = "CMAKE_HOME_DIRECTORY:INTERNAL=" + str(self.fixture.root) + "\n"
                tool = self.fixture.root / "compiler-wrapper"
                if tool.exists():
                    entry += f"CMAKE_C_COMPILER:FILEPATH={tool}\n"
                (self.fixture.build / "CMakeCache.txt").write_bytes(
                    entry.replace("\n", "\r\n").encode())
                (self.fixture.build / "build.ninja").write_text("# fixture\n")
            return self.fixture.execute(command, environment, log)

        def run():
            commands.clear()
            status, output = self.fixture.run_gate(
                execute=execute, arguments=["core/src/time.c"])
            self.assertEqual(status, 0, output)
            self.assertTrue(any("--gtest_list_tests" in cmd for cmd in commands))
            return sum("configure" in cmd for cmd in commands)

        self.assertEqual(run(), 1)
        self.assertEqual(run(), 0)
        cache = self.fixture.build / "CMakeCache.txt"
        tool = self.fixture.root / "compiler-wrapper"
        tool.write_text("first implementation")
        with cache.open("a") as output:
            output.write(f"CMAKE_C_COMPILER:FILEPATH={tool}\n")
        self.assertEqual(run(), 1)
        self.assertEqual(run(), 0)
        tool.write_text("different implementation at the same path")
        self.assertEqual(run(), 1)
        (self.fixture.build / "build.ninja").unlink()
        self.assertEqual(run(), 1)

    def test_foreign_build_tree_is_rejected_before_cmake_fresh(self):
        cache = self.fixture.build / "CMakeCache.txt"
        original = "CMAKE_HOME_DIRECTORY:INTERNAL=/different/source\n"
        cache.write_text(original)
        commands = []

        def execute(cmd, env, log):
            commands.append(cmd)
            return self.fixture.execute(cmd, env, log)

        status, detail = self.fixture.run_gate(execute=execute)
        self.assertEqual(status, 1, detail)
        self.assertFalse(any("configure" in cmd for cmd in commands))
        self.assertEqual(cache.read_text(), original)

    def test_explicit_all_keeps_every_tool_domain(self):
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        status, output = self.fixture.run_gate(
            execute=execute, arguments=["--all", "--tools", "none"])
        self.assertEqual(status, 0, output)
        self.assertEqual([command[command.index("--suite") + 1]
                          for command in commands if "--suite" in command],
                         list(reversed(tdd_gate.TOOL_SUITES)))

    def test_mixed_commit_plan_runs_tool_domains_before_extra_domains(self):
        plan = dict.fromkeys((*tdd_gate.TOOL_SUITES, "integration", "freertos"))
        plan["configure"] = ("tools/configure/tests/resources/peripherals/test_configure.py",
                             "tools/configure/tests/resources/peripherals/test_device_graph_stage.py")
        plan["integration"] = ("tests/integration/sdk/test_source_sdk.py",)
        plan["freertos"] = ("os/freertos/test_m7_integer_overlay.py",)
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        with patch.object(tdd_gate, "staged_paths", return_value=["AGENTS.md"]), \
                patch.object(tdd_gate, "check_staged_snapshot"), \
                patch.object(tdd_gate, "staged_tree", return_value="frozen"), \
                patch.object(tdd_gate, "commit_tool_plan", return_value=plan):
            status, output = self.fixture.run_gate(
                execute=execute, arguments=["--staged"])
        self.assertEqual(status, 0, output)
        tools = [command for command in commands if "--suite" in command]
        self.assertEqual([command[command.index("--suite") + 1]
                          for command in tools],
                         [*reversed(tdd_gate.TOOL_SUITES), "integration", "freertos"])
        for command in tools:
            files = plan[command[command.index("--suite") + 1]]
            if files is None:
                self.assertNotIn("--files", command)
            else:
                self.assertEqual(command[command.index("--files") + 1:],
                                 list(files))
        self.assertIn("discovery", self.fixture.events)
        self.assertIn("execution", self.fixture.events)
        record = json.loads((self.fixture.build / "tdd/execution.json").read_text())
        self.assertEqual(record["scope"], "commit-tools")
        self.assertFalse(record["qualification"])
        self.assertEqual(len(record["tool_suites"]), len(plan))
        self.assertEqual(record["passed"], 1)

    def test_early_tool_failure_does_not_start_extra_domains(self):
        plan = dict.fromkeys((*tdd_gate.TOOL_SUITES, "integration", "freertos"))
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            if ("--suite" in command and
                    command[command.index("--suite") + 1] == "configure"):
                raise ValueError("configuration contract failed")
            return self.fixture.execute(command, environment, log)

        with patch.object(tdd_gate, "staged_paths", return_value=["AGENTS.md"]), \
                patch.object(tdd_gate, "check_staged_snapshot"), \
                patch.object(tdd_gate, "staged_tree", return_value="frozen"), \
                patch.object(tdd_gate, "commit_tool_plan", return_value=plan):
            status, output = self.fixture.run_gate(
                execute=execute, arguments=["--staged"])
        self.assertEqual(status, 1, output)
        self.assertEqual([command[command.index("--suite") + 1]
                          for command in commands if "--suite" in command],
                         [name for name in reversed(tdd_gate.TOOL_SUITES)
                          if name != "testing"])
        self.assertFalse((self.fixture.build / "tdd/execution.json").exists())
        self.assertNotIn("discovery", self.fixture.events)
        self.assertNotIn("execution", self.fixture.events)

    def test_staged_build_maintenance_runs_modules_and_full_google(self):
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        with patch.object(tdd_gate, "staged_paths", return_value=[
                "AGENTS.md", "cmake/platform/SDK.cmake", ".gersemirc"]), \
                patch.object(tdd_gate, "check_staged_snapshot"), \
                patch.object(tdd_gate, "staged_tree", return_value="frozen"):
            status, output = self.fixture.run_gate(
                execute=execute, arguments=["--staged"])
        self.assertEqual(status, 0, output)
        tools = [command for command in commands if "--suite" in command]
        self.assertTrue(tools)
        self.assertTrue(all("--files" in command for command in tools))
        self.assertIn("discovery", self.fixture.events)
        self.assertIn("execution", self.fixture.events)
        record = json.loads((self.fixture.build / "tdd/execution.json").read_text())
        self.assertEqual(record["scope"], "commit-tools")
        self.assertFalse(record["qualification"])
        self.assertTrue(all("-commit.json" in command[
            command.index("--report") + 1] for command in tools))

    def test_staged_unknown_path_keeps_full_domain_execution(self):
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        with patch.object(tdd_gate, "staged_paths", return_value=["future.bin"]), \
                patch.object(tdd_gate, "check_staged_snapshot"), \
                patch.object(tdd_gate, "staged_tree", return_value="frozen"):
            status, output = self.fixture.run_gate(
                execute=execute, arguments=["--staged"])
        self.assertEqual(status, 0, output)
        tools = [command for command in commands if "--suite" in command]
        self.assertEqual(len(tools), len(tdd_gate.TOOL_SUITES))
        self.assertTrue(all("--files" not in command for command in tools))

    def test_explicit_all_tools_runs_even_for_staged_documentation(self):
        commands = []

        def execute(command, environment, log):
            commands.append(command)
            return self.fixture.execute(command, environment, log)

        with patch.object(tdd_gate, "staged_paths", return_value=["README.md"]), \
                patch.object(tdd_gate, "check_staged_snapshot"), \
                patch.object(tdd_gate, "staged_tree", return_value="frozen"):
            status, output = self.fixture.run_gate(
                execute=execute, arguments=["--staged", "--tools", "all"])
        self.assertEqual(status, 0, output)
        tools = [command for command in commands if "--suite" in command]
        self.assertEqual(len(tools), len(tdd_gate.TOOL_SUITES))
        self.assertTrue(all("--files" not in command for command in tools))

    def test_arm_tool_domains_require_the_contracts_doctor(self):
        for name in ("tools/configure/tests/emission/test_incremental_consumer.py",
                     "tests/integration/resources/test_measurement_compare.py",
                     "os/freertos/test_m7_integer_overlay.py"):
            commands = []

            def execute(command, environment, log):
                commands.append(command)
                return self.fixture.execute(command, environment, log)

            with self.subTest(path=name), patch.object(
                    tdd_gate, "staged_paths", return_value=[name]), \
                    patch.object(tdd_gate, "check_staged_snapshot"), \
                    patch.object(tdd_gate, "staged_tree", return_value="frozen"):
                status, output = self.fixture.run_gate(
                    execute=execute, arguments=["--staged"])
                self.assertEqual(status, 0, output)
                self.assertEqual(commands[0][-2:], ["--target", "contracts"])


if __name__ == "__main__":
    unittest.main()
