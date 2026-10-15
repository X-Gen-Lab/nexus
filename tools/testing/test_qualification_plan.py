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


if __name__ == "__main__":
    unittest.main()
