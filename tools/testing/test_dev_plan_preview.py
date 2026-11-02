"""Plan previews expose authored selections without executing their checkers."""

from contextlib import redirect_stderr, redirect_stdout
import hashlib
import importlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.dev import dev
from tools.dev.qualification import CHECKERS


class DeveloperPlanPreviewTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.plan = self.root / "plan.json"
        self.output = self.root / "future-execution"
        self.stages = [
            {"name": "tools", "checker": "tool_tests",
             "args": ["--suite", "testing", "--report",
                      "{output}/tools.json"]},
            {"name": "configure", "checker": "configure",
             "args": ["--preset", "native-debug"], "timeout_s": 90},
            {"name": "contracts", "checker": "native_contracts",
             "args": ["--preset=native-debug"]},
            {"name": "station", "checker": "hil_prepare",
             "args": ["--fixture", "{root}/fixture.json",
                      "--output", "{output}/station"]},
        ]
        for checker in CHECKERS.values():
            script = self.root / checker[0]
            script.parent.mkdir(parents=True, exist_ok=True)
            script.write_text("# A checker input; it must never execute.\n")
        self.presets = self.root / "CMakePresets.json"
        self.presets.write_text(json.dumps({
            "version": 6,
            "configurePresets": [
                {"name": "base", "hidden": True,
                 "generator": "Ninja", "binaryDir": "${sourceDir}/build"},
                {"name": "native-debug", "inherits": "base"},
            ],
            "buildPresets": [{"name": "native-debug",
                              "configurePreset": "native-debug"}],
            "testPresets": [{"name": "native-debug",
                             "configurePreset": "native-debug"}],
        }))
        self.write_plan()

    def write_plan(self, **changes):
        self.plan.write_text(json.dumps({"schema_version": 1, "formal": True,
                                         "stages": self.stages, **changes}))

    def module(self):
        return importlib.import_module("tools.dev.plan")

    def preview(self):
        return self.module().preview(self.root, self.plan, self.output,
                                     environment={})

    def test_preview_uses_actual_plan_order_commands_and_deadlines(self):
        with (
            patch("subprocess.run", side_effect=AssertionError("executed")),
            patch("subprocess.Popen", side_effect=AssertionError("spawned")),
        ):
            result = self.preview()
        self.assertEqual([stage["name"] for stage in result["stages"]],
                         [stage["name"] for stage in self.stages])
        self.assertEqual(result["stages"][0]["argv"], [
            dev.sys.executable, "-B",
            str(self.root / CHECKERS["tool_tests"][0]),
            "--suite", "testing", "--report", str(self.output / "tools.json")])
        self.assertEqual(result["stages"][1]["timeout_s"], 90)
        self.assertEqual(result["stages"][0]["timeout_s"], 1800)
        self.assertFalse(self.output.exists())

    def test_preview_carries_input_bytes_but_no_test_or_physical_pass(self):
        result = self.preview()
        self.assertEqual(result["kind"], "test_plan_preview")
        self.assertEqual(result["status"], "not_executed")
        self.assertFalse(result["qualification"])
        self.assertFalse(result["software_qualified"])
        self.assertEqual(result["physical_status"], "not_executed")
        self.assertEqual(result["plan"]["sha256"],
                         hashlib.sha256(self.plan.read_bytes()).hexdigest())
        self.assertTrue(result["formal_requested"])
        for index, stage in enumerate(result["stages"]):
            self.assertEqual(stage["selection"]["stage_index"], index)
            self.assertEqual(stage["selection"]["reason"],
                             "explicit_plan_stage")
            self.assertIsNone(stage["coverage"]["case_count"])
            self.assertEqual(stage["coverage"]["discovery"], "not_executed")
            self.assertEqual(stage["status"], "not_executed")

    def test_stage_coverage_reads_owned_suite_and_preset_metadata(self):
        result = self.preview()
        tools, configure, contracts, station = result["stages"]
        self.assertEqual(tools["coverage"]["tool_suite"], "testing")
        self.assertEqual(tools["coverage"]["tool_directory"], "tools/testing")
        self.assertEqual(configure["coverage"]["preset"], {
            "kind": "configure", "name": "native-debug",
            "configure": "native-debug", "binary_directory":
            str(self.root / "build"), "filter": None})
        self.assertEqual(contracts["coverage"]["preset"]["kind"], "test")
        self.assertEqual(station["hardware"]["purpose"],
                         "station_preparation_only")
        self.assertFalse(station["hardware"]["requires_equipment"])

    def test_unknown_placeholder_checker_and_duplicate_stage_reject(self):
        originals = json.loads(self.plan.read_text())
        cases = [
            {"name": "bad", "checker": "shell", "args": []},
            {"name": "bad", "checker": "build", "args": ["{secret}"]},
            self.stages[0],
        ]
        for stage in cases:
            with self.subTest(stage=stage):
                self.write_plan(stages=originals["stages"] + [stage])
                with self.assertRaises(ValueError):
                    self.preview()
                self.assertFalse(self.output.exists())

    def test_unknown_disabled_hidden_or_missing_preset_rejects(self):
        document = json.loads(self.presets.read_text())
        for name in ("missing", "base", "disabled"):
            with self.subTest(name=name):
                if name == "disabled":
                    document["configurePresets"].append({
                        "name": "disabled", "binaryDir": "build",
                        "condition": False})
                self.presets.write_text(json.dumps(document))
                self.write_plan(stages=[{"name": "configure",
                                        "checker": "configure",
                                        "args": ["--preset", name]}])
                with self.assertRaises(ValueError):
                    self.preview()
        self.write_plan(stages=[{"name": "configure", "checker": "configure",
                                 "args": []}])
        with self.assertRaisesRegex(ValueError, "preset"):
            self.preview()

    def test_ambiguous_preset_or_unknown_tool_suite_rejects(self):
        for stage in (
                {"name": "configure", "checker": "configure",
                 "args": ["--preset", "native-debug", "--preset=other"]},
                {"name": "tools", "checker": "tool_tests",
                 "args": ["--suite", "missing"]}):
            with self.subTest(stage=stage):
                self.write_plan(stages=[stage])
                with self.assertRaises(ValueError):
                    self.preview()

    def test_existing_or_symlink_execution_destination_rejects(self):
        self.output.mkdir()
        with self.assertRaisesRegex(ValueError, "new|exist"):
            self.preview()
        self.output.rmdir()
        self.output.symlink_to(self.root / "missing", target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.preview()

    def test_missing_or_symlink_checker_rejects(self):
        script = self.root / CHECKERS["tool_tests"][0]
        script.unlink()
        with self.assertRaises(ValueError):
            self.preview()
        script.symlink_to(self.plan)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.preview()

    def test_preset_changed_after_loading_cannot_be_bound_to_new_bytes(self):
        module = self.module()
        reader = module.Presets

        def changing_reader(root, environment):
            result = reader(root, environment)
            document = json.loads(self.presets.read_text())
            document["configurePresets"][0]["binaryDir"] = "changed"
            self.presets.write_text(json.dumps(document))
            return result

        with patch.object(module, "Presets", side_effect=changing_reader):
            with self.assertRaisesRegex(ValueError, "changed"):
                self.preview()
        self.assertFalse(self.output.exists())

    def test_json_is_exclusive_and_future_execution_stays_absent(self):
        destination = self.root / "preview.json"
        arguments = ["--plan", str(self.plan), "--output-root",
                     str(self.output), "--json", str(destination)]
        with redirect_stdout(io.StringIO()):
            self.assertEqual(self.module().main(arguments, root=self.root), 0)
        result = json.loads(destination.read_text())
        self.assertEqual(result["status"], "not_executed")
        original = destination.read_bytes()
        with redirect_stderr(io.StringIO()):
            self.assertEqual(self.module().main(arguments, root=self.root), 2)
        self.assertEqual(destination.read_bytes(), original)
        self.assertFalse(self.output.exists())

    def test_json_cannot_replace_authority_or_create_execution_directory(self):
        original = self.plan.read_bytes()
        destinations = (self.plan, self.presets, self.output / "preview.json")
        for destination in destinations:
            with self.subTest(destination=destination):
                arguments = ["--plan", str(self.plan), "--output-root",
                             str(self.output), "--json", str(destination)]
                with redirect_stderr(io.StringIO()):
                    result = self.module().main(arguments, root=self.root)
                self.assertEqual(result, 2)
                self.assertFalse(self.output.exists())
                self.assertEqual(self.plan.read_bytes(), original)

    def test_cli_routes_preview_without_executing_developer_commands(self):
        with (
            patch.object(dev, "ROOT", self.root),
            patch.object(dev, "execute", side_effect=AssertionError("run")),
            redirect_stdout(io.StringIO()),
        ):
            self.assertEqual(dev.main([
                "plan", "--plan", str(self.plan), "--output-root",
                str(self.output)]), 0)

    def test_cli_rejects_preset_filter_instead_of_silently_ignoring_it(self):
        with redirect_stderr(io.StringIO()), redirect_stdout(io.StringIO()):
            result = dev.main(["plan", "--preset", "native-debug"])
        self.assertEqual(result, 2)


if __name__ == "__main__":
    unittest.main()
