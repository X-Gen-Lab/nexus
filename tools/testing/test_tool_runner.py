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
    def copy_cli(self, root):
        for name in ("testing/run_tool_tests.py", "testing/module_workers.py",
                     "dev/process.py", "dev/build_lock.py"):
            target = root / "tools" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "tools" / name, target)
        return root / "tools/testing/run_tool_tests.py"

    def test_module_paths_reject_globs_and_case_selections(self):
        for name in ("tools/testing/test_[a].py", "tools/testing/test_a?.py",
                     "tools/testing/test_a*.py", "tools/testing/test_a.py::Case"):
            with self.subTest(path=name):
                self.assertIsNone(run_tool_tests.module_domain(name))

    def test_commit_files_run_the_complete_module_without_other_modules(self):
        with tempfile.TemporaryDirectory(prefix="commit tools ") as name:
            root = Path(name)
            directory = root / "tools/testing"
            directory.mkdir(parents=True)
            selected = directory / "test_selected.py"
            selected.write_text(
                "import unittest\nclass Selected(unittest.TestCase):\n"
                "    def test_first(self): pass\n"
                "    def test_second(self): pass\n")
            (directory / "test_unselected.py").write_text(
                "raise RuntimeError('unselected module must not load')\n")
            runner = self.copy_cli(root)
            report = root / "commit.json"
            command = [sys.executable, "-B", str(runner), "--suite", "testing",
                       "--files", "tools/testing/test_selected.py",
                       "--report", str(report)]
            result = subprocess.run(command, capture_output=True, text=True,
                                    timeout=30, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            record = json.loads(report.read_text())
            self.assertEqual(record["executed"], 2)
            self.assertEqual(record["scope"], "commit-tools")
            self.assertFalse(record["qualification"])
            self.assertEqual(record["files"], ["tools/testing/test_selected.py"])
            (directory / "test_glob_a.py").write_text(selected.read_text())
            (directory / "test_glob_[a].py").write_text(
                "raise RuntimeError('literal glob module must not execute')\n")
            invalid = list(command)
            invalid[invalid.index("--files") + 1] = "tools/testing/test_glob_[a].py"
            result = subprocess.run(invalid, capture_output=True, text=True,
                                    timeout=30, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(report.exists())
            selected.write_text(selected.read_text().replace(
                "def test_second(self): pass", "def test_second(self): self.fail('changed')"))
            result = subprocess.run(command, capture_output=True, text=True,
                                    timeout=30, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(report.exists())

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
            runner = self.copy_cli(root)
            report = root / "execution.json"
            report.write_text('{"stale": true}\n')
            command = [sys.executable, "-B", str(runner), "--suite",
                       "freertos", "--report", str(report)]
            completed = subprocess.run(command, capture_output=True,
                                       text=True, timeout=30, check=False)
            self.assertEqual(completed.returncode, 0,
                             completed.stdout + completed.stderr)
            record = json.loads(report.read_text())
            self.assertEqual(record.pop("workers"), 1)
            self.assertEqual(set(record.pop("modules")),
                             {"os/freertos/test_fixture.py"})
            slowest = record.pop("slowest_tests")
            self.assertEqual(len(slowest), 1)
            self.assertIn("test_complete", slowest[0]["test"])
            self.assertGreaterEqual(slowest[0]["duration_seconds"], 0)
            self.assertEqual(record, {
                "suite": "freertos", "discovered": 1, "executed": 1,
                "skipped": 0, "failures": 0, "errors": 0,
                "expected_failures": 0, "unexpected_successes": 0,
                "status": "passed", "scope": "full-tool-domain",
                "qualification": False})

    def test_ci_only_domain_skip_cannot_reuse_a_previous_success(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            tests = root / "os/freertos"
            tests.mkdir(parents=True)
            (tests / "test_fixture.py").write_text(
                "import unittest\n"
                "class Domain(unittest.TestCase):\n"
                "    @unittest.skip('fixture')\n"
                "    def test_skipped(self): pass\n")
            runner = self.copy_cli(root)
            report = root / "execution.json"
            report.write_text('{"status": "passed"}\n')
            result = subprocess.run(
                [sys.executable, "-B", str(runner), "--suite", "freertos",
                 "--report", str(report)], capture_output=True, text=True,
                timeout=30, check=False)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
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

    def test_slowest_cases_use_actual_result_durations_and_keep_only_ten(self):
        cases = [unittest.FunctionTestCase(lambda: None,
                                           description=f"case-{index}")
                 for index in range(12)]
        times = [value for index in range(12) for value in (0, index + 1)]
        with patch.object(run_tool_tests, "perf_counter", side_effect=times,
                          create=True):
            record = self.run_fixture(unittest.TestSuite(cases))
        self.assertEqual([item["duration_seconds"]
                          for item in record["slowest_tests"]],
                         list(range(12, 2, -1)))
        self.assertTrue(all(item["test"] for item in record["slowest_tests"]))

    def test_class_setup_error_retains_empty_durations_and_actual_failure(self):
        class BrokenSetup(unittest.TestCase):
            @classmethod
            def setUpClass(cls):
                raise RuntimeError("actual class setup failed")

            def test_unreached(self):
                self.fail("class setup failed before this case")

        with self.assertRaises(ValueError) as caught:
            self.run_fixture(unittest.TestSuite([BrokenSetup("test_unreached")]))
        record = json.loads(str(caught.exception).split(": ", 1)[1])
        self.assertEqual(record["executed"], 0)
        self.assertEqual(record["errors"], 1)
        self.assertEqual(record["slowest_tests"], [])

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




class ChangedDomainTests(unittest.TestCase):
    def test_support_changes_select_the_complete_evidence_reader_contracts(self):
        self.assertEqual(run_tool_tests.commit_tool_plan([
            "tools/maintenance/capabilities.py"]), {"testing": (
                "tools/testing/test_maintenance.py",
                "tools/testing/test_support_selection.py")})

    def test_maintained_aliases_reference_existing_sources_and_complete_modules(self):
        for source, modules in run_tool_tests.COMMIT_ALIASES.items():
            with self.subTest(source=source):
                self.assertTrue((ROOT / source).is_file(), source)
                self.assertTrue(modules, source)
                for module in modules:
                    self.assertIsNotNone(run_tool_tests.module_domain(module), module)
                    self.assertTrue((ROOT / module).is_file(), module)

    def test_commit_message_checker_selects_its_complete_test_module(self):
        self.assertEqual(run_tool_tests.commit_tool_plan([
            "scripts/ci/check_commit_message.py",
            "scripts/ci/test_commit_message.py"]),
            {"ci": ("scripts/ci/test_commit_message.py",)})

    def test_coupled_assembly_selects_complete_ownership_and_ir_modules(self):
        plan = run_tool_tests.commit_tool_plan([
            "tools/configure/coupled_assembly.py"])
        self.assertIsNotNone(plan.get("configure"))
        self.assertTrue({
            "tools/configure/tests/test_power_stage_domains.py",
            "tools/configure/tests/test_controller_integration.py",
            "tools/configure/tests/test_static_ir.py",
            "tools/configure/tests/test_platform_lifecycle.py",
            "tools/configure/tests/test_factory_features.py",
            "tools/configure/tests/test_runtime.py",
        }.issubset(plan["configure"]))

    def test_preset_helper_selects_its_consumer_and_build_contracts(self):
        self.assertEqual(run_tool_tests.commit_tool_plan([
            "tools/dev/presets.py"]), {"testing": (
                "tools/testing/test_build_workflow.py",
                "tools/testing/test_sdk_verification_call.py",
                "tools/testing/test_tdd_gate.py")})

    def test_commit_plan_selects_modules_for_build_and_policy_changes(self):
        plan = run_tool_tests.commit_tool_plan([
            "AGENTS.md", ".pre-commit-config.yaml", ".gersemirc",
            "dependencies/environment.lock.json",
            "dependencies/python-tools-linux-cp312.lock.json",
            "cmake/platform/SDK.cmake", "tools/dev/timings.py",
            "tools/testing/test_dev_timings.py"])
        self.assertTrue(plan)
        self.assertTrue(all(files for files in plan.values()))
        self.assertIn("tools/testing/test_tool_runner.py", plan["testing"])
        self.assertIn("tools/testing/test_dev_timings.py", plan["testing"])
        self.assertIn("tools/testing/test_cmake_helpers.py", plan["testing"])
        self.assertIn("scripts/ci/test_install_dev_tools.py", plan["ci"])
        self.assertLess(sum(map(len, plan.values())), 20)

    def test_commit_facts_use_selected_assemblies_and_resource_contracts(self):
        plan = run_tool_tests.commit_tool_plan([
            "io/include/nexus/io/gpio.h", "soc/stm32/stm32f4/facts/devices.json",
            "boards/stm32f407zg_qiming_v41/board.json"])
        self.assertEqual(plan["testing"], ("tools/testing/test_build_impact.py",))
        self.assertIn("tools/configure/tests/test_resource_graph.py", plan["configure"])

    def test_commit_unknown_and_vendor_lock_changes_keep_full_domains(self):
        for name in ("future/build-policy.bin", "tools/future_policy.py",
                     "future/custom.cmake", "future/dev.py", "cmake/future-policy.cmake",
                     "vendors/library/CMakeLists.txt",
                     ".github/workflows/future-input.dat",
                     "dependencies/stm32f4-hal.lock.json"):
            with self.subTest(path=name):
                plan = run_tool_tests.commit_tool_plan([name])
                self.assertTrue(plan)
                self.assertTrue(all(files is None for files in plan.values()))

    def test_stale_cross_domain_alias_keeps_its_declared_full_domain(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            source = root / "tools/measurement/resources.py"
            source.parent.mkdir(parents=True)
            source.write_text("# Resource parser fixture.\n")
            for deleted_source in (False, True):
                with self.subTest(deleted_source=deleted_source):
                    if deleted_source:
                        source.unlink()
                    with patch.object(run_tool_tests, "ROOT", root):
                        self.assertEqual(run_tool_tests.commit_tool_plan([
                            "tools/measurement/resources.py"]),
                            {"testing": None, "contracts": None})

    def test_unknown_namespace_does_not_borrow_another_tools_named_test(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            source = root / "tools/future/dev.py"
            source.parent.mkdir(parents=True)
            source.write_text("# Unreviewed tool namespace.\n")
            tests = root / "tools/testing"
            tests.mkdir(parents=True)
            (tests / "test_dev.py").write_text("# Covers tools/dev/dev.py.\n")
            with patch.object(run_tool_tests, "ROOT", root):
                self.assertEqual(run_tool_tests.commit_tool_plan([
                    "tools/future/dev.py"]), {"testing": None})

    def test_unknown_existing_test_json_keeps_full_domain(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            source = root / "tests/future.json"
            source.parent.mkdir(parents=True)
            source.write_text("{}\n")
            for name in run_tool_tests.FACT_TESTS:
                module = root / name
                module.parent.mkdir(parents=True, exist_ok=True)
                module.write_text("# Maintained module fixture.\n")
            with patch.object(run_tool_tests, "ROOT", root):
                self.assertEqual(run_tool_tests.commit_tool_plan([
                    "tests/future.json"]), {"testing": None, "configure": None})

    def test_external_provider_descriptor_uses_its_package_contract(self):
        plan = run_tool_tests.commit_tool_plan([
            "tests/support/external_provider/provider/nexus-soc.json"])
        self.assertEqual(plan, {"configure": (
            "tools/configure/tests/test_external_package.py",)})

    def test_board_wiring_selects_its_declared_electrical_contract(self):
        contract = "tools/configure/tests/test_qiming_board_facts.py"
        for board in ("stm32f407zg_qiming_core", "stm32f407zg_qiming_v41",
                      "stm32f407ve_sky_qingchun"):
            with self.subTest(board=board):
                plan = run_tool_tests.commit_tool_plan([f"boards/{board}/board.json"])
                self.assertEqual(plan["testing"], ("tools/testing/test_build_impact.py",))
                self.assertEqual(contract in plan["configure"], "qiming" in board)
                self.assertNotIn(None, plan.values())

    def test_build_helpers_run_their_actual_firmware_and_workflow_contracts(self):
        budgets = {"tests/contracts/test_build_artifacts.py",
                   "tests/contracts/test_measurement_resources.py"}
        cases = {
            "tools/measurement/resources.py": ("contracts", budgets),
            "tools/measurement/build_resources.py": ("contracts", budgets),
            "tools/measurement/elf.py": ("contracts", budgets | {
                "tests/contracts/test_image_artifacts.py"}),
            "scripts/ci/reference_matrix.py": ("workflows", {
                ".github/workflows/test_platform_ci.py"}),
            "tools/testing/arm_links.py": ("testing", {
                "tools/testing/test_affected_arm_links.py", "tools/testing/test_tdd_gate.py"}),
        }
        for path, (domain, modules) in cases.items():
            with self.subTest(path=path):
                self.assertEqual(run_tool_tests.commit_tool_plan([path]),
                                 {domain: tuple(sorted(modules))})

    def test_sdk_device_fixture_avoids_unrelated_global_checks(self):
        plan = run_tool_tests.commit_tool_plan([
            "cmake/package/verification/device_tmp117/f407.toml",
            "components/devices/tmp117/src/i2c_transport.c"])
        self.assertEqual(plan, {"testing": ("tools/testing/test_sdk_device_image_policy.py",)})

    def test_sdk_code_and_known_fixture_inputs_select_both_complete_domains(self):
        for name in ("cmake/package/package_source_sdk.py",
                     "cmake/package/verify_consumers.py",
                     "cmake/package/verification/example/main.c",
                     "cmake/package/verification/example/public.h",
                     "cmake/package/verification/example/assembly.toml",
                     "cmake/package/verification/example/board.json"):
            with self.subTest(path=name):
                self.assertEqual(run_tool_tests.changed_suites([name]),
                                 ("testing", "configure"))

    def test_sdk_fixture_build_rules_and_unknown_inputs_keep_all_domains(self):
        for name in ("cmake/package/verification/example/CMakeLists.txt",
                     "cmake/package/verification/example/custom.cmake",
                     "cmake/package/verification/example/helper.py",
                     "cmake/package/verification/example/data.unknown",
                     "cmake/package/future-policy.py",
                     "cmake/platform/Firmware.cmake",
                     "cmake/toolchains/arm-gcc.cmake",
                     "cmake/presets/native.json",
                     "cmake/testing/GoogleTest.cmake",
                     "tools/testing/run_tool_tests.py",
                     "tools/dev/change_paths.py",
                     "scripts/ci/tdd_gate.py"):
            with self.subTest(path=name):
                self.assertEqual(run_tool_tests.changed_suites([name]),
                                 tuple(run_tool_tests.SUITES))

    def test_deleted_and_renamed_fixture_paths_retain_their_domain_union(self):
        self.assertEqual(run_tool_tests.changed_suites([
            "cmake/package/verification/removed/consumer.c",
            "cmake/package/verification/new/consumer.c"]),
            ("testing", "configure"))
        self.assertEqual(run_tool_tests.changed_suites([
            "cmake/package/verification/old/consumer.c",
            "future-location/consumer.c"]), tuple(run_tool_tests.SUITES))
        self.assertEqual(run_tool_tests.changed_suites([
            "cmake/package/verify_consumers.py", ".github/workflows/build.yml"]),
            tuple(run_tool_tests.SUITES))

    def test_documentation_does_not_select_behavioral_tool_domains(self):
        self.assertEqual(run_tool_tests.changed_suites([
            ".github/workflows/README.md", "tools/configure/README.md",
            "cmake/package/README.md"]), ())

    def test_workflow_readme_does_not_expand_a_production_change(self):
        self.assertEqual(run_tool_tests.changed_suites([
            ".github/workflows/README.md", "core/src/time.c"]),
            ())

    def test_firmware_sources_do_not_select_tool_domains(self):
        for name in ("core/src/time.c", "arch/arm/cortex_m/interrupt.S",
                     "os/freertos/mutex.c", "io/src/gpio.c",
                     "soc/stm32/stm32f4/drivers/uart/uart_hal.c",
                     "boards/stm32f407zg-qiming-v41/board.c",
                     "components/services/storage/storage.c",
                     "tests/contracts/test_gpio.cpp"):
            with self.subTest(path=name):
                self.assertEqual(run_tool_tests.changed_suites([name]), ())

    def test_headers_and_package_data_keep_configuration_boundaries(self):
        for name in ("core/include/nexus/core/result.h",
                     "io/include/nexus/io/gpio.h",
                     "soc/stm32/stm32f4/drivers/uart/binding.json",
                     "boards/stm32f407zg-qiming-v41/board.json",
                     "tests/contracts/uart/assembly.toml"):
            with self.subTest(path=name):
                self.assertEqual(run_tool_tests.changed_suites([name]),
                                 ("testing", "configure"))

    def test_unknown_firmware_and_external_inputs_keep_all_domains(self):
        for name in ("soc/stm32/stm32f4/future-policy.py",
                     "core/new-build-input.dat", "new-provider/driver.c"):
            with self.subTest(path=name):
                self.assertEqual(run_tool_tests.changed_suites([name]),
                                 tuple(run_tool_tests.SUITES))

    def test_tool_changes_and_mixed_paths_keep_the_affected_domain_union(self):
        self.assertEqual(run_tool_tests.changed_suites([
            "core/src/time.c", "tools/dev/presets.py"]), ("testing",))
        self.assertEqual(run_tool_tests.changed_suites([
            "io/src/gpio.c", "tools/configure/bindings.py"]),
            ("testing", "configure"))

    def test_complete_domains_follow_actual_changes_and_deletions(self):
        self.assertEqual(run_tool_tests.changed_suites(["soc/stm32/driver.c"]),
                         ())
        self.assertEqual(run_tool_tests.changed_suites([
            "tools/configure/removed.py", "README.md"]),
            ("testing", "configure"))
        self.assertEqual(run_tool_tests.changed_suites(["scripts/validation/junit.py"]),
                         tuple(run_tool_tests.SUITES))
        self.assertEqual(run_tool_tests.changed_suites(["new-build-entry.py"]),
                         tuple(run_tool_tests.SUITES))

    def test_maintenance_policy_is_a_behavioral_input(self):
        from tools.dev.change_paths import behavioral
        self.assertTrue(behavioral("AGENTS.md"))


if __name__ == "__main__":
    unittest.main()
