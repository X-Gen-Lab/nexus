"""The maintained software plan executes complete Native and SDK boundaries."""

from pathlib import Path
import unittest

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
                self.assertTrue(expected)
                self.assertEqual(len(fixtures), len(set(fixtures)),
                                 "Duplicate fixture is not another execution")
                self.assertEqual(set(fixtures), expected,
                                 "New fixture omitted from formal software plan")
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
