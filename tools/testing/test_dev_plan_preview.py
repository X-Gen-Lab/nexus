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

    def test_hil_fixture_requirements_remain_pending_without_resolved_routes(self):
        original = Path(__file__).resolve().parents[2]
        fixture = original / "tools/hil/fixtures/stm32f407_qiming_v31.json"
        (self.root / "fixture.json").write_bytes(fixture.read_bytes())
        self.stages[-1]["args"] = ["--fixture", "{root}/fixture.json",
                                    "--resolved", "{output}/resolved.json",
                                    "--report", "{output}/hil.json"]
        self.write_plan()
        result = self.preview()
        requirement = result["stages"][-1]["hardware"]["requirements"]
        self.assertEqual(requirement["status"], "configuration_pending")
        self.assertIn("uart_echo", requirement["required_tests"])
        self.assertIsNone(requirement["station_prerequisites"])
        self.assertTrue(all(row["status"] == "not_executed" for row in
                            requirement["pending_qualification"]))
        self.assertEqual(result["stages"][1]["hardware"]["assessment"],
                         "unknown")
        self.assertFalse(self.output.exists())

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

    def test_transient_authored_plan_cannot_replace_retained_stage_meaning(self):
        original = self.plan.read_bytes()
        replacement = json.loads(original)
        replacement["stages"][0]["timeout_s"] = 17
        read = Path.read_bytes

        def during_read(path):
            if Path(path) == self.plan:
                self.plan.write_text(json.dumps(replacement))
                try:
                    return read(path)
                finally:
                    self.plan.write_bytes(original)
            return read(path)

        with patch.object(Path, "read_bytes", new=during_read):
            try:
                result = self.preview()
            except ValueError:
                result = None
        if result is not None:
            self.assertEqual(result["stages"][0]["timeout_s"], 1800)
            self.assertEqual(result["plan"]["sha256"],
                             hashlib.sha256(original).hexdigest())
        self.assertEqual(self.plan.read_bytes(), original)

    def hil_inputs(self):
        from tools.evidence.identity import canonical_digest

        source = Path(__file__).resolve().parents[2]
        fixture = self.root / "fixture.json"
        fixture.write_bytes((source /
            "tools/hil/fixtures/stm32f407_qiming_v31.json").read_bytes())
        authored = json.loads(fixture.read_text())
        resolved = self.root / "resolved.json"
        configuration = {
            "schema_version": 1, "board": authored["board"],
            "part": authored["part"], "backend": "baremetal",
            "controllers": [{"id": "uart1", "kind": "uart",
                "controller": "USART1", "mode": "poll", "baud": 115200,
                "pins": [{"function": "tx", "pin": "PA9"},
                         {"function": "rx", "pin": "PA10"}]}]}
        configuration["configuration_sha256"] = canonical_digest(configuration)
        resolved.write_text(json.dumps(configuration))
        self.write_plan(stages=[{"name": "station", "checker": "hil_prepare",
            "args": ["--fixture", str(fixture), "--resolved", str(resolved),
                     "--report", "{output}/station.json"]}])
        return fixture, resolved

    def test_transient_hil_fixture_cannot_replace_retained_pending_budget(self):
        fixture, _resolved = self.hil_inputs()
        original = fixture.read_bytes()
        replacement = json.loads(original)
        replacement["pending_qualification"][0]["budget"] = 17
        read = Path.read_bytes

        def during_read(path):
            if Path(path) == fixture:
                fixture.write_text(json.dumps(replacement))
                try:
                    return read(path)
                finally:
                    fixture.write_bytes(original)
            return read(path)

        with patch.object(Path, "read_bytes", new=during_read):
            try:
                result = self.preview()
            except ValueError:
                result = None
        if result is not None:
            pending = result["stages"][0]["hardware"]["requirements"][
                "pending_qualification"]
            self.assertIsNone(pending[0]["budget"])
        self.assertEqual(fixture.read_bytes(), original)

    def test_transient_hil_resolved_cannot_replace_retained_configuration(self):
        from tools.evidence import identity

        _fixture, resolved = self.hil_inputs()
        original = resolved.read_bytes()
        replacement = json.loads(original)
        replacement["backend"] = "freertos"
        replacement.pop("configuration_sha256")
        replacement["configuration_sha256"] = identity.canonical_digest(
            replacement)
        validate = identity.resolved_identity

        def during_validation(path):
            resolved.write_text(json.dumps(replacement))
            try:
                return validate(path)
            finally:
                resolved.write_bytes(original)

        with patch.object(identity, "resolved_identity",
                          side_effect=during_validation):
            with self.assertRaises(ValueError):
                self.preview()
        self.assertEqual(resolved.read_bytes(), original)

    def test_transient_preset_cannot_replace_retained_build_selection(self):
        module = self.module()
        original = self.presets.read_bytes()
        replacement = json.loads(original)
        replacement["configurePresets"][0]["binaryDir"] = "transient-build"
        load_presets, read = module.Presets, Path.read_bytes

        def during_presets(root, environment):
            self.presets.write_text(json.dumps(replacement))
            try:
                return load_presets(root, environment)
            finally:
                self.presets.write_bytes(original)

        def during_read(path):
            if Path(path) == self.presets:
                self.presets.write_text(json.dumps(replacement))
                try:
                    return read(path)
                finally:
                    self.presets.write_bytes(original)
            return read(path)

        with patch.object(module, "Presets", side_effect=during_presets), \
                patch.object(Path, "read_bytes", new=during_read):
            with self.assertRaises(ValueError):
                self.preview()
        self.assertEqual(self.presets.read_bytes(), original)

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
