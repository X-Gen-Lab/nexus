"""Board fitting and jumpers are explicit selections, never physical proof."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import authored, configure as gate, experience


class BoardHardwareTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ("dependencies/platform-compatibility.json",
                     "tests/support/native/soc.json", "tests/support/native/routes.json",
                     "tests/support/native/board/board.json"):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(gate.ROOT / name, path)
        self.board_path = self.root / "tests/support/native/board/board.json"
        self.board = gate.load(self.board_path)
        self.assembly = self.root / "assembly.toml"
        self.contents = '''schema = 2
board = "native_reference"
backend = "native"
clock = "model"
[memory]
main_stack_bytes = 2048
'''

    def options(self):
        self.board["hardware_options"] = {
            "debug": ["swd", "jtag"],
            "nor_population": ["absent", "w25q128"]}
        self.board["bindings"][0]["requires"] = {"debug": "swd"}

    def resolve(self, extra=""):
        self.board_path.write_text(json.dumps(self.board), encoding="utf-8")
        self.assembly.write_text(self.contents + extra, encoding="utf-8")
        return gate.resolve_ir(self.assembly, self.root, test_support=True)

    def gpio(self, selection=""):
        return self.resolve(selection + '''
[gpio.status]
binding = "led0"
mode = "output"
''')

    def reject(self, extra, message="hardware"):
        with self.assertRaisesRegex(gate.ConfigurationError, message):
            self.gpio(extra)

    def test_existing_board_does_not_gain_an_effective_hardware_decision(self):
        result = self.gpio()
        self.assertNotIn("hardware", result.to_dict())

    def test_explicit_choices_are_immutable_audit_facts_without_hil(self):
        self.options()
        result = self.gpio('[hardware]\ndebug = "swd"\n')
        self.assertEqual(result.hardware.selected[0].option, "debug")
        self.assertEqual(result.hardware.selected[0].value, "swd")
        with self.assertRaises(FrozenInstanceError):
            result.hardware.selected[0].value = "jtag"
        projected = result.to_dict()["hardware"]
        self.assertEqual(projected["selected"], {"debug": "swd"})
        self.assertEqual(projected["options"]["nor_population"],
                         ["absent", "w25q128"])
        self.assertEqual(projected["physical_verification"],
                         "not_established_by_configuration")
        self.assertFalse(result.physical_qualified)

    def test_missing_condition_rejects_before_controller_normalization(self):
        self.options()
        with patch.object(gate, "normalize_controller") as normalize:
            self.reject("", "hardware.*debug")
        normalize.assert_not_called()

    def test_contradictory_choice_rejects_instead_of_repairing(self):
        self.options()
        self.reject('[hardware]\ndebug = "jtag"\n', "hardware.*debug")

    def test_unknown_or_nonstring_authored_values_are_rejected(self):
        self.options()
        for extra in ('[hardware]\nmissing = "swd"\n',
                      '[hardware]\ndebug = "automatic"\n',
                      '[hardware]\ndebug = true\n',
                      '[hardware]\ndebug = 1\n',
                      'hardware = []\n'):
            with self.subTest(extra=extra):
                self.reject(extra)

    def test_board_option_choices_are_closed_and_nonempty(self):
        for choices in ([], ["swd", "swd"], "swd", [True], [""], [1]):
            with self.subTest(choices=choices):
                self.options()
                self.board["hardware_options"]["debug"] = choices
                self.reject("", "hardware")

    def test_binding_conditions_must_reference_declared_exact_choices(self):
        for requirements in ({"unknown": "swd"}, {"debug": "automatic"},
                             {"debug": True}, ["debug"], {}):
            with self.subTest(requirements=requirements):
                self.options()
                self.board["bindings"][0]["requires"] = requirements
                self.reject('[hardware]\ndebug = "swd"\n')

    def test_unselected_binding_does_not_invent_fitting_or_block_console(self):
        self.options()
        result = self.resolve('''
[uart.console]
binding = "uart0"
mode = "irq"
baud = 115200
[uart.console.rx]
format = "events"
capacity = 64
[uart.console.irq]
priority = 5
''')
        self.assertEqual(result.hardware.selected, ())
        view = experience.bindings_view(experience.binding_facts(
            gate, result, self.root))
        led = next(x for x in view["bindings"] if x["binding"] == "led0")
        self.assertEqual(led["hardware_requirements"], {"debug": "swd"})
        self.assertEqual(led["hardware_selection"], "unselected")
        self.assertEqual(led["admission"], "configuration-required")

    def test_board_first_query_keeps_requirements_unassessed(self):
        self.options()
        self.board_path.write_text(json.dumps(self.board), encoding="utf-8")
        catalogue = experience.board_catalogue(
            gate, self.root, board="native_reference", test_support=True)
        view = experience.bindings_view(catalogue)
        led = next(x for x in view["bindings"] if x["binding"] == "led0")
        self.assertEqual(led["hardware_selection"], "unselected")
        self.assertEqual(view["hardware"]["selected"], {})
        self.assertEqual(view["selection"]["backend"], None)

    def test_spi_child_cs_cannot_bypass_population_condition(self):
        self.options()
        self.board["bindings"][0].update(
            initial=1, requires={"nor_population": "w25q128"})
        extra = '''
[spi.bus]
binding = "spi0"
mode = "short-poll"
[spi_device.nor]
controller = "bus"
cs_binding = "led0"
max_hz = 1000000
mode = 0
'''
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "hardware.*nor_population"):
            self.resolve(extra)
        result = self.resolve('[hardware]\nnor_population = "w25q128"\n'
                              + extra)
        self.assertEqual(result.devices[0].id, "nor")

    def test_hardware_change_is_an_effective_diff_and_distinct_identity(self):
        self.options()
        before = self.resolve('[hardware]\ndebug = "swd"\n')
        after = self.resolve('[hardware]\ndebug = "jtag"\n')
        self.assertNotEqual(before.configuration_sha256,
                            after.configuration_sha256)
        changes = experience.differences(before, after)
        self.assertTrue(any(change["path"].startswith("hardware")
                            for change in changes["changes"]), changes)

    def test_generator_source_closure_includes_hardware_contract(self):
        self.options()
        self.resolve()
        output = self.root / "bundle"
        gate.configure(self.assembly, output, self.root, test_support=True)
        paths = json.loads((output / "configuration_code_paths.json").read_text())
        self.assertIn("tools/configure/board_hardware.py",
                      paths)

    def test_pdf_provenance_is_exact_source_identity_without_copying_pdf(self):
        self.board["provenance"] = [{
            "file": "启明欣欣407ZGT6核心板原理图.pdf",
            "sha256": "4" * 64, "pages": [1],
            "components": ["U1", "D0"], "nets": ["LED0"],
            "scope": "Schematic facts only, no physical fitting proof"}]
        result = self.gpio()
        self.assertFalse(result.physical_qualified)
        self.assertFalse(any(x["path"].endswith(".pdf") for x in result.inputs))

    def test_pdf_provenance_rejects_incomplete_or_invalid_identities(self):
        valid = {"file": "board.pdf", "sha256": "4" * 64,
                 "pages": [1], "scope": "schematic only"}
        for patch_value in ({"sha256": "4" * 63}, {"pages": [0]},
                            {"pages": [1, 1]}, {"file": "../board.pdf"},
                            {"pages": [True]}, {"components": [1]}):
            with self.subTest(patch_value=patch_value):
                self.board["provenance"] = [{**valid, **patch_value}]
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "[Pp]rovenance"):
                    self.gpio()
        for missing in ("file", "sha256", "pages"):
            self.board["provenance"] = [
                {k: v for k, v in valid.items() if k != missing}]
            with self.subTest(missing=missing):
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "[Pp]rovenance"):
                    self.gpio()

    def test_editor_schema_uses_string_hardware_choices(self):
        schema = authored.editor_schema()["properties"]["hardware"]
        self.assertEqual(schema["type"], "object")
        self.assertEqual(schema["additionalProperties"], {"type": "string"})


if __name__ == "__main__":
    unittest.main()
