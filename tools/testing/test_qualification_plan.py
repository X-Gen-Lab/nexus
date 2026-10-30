"""The maintained software plan executes complete Native and SDK boundaries."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from scripts.ci import provider_assemblies
from tools.dev import qualification
from tools.dev.qualification import CHECKERS, validate_plan
from tools.dev.presets import Presets
from tools.evidence.common import EvidenceError, load_json

ROOT = Path(__file__).resolve().parents[2]


class QualificationStageBudgetTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.environment = {key: value for key, value in os.environ.items()
                            if key != "NEXUS_COMMAND_TIMEOUT"}
        script = self.root / CHECKERS["configure"][0]
        script.parent.mkdir(parents=True)
        script.write_text(
            "import json, os, sys\n"
            "from pathlib import Path\n"
            f"sys.path.insert(0, {str(ROOT)!r})\n"
            "from tools.dev.process import run\n"
            "log = Path(sys.argv[-1])\n"
            "result = run([sys.executable, '-B', '-c', "
            "\"print('actual nested budget probe')\"], "
            "cwd=Path.cwd(), env=os.environ.copy(), log=log)\n"
            "log.with_suffix('.env.json').write_text(json.dumps({"
            "'budget': os.environ.get('NEXUS_COMMAND_TIMEOUT')}))\n"
            "raise SystemExit(result.returncode)\n")
        (self.root / ".gitignore").write_text("/runs/\n")
        environment = self.environment | {
            "GIT_AUTHOR_NAME": "Qualification fixture",
            "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
            "GIT_COMMITTER_NAME": "Qualification fixture",
            "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
        }
        for arguments in (("init", "--quiet"), ("add", "."),
                          ("commit", "--quiet", "-m", "initial fixture")):
            subprocess.run(["git", "-c", "maintenance.auto=false", "-c",
                            "gc.auto=0", "-C", str(self.root), *arguments],
                           env=environment, check=True, capture_output=True,
                           text=True, timeout=30)

    def execute(self, budgets, *, explicit=None, name="result"):
        stages = []
        for index, budget in enumerate(budgets):
            stage = {"name": "probe-" + str(index), "checker": "configure",
                     "args": ["{output}/probe-" + str(index) + ".nested.log"]}
            if budget is not None:
                stage["timeout_s"] = budget
            stages.append(stage)
        plan = {"schema_version": 1, "formal": True, "stages": stages}
        environment = dict(self.environment)
        if explicit is not None:
            environment["NEXUS_COMMAND_TIMEOUT"] = explicit
        output = self.root / "runs" / name
        with patch.dict(os.environ, environment, clear=True):
            report = qualification.execute(plan, self.root, output)
            self.assertEqual(dict(os.environ), environment)
        self.assertEqual(report["status"], "executed_passed")
        self.assertFalse(report["source_before"]["dirty"])
        self.assertEqual(report["source_before"], report["source_after"])
        observed = []
        for index, budget in enumerate(budgets):
            name = "probe-" + str(index)
            nested = load_json(output / (name + ".nested.command.json"))
            outer = load_json(output / (name + ".command.json"))
            self.assertEqual(nested["exit_code"], 0)
            self.assertEqual(nested["outcome"], "completed")
            self.assertEqual(outer["timeout_seconds"],
                             1800 if budget is None else budget)
            observed.append(nested["timeout_seconds"])
            if explicit is not None:
                received = load_json(output / (name + ".nested.env.json"))
                self.assertEqual(received["budget"], explicit)
        return observed

    def test_declared_stage_budget_reaches_the_real_nested_command(self):
        self.assertEqual(self.execute([7200.25]), [7200.25])

    def test_omitted_stage_budget_keeps_the_nested_default(self):
        self.assertEqual(self.execute([None]), [1800])

    def test_different_stage_budgets_do_not_leak_to_later_stages(self):
        self.assertEqual(self.execute([7.25, 13, None]), [7.25, 13, 1800])

    def test_explicit_lower_or_higher_caller_budget_keeps_precedence(self):
        for explicit in ("2.50", "14400"):
            with self.subTest(explicit=explicit):
                self.assertEqual(self.execute([30], explicit=explicit,
                                              name="explicit-" + explicit),
                                 [float(explicit)])


class QualityToolPreflightTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.started = self.root / "first-stage-started"
        self.quality_started = self.root / "quality-stage-started"
        for checker, marker in (("configure", self.started),
                                ("quality_tools", self.quality_started)):
            script = self.root / CHECKERS[checker][0]
            script.parent.mkdir(parents=True, exist_ok=True)
            script.write_text(
                "from pathlib import Path\n"
                f"Path({str(marker)!r}).write_text('started')\n")
        self.source = {"dirty": False, "commit": "isolated-checker-fixture"}

    def tool(self, name="analyzer", *, executable=True):
        path = self.bin / name
        path.write_text("#!/bin/sh\nexit 0\n")
        path.chmod(0o755 if executable else 0o644)
        return path

    def plan(self, arguments=None):
        stages = [{"name": "first", "checker": "configure", "args": []}]
        if arguments is not None:
            stages.append({"name": "late-quality", "checker": "quality_tools",
                           "args": ["tidy", *arguments]})
        return {"schema_version": 1, "stages": stages}

    def execute(self, arguments=None, *, path=None, extra_stages=()):
        plan = self.plan(arguments)
        plan["stages"].extend(extra_stages)
        with patch.dict(os.environ, {"PATH": str(self.bin) if path is None
                                     else path}), patch.object(
                qualification, "observe_git_source", return_value=self.source):
            return qualification.execute(plan, self.root, self.root / "result")

    def test_missing_late_tool_rejects_before_first_stage(self):
        caught = None
        try:
            self.execute(["--tool", "missing-analyzer"])
        except EvidenceError as error:
            caught = error
        self.assertFalse(self.started.exists(),
                         "missing late tool already executed the first checker")
        self.assertIsNotNone(caught, "missing analyzer was not rejected")
        self.assertIn("late-quality", str(caught))
        self.assertIn("missing-analyzer", str(caught))
        self.assertFalse(self.quality_started.exists())
        self.assertFalse((self.root / "result").exists())

    def test_bare_tools_use_the_execution_environment(self):
        selected = self.tool()
        report = self.execute(["--tool", selected.name])
        self.assertEqual(report["status"], "executed_passed")
        self.assertEqual([stage["name"] for stage in report["stages"]],
                         ["first", "late-quality"])
        self.assertTrue(self.started.exists())
        self.assertTrue(self.quality_started.exists())

    def test_absolute_executable_does_not_require_a_path_entry(self):
        selected = self.tool()
        report = self.execute(["--tool=" + str(selected)], path="")
        self.assertEqual(report["status"], "executed_passed")

    def test_relative_tool_paths_use_the_checker_working_directory(self):
        self.tool()
        report = self.execute(["--tool", "bin/analyzer"], path="")
        self.assertEqual(report["status"], "executed_passed")

    def test_relative_path_entries_use_the_checker_working_directory(self):
        self.tool()
        report = self.execute(["--tool=analyzer"], path="bin")
        self.assertEqual(report["status"], "executed_passed")

    def test_explicit_analysis_root_controls_relative_tool_selection(self):
        destination = self.root / "analysis-root" / "bin"
        destination.mkdir(parents=True)
        self.tool().replace(destination / "analyzer")
        report = self.execute(["--root=missing-root", "--root", "analysis-root",
                               "--tool", "bin/analyzer"], path="")
        self.assertEqual(report["status"], "executed_passed")

    def test_explicit_analysis_root_controls_relative_path_search(self):
        destination = self.root / "analysis-root" / "bin"
        destination.mkdir(parents=True)
        self.tool().replace(destination / "analyzer")
        report = self.execute(["--root=analysis-root", "--tool=analyzer"],
                              path="bin")
        self.assertEqual(report["status"], "executed_passed")

    def test_repeated_tool_selection_uses_the_last_spelling(self):
        selected = self.tool()
        report = self.execute(["--tool", "missing-analyzer",
                               "--tool=" + selected.name])
        self.assertEqual(report["status"], "executed_passed")

    def test_separated_selection_can_replace_an_earlier_equals_selection(self):
        selected = self.tool()
        report = self.execute(["--tool=missing-analyzer", "--tool",
                               selected.name])
        self.assertEqual(report["status"], "executed_passed")

    def test_later_missing_selection_overrides_an_existing_executable(self):
        selected = self.tool()
        with self.assertRaisesRegex(EvidenceError, "missing-analyzer"):
            self.execute(["--tool=" + selected.name,
                          "--tool", "missing-analyzer"])
        self.assertFalse(self.started.exists())

    def test_missing_empty_and_unknown_selectors_do_not_start_stages(self):
        for arguments in ([], ["--tool"], ["--tool="],
                          ["--tool", "--report", "unused"],
                          ["--analyzer=missing"],
                          ["--tool=missing", "--tool"]):
            with self.subTest(arguments=arguments):
                with self.assertRaisesRegex(EvidenceError, "late-quality"):
                    self.execute(arguments)
                self.assertFalse(self.started.exists())
        with self.assertRaisesRegex(EvidenceError, "invalid adapter argument"):
            self.execute(["--tool", ""])
        self.assertFalse(self.started.exists())

    def test_nonexecutable_file_and_directory_are_not_tools(self):
        self.tool("not-executable", executable=False)
        (self.bin / "directory").mkdir()
        for name in ("not-executable", "directory"):
            with self.subTest(tool=name):
                with self.assertRaisesRegex(EvidenceError, name):
                    self.execute(["--tool", name])
                self.assertFalse(self.started.exists())

    def test_missing_second_analyzer_prevents_all_stage_execution(self):
        selected = self.tool()
        stages = [{"name": "second-quality", "checker": "quality_tools",
                   "args": ["cppcheck", "--tool=missing-second"]}]
        with self.assertRaisesRegex(EvidenceError, "second-quality"):
            self.execute(["--tool", selected.name], extra_stages=stages)
        self.assertFalse(self.started.exists())
        self.assertFalse(self.quality_started.exists())

    def test_plan_without_analyzer_does_not_require_one(self):
        report = self.execute(path="")
        self.assertEqual(report["status"], "executed_passed")
        self.assertEqual(len(report["stages"]), 1)

    def test_presence_does_not_preapprove_actual_checker_success(self):
        selected = self.tool()
        script = self.root / CHECKERS["quality_tools"][0]
        with script.open("a") as stream:
            stream.write("raise SystemExit(7)\n")
        report = self.execute(["--tool", str(selected)])
        self.assertEqual(report["status"], "failed")
        self.assertEqual(report["stages"][-1]["exit_code"], 7)


class QualificationPlanTests(unittest.TestCase):
    def setUp(self):
        self.plan = load_json(ROOT / "tools/dev/plans/software.json")
        self.stages = self.plan["stages"]

    def test_formal_analyzers_have_explicit_bounded_parallelism(self):
        selected = [stage for stage in self.stages
                    if stage["checker"] == "quality_tools"]
        self.assertEqual({stage["name"] for stage in selected},
                         {"tidy", "cppcheck"})
        for stage in selected:
            with self.subTest(stage=stage["name"]):
                arguments = stage["args"]
                self.assertIn("--parallel", arguments)
                self.assertEqual(arguments[arguments.index("--parallel") + 1],
                                 "2")
                self.assertNotIn("--source", arguments)
                self.assertNotIn("--filter", arguments)
        validate_plan(self.plan)

    def test_analyzers_run_when_the_debug_compilation_inputs_are_ready(self):
        names = [stage["name"] for stage in self.stages]
        built = names.index("build-native-debug")
        contracts = names.index("native_contracts-native-debug")
        self.assertEqual(names[built + 1:contracts], ["tidy", "cppcheck"],
                         "Analyze the completed Debug inputs before later work")
        for stage in self.stages[built + 1:contracts]:
            arguments = stage["args"]
            self.assertEqual(arguments[arguments.index("--build-dir") + 1],
                             "{root}/build/native-debug")

    def test_analyzer_failure_stops_before_native_execution_and_later_builds(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tools = root / "bin"
            tools.mkdir()
            for name in ("clang-tidy", "cppcheck"):
                tool = tools / name
                tool.write_text("#!/bin/sh\nexit 0\n")
                tool.chmod(0o755)
            for script, *_ in CHECKERS.values():
                path = root / script
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("# Isolated external-checker fixture.\n")

            def run(argv, *, log, **kwargs):
                # Inject an external analyzer failure, without running a build.
                failed = Path(argv[2]).name == "quality_tools.py"
                log.write_text("analyzer failed\n" if failed else "passed\n")
                return subprocess.CompletedProcess(argv, 7 if failed else 0)

            with patch.dict(os.environ, {"PATH": str(tools)}), patch.object(
                    qualification, "observe_git_source",
                    return_value={"dirty": False, "commit": "plan-fixture"}), \
                    patch.object(qualification, "run_command", side_effect=run):
                report = qualification.execute(self.plan, root, root / "result")
        self.assertEqual(report["status"], "failed")
        self.assertEqual(report["stages"][-1]["name"], "tidy")
        self.assertEqual(report["stages"][-1]["exit_code"], 7)
        attempted = [stage["name"] for stage in report["stages"]]
        self.assertIn("build-native-debug", attempted)
        self.assertNotIn("native_contracts-native-debug", attempted)
        self.assertNotIn("configure-native-release", attempted)
        self.assertNotIn("source-sdk", attempted)

    def test_each_native_preset_configures_builds_all_and_runs_full_ctest(self):
        for preset in ("native-debug", "native-release", "native-asan",
                       "native-tsan"):
            with self.subTest(preset=preset):
                selected = [stage for stage in self.stages
                            if preset in stage["args"]]
                self.assertEqual([stage["checker"] for stage in selected],
                                 ["configure", "build", "native_contracts"])
                for stage in selected:
                    self.assertEqual(stage["args"], ["--preset", preset])
                positions = [self.stages.index(stage) for stage in selected]
                self.assertEqual(positions, sorted(positions))

    def test_source_sdk_illegal_input_boundaries_have_a_fixed_full_runner(self):
        self.assertEqual(CHECKERS.get("sdk_boundary_tests"),
                         ("tests/contracts/test_source_sdk.py",))
        selected = [stage for stage in self.stages
                    if stage["checker"] == "sdk_boundary_tests"]
        self.assertEqual(len(selected), 1)
        self.assertEqual(selected[0]["args"], [])
        self.assertTrue(any(stage["checker"] == "sdk_consumers"
                            for stage in self.stages))

    def test_sdk_boundary_script_bootstraps_without_import_environment(self):
        script, = CHECKERS["sdk_boundary_tests"]
        case = ("SourceSDKSelectionTests."
                "test_component_construction_helpers_are_required_sdk_payload")
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [sys.executable, "-I", "-B", str(ROOT / script), case],
                cwd=directory, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Ran 1 test", result.stderr)

    def test_all_four_tool_domains_share_the_fixed_complete_runner(self):
        selected = [stage for stage in self.stages
                    if stage["checker"] == "tool_tests"]
        self.assertEqual(CHECKERS["tool_tests"],
                         ("tools/testing/run_tool_tests.py",))
        self.assertEqual([stage["args"][1] for stage in selected],
                         ["testing", "configure", "ci", "workflows"])
        for stage in selected:
            self.assertEqual(stage["args"][0], "--suite")
            self.assertEqual(stage["args"][2], "--report")
            self.assertEqual(len(stage["args"]), 4)
            self.assertTrue(stage["args"][3].startswith("{output}/"))
        validate_plan(self.plan)

    def test_actual_arm_artifact_recovery_boundaries_have_a_fixed_full_runner(self):
        self.assertEqual(CHECKERS.get("build_artifact_tests"),
                         ("tests/contracts/test_build_artifacts.py",))
        selected = [stage for stage in self.stages
                    if stage["checker"] == "build_artifact_tests"]
        self.assertEqual(len(selected), 1)
        self.assertEqual(selected[0]["args"], [])
        validate_plan(self.plan)

    def test_selected_image_artifacts_have_one_fixed_complete_runner(self):
        self.assertEqual(CHECKERS.get("image_artifact_tests"),
                         ("tests/contracts/test_image_artifacts.py",))
        selected = [stage for stage in self.stages
                    if stage["checker"] == "image_artifact_tests"]
        self.assertEqual(len(selected), 1)
        self.assertEqual(selected[0]["name"], "image-artifact-boundaries")
        self.assertEqual(selected[0]["args"], [])
        validate_plan(self.plan)

    def test_selected_image_boundaries_follow_existing_artifact_recovery(self):
        names = [stage["name"] for stage in self.stages]
        self.assertIn("image-artifact-boundaries", names)
        previous = names.index("build-artifact-boundaries")
        self.assertEqual(names[previous:previous + 3],
                         ["build-artifact-boundaries",
                          "image-artifact-boundaries",
                          "providers-mainstream_assembly"])

    def test_image_artifact_script_bootstraps_without_import_environment(self):
        script = ROOT / "tests/contracts/test_image_artifacts.py"
        case = ("ImageArtifactTests."
                "test_default_full_reset_image_retains_existing_accounting")
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [sys.executable, "-I", "-B", str(script), case],
                cwd=directory, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Ran 1 test", result.stderr)

    def test_missing_arm_tool_cannot_be_a_skipped_image_artifact_pass(self):
        script = ROOT / "tests/contracts/test_image_artifacts.py"
        case = ("ImageArtifactTests."
                "test_default_full_reset_image_retains_existing_accounting")
        environment = os.environ.copy()
        environment["PATH"] = ""
        environment.pop("NEXUS_ARM_GCC", None)
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [sys.executable, "-I", "-B", str(script), case],
                cwd=directory, env=environment, capture_output=True,
                text=True, timeout=30)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Prepared ARM compiler required", result.stderr)

    def test_each_expanded_provider_fixture_is_explicitly_qualified_once(self):
        for directory in ("legacy_transport_assembly", "mainstream_assembly",
                          "g474_assembly", "h743_assembly", "u575_assembly"):
            with self.subTest(directory=directory):
                stages = [stage for stage in self.stages
                          if stage["name"] == "providers-" + directory]
                self.assertEqual(len(stages), 1,
                                 "Missing complete maintained fixture stage")
                stage = stages[0]
                self.assertEqual(stage["checker"], "provider_assemblies")
                fixtures = [stage["args"][index + 1]
                            for index, value in enumerate(stage["args"])
                            if value == "--fixture"]
                expected = {"{root}/" + path.relative_to(ROOT).as_posix()
                            for path in (ROOT / "tests/contracts" /
                                         directory).glob("*.toml")}
                if directory == "h743_assembly":
                    expected |= {
                        "{root}/tools/configure/assemblies/h743-" + name + ".toml"
                        for name in ("spi-dma", "spi-dma-cache-on",
                                     "spi-dma-uart-tx", "spi-dma-uart-rx",
                                     "uart-continuous", "uart-continuous-cache-on",
                                     "uart-duplex-continuous",
                                     "uart-duplex-continuous-cache-on")}
                    self.assertEqual(len(expected), 34)
                    projects = [stage["args"][index + 1]
                                for index, value in enumerate(stage["args"])
                                if value == "--project"]
                    self.assertEqual(projects,
                        ["{root}/tests/contracts/h743_assembly"])
                if directory == "u575_assembly":
                    expected |= {
                        "{root}/tools/configure/assemblies/u575-" + name + ".toml"
                        for name in ("uart-chain", "lptim-run")}
                    self.assertEqual(len(expected), 12)
                self.assertTrue(expected)
                self.assertEqual(len(fixtures), len(set(fixtures)),
                                 "Duplicate fixture is not another execution")
                self.assertEqual(set(fixtures), expected,
                                 "New fixture omitted from formal software plan")
        validate_plan(self.plan)

    def test_canonical_provider_inputs_use_executable_owned_projects(self):
        for directory, family in (("h743_assembly", "stm32h743"),
                                  ("u575_assembly", "stm32u575")):
            with self.subTest(directory=directory):
                stage, = [item for item in self.stages
                          if item["name"] == "providers-" + directory]
                args = stage["args"]
                paths = [Path(args[i + 1].replace("{root}", str(ROOT)))
                         for i, value in enumerate(args)
                         if value == "--fixture" and args[i + 1].startswith(
                             "{root}/tools/configure/assemblies/")]
                projects = [Path(args[i + 1].replace("{root}", str(ROOT)))
                            for i, value in enumerate(args) if value == "--project"]
                self.assertTrue(paths)
                self.assertLessEqual(len(projects), 1)
                rows = provider_assemblies.validate_fixtures(
                    ROOT, paths, family=family,
                    project=projects[0] if projects else None)
                self.assertEqual(len(rows), len(paths))
                self.assertEqual({row["project"] for row in rows},
                                 {str(ROOT / "tests/contracts" / directory)})


    def test_named_pin_state_canonical_has_one_real_arm_checker_stage(self):
        fixture = "{root}/tools/configure/assemblies/g474-pin-states.toml"
        selected = [stage for stage in self.stages if fixture in stage["args"]]
        self.assertEqual(len(selected), 1)
        stage = selected[0]
        self.assertEqual(stage["checker"], "provider_assemblies")
        args = stage["args"]
        self.assertEqual([args[i + 1] for i, arg in enumerate(args)
                          if arg == "--fixture"], [fixture])
        self.assertEqual([args[i + 1] for i, arg in enumerate(args)
                          if arg == "--project"],
                         ["{root}/tests/contracts/pin_state_fixture"])
        self.assertEqual([args[i + 1] for i, arg in enumerate(args)
                          if arg == "--family"], ["stm32g474"])
        self.assertEqual([args[i + 1] for i, arg in enumerate(args)
                          if arg == "--parallel"], ["1"])
        self.assertNotIn("status", stage)
        validate_plan(self.plan)

    def test_each_io_evolution_preset_configures_and_builds_once(self):
        selected = [name for name in Presets(ROOT).rows["configure"]
                    if name.startswith("fixture-io-evolution-")]
        self.assertTrue(selected)
        for preset in selected:
            with self.subTest(preset=preset):
                stages = [stage for stage in self.stages
                          if stage["args"] == ["--preset", preset]]
                self.assertEqual([stage["checker"] for stage in stages],
                                 ["configure", "build"])
        validate_plan(self.plan)

    def test_native_time_presets_keep_real_host_targets_and_execution_boundaries(self):
        presets = Presets(ROOT)
        configurations = [row for row, _ in presets.rows["configure"].values()]
        builds = [row for row, _ in presets.rows["build"].values()]
        for profile in ("pair", "empty"):
            with self.subTest(profile=profile):
                name = "fixture-io-evolution-native-time-" + profile
                selected = [item for item in configurations if item["name"] == name]
                self.assertEqual(len(selected), 1)
                preset = selected[0]
                self.assertEqual(preset["inherits"], "native-release")
                variables = preset["cacheVariables"]
                self.assertEqual(variables["NEXUS_ASSEMBLY_FILE"],
                    "${sourceDir}/cmake/package/verification/io_evolution/" +
                    "native-time-" + profile + ".toml")
                self.assertNotIn("CMAKE_TOOLCHAIN_FILE", variables)
                self.assertNotIn("toolchainFile", preset)
                self.assertNotIn("NEXUS_IO_EVOLUTION_FIXTURE", variables)
                self.assertNotEqual(variables.get("NEXUS_BUILD_TESTS"), "OFF")
                selected_builds = [item for item in builds if item["name"] == name]
                self.assertEqual(len(selected_builds), 1)
                self.assertEqual(selected_builds[0]["configurePreset"], name)
                self.assertEqual(selected_builds[0]["targets"],
                    ["nexus_rtc_lptim_" + profile + "_generated_test"])
                stages = [stage for stage in self.stages
                          if stage["args"] == ["--preset", name]]
                self.assertEqual([stage["checker"] for stage in stages],
                                 ["configure", "build"])
                self.assertTrue(all("status" not in stage for stage in stages),
                                "Planned stages are not execution evidence")
        self.assertEqual(CHECKERS["native_contracts"],
                         ("scripts/ci/native_contracts.py",))
        self.assertEqual(CHECKERS["sdk_consumers"],
                         ("cmake/package/verify_consumers.py",))
        for preset in ("native-debug", "native-release", "native-asan", "native-tsan"):
            selected = [stage for stage in self.stages
                        if stage["checker"] == "native_contracts" and
                        stage["args"] == ["--preset", preset]]
            self.assertEqual(len(selected), 1)
        self.assertEqual(sum(stage["checker"] == "sdk_consumers"
                             for stage in self.stages), 1)
        sdk_checker = (ROOT / "cmake/package/verify_consumers.py").read_text()
        self.assertIn('run_checked([str(image)])', sdk_checker)
        self.assertIn('"nm", "-S", "--defined-only", "--format=posix"', sdk_checker)
        canonical = (ROOT / "cmake/package/verification/io_evolution/consumers/"
                     "native-time-pair.c").read_text()
        self.assertIn('"/proc/self/maps"', canonical)
        self.assertIn('readonly_mapping(a->ops, sizeof(*a->ops))', canonical)
        validate_plan(self.plan)

    def test_redistributed_io_references_have_one_qualified_preset(self):
        directory = ROOT / "cmake/package/verification/io_evolution"
        fixtures = sorted(directory.glob("*.toml"))
        self.assertTrue(fixtures, "Redistributed references must not disappear")
        presets = [row for row, _ in Presets(ROOT).rows["configure"].values()]
        for fixture in fixtures:
            with self.subTest(fixture=fixture.name):
                source = "${sourceDir}/" + fixture.relative_to(ROOT).as_posix()
                selected = [preset["name"] for preset in presets
                            if preset.get("cacheVariables", {}).get(
                                "NEXUS_ASSEMBLY_FILE") == source]
                self.assertEqual(len(selected), 1)
                stages = [stage for stage in self.stages
                          if stage["args"] == ["--preset", selected[0]]]
                self.assertEqual([stage["checker"] for stage in stages],
                                 ["configure", "build"])
        validate_plan(self.plan)


if __name__ == "__main__":
    unittest.main()
