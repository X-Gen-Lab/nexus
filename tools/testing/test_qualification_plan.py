"""The maintained software plan executes complete Native and SDK boundaries."""

from pathlib import Path
import unittest

from scripts.ci import provider_assemblies
from tools.dev.qualification import CHECKERS, validate_plan
from tools.evidence.common import load_json

ROOT = Path(__file__).resolve().parents[2]


class QualificationPlanTests(unittest.TestCase):
    def setUp(self):
        self.plan = load_json(ROOT / "tools/dev/plans/software.json")
        self.stages = self.plan["stages"]

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
        presets = load_json(ROOT / "CMakePresets.json")["configurePresets"]
        selected = [preset["name"] for preset in presets
                    if preset["name"].startswith("fixture-io-evolution-")]
        self.assertTrue(selected)
        for preset in selected:
            with self.subTest(preset=preset):
                stages = [stage for stage in self.stages
                          if stage["args"] == ["--preset", preset]]
                self.assertEqual([stage["checker"] for stage in stages],
                                 ["configure", "build"])
        validate_plan(self.plan)

    def test_native_time_presets_keep_real_host_targets_and_execution_boundaries(self):
        presets = load_json(ROOT / "CMakePresets.json")
        configurations = presets["configurePresets"]
        builds = presets["buildPresets"]
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
        presets = load_json(ROOT / "CMakePresets.json")["configurePresets"]
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
