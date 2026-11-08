"""F407 resolution is one typed choice shared by all regular ADC modes."""
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.configure import adc, authored, bindings, configure as gate


class F407AdcResolutionTests(unittest.TestCase):
    def resolve(self, directory, density, mode, bits=None):
        fixture = gate.ROOT / "tests/contracts/adc_external_assembly"
        board = gate.load(fixture / f"{density}_board/board.json")
        if mode == "single-shot":
            board["bindings"][0]["route"] = "adc1-ch15-pc5"
        (directory / "board.json").write_text(json.dumps(board))
        source = (fixture / f"{density}-scan.toml").read_text()
        source = source.replace(f'board_package = "{density}_board"',
                                'board_package = "."')
        source = source.replace('mode = "low-rate-scan"', f'mode = "{mode}"')
        if mode == "single-shot":
            source = source.replace(
                'channels = [15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0]',
                'channels = [15]')
            source = source.replace(
                'sample_times = [7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7]',
                'sample_times = [7]')
        if bits is not None:
            source += f"resolution_bits = {bits}\n"
        if mode.startswith("trigger-dma"):
            source += "\n[adc.analog.irq]\npriority = 5\n"
        assembly = directory / "assembly.toml"
        assembly.write_text(source)
        return gate.resolve_ir(assembly)

    def test_omitted_resolution_means_explicit_twelve_bit_ir(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.resolve(Path(temporary), "ve", "low-rate-scan")
            self.assertEqual(12, result.controllers[0].options.resolution_bits)
            self.assertEqual(12, result.controllers[0].projection()[
                "resolution_bits"])

    def test_each_resolution_assembles_every_maintained_mode(self):
        with tempfile.TemporaryDirectory() as temporary:
            for density in ("ve", "zg"):
                for mode in ("single-shot", "low-rate-scan", "trigger-dma",
                             "trigger-dma-continuous"):
                    for bits in (6, 8, 10, 12):
                        with self.subTest(density=density, mode=mode, bits=bits):
                            result = self.resolve(Path(temporary), density,
                                                  mode, bits)
                            self.assertEqual(bits, result.controllers[0].options.
                                             resolution_bits)
                            self.assertEqual(bits, result.controllers[0].
                                             projection()["resolution_bits"])
                            self.assertIn(f".resolution = {(12 - bits) // 2}u",
                                          bindings.emit(result)[1])

    def test_non_hardware_resolutions_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            for value in ("0", "5", "7", "9", "11", "13", "true", '"8"'):
                with self.subTest(value=value):
                    with self.assertRaisesRegex(ValueError, "resolution"):
                        self.resolve(Path(temporary), "ve", "single-shot", value)

    def test_editor_schema_exposes_exact_integer_choices(self):
        adc_schema = authored.editor_schema()["properties"]["adc"][
            "additionalProperties"]["properties"]
        self.assertEqual({"type": "integer", "enum": [6, 8, 10, 12],
                          "default": 12},
                         {key: adc_schema["resolution_bits"][key]
                          for key in ("type", "enum", "default")})

    def test_explicit_native_model_keeps_the_same_selected_raw_format(self):
        board = gate.load(gate.ROOT / "tests/support/native/board/board.json")
        board["bindings"] = [{"id": "analog", "controller": "ADC0",
                              "route": "adc0"}]
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "board.json").write_text(json.dumps(board))
            assembly = directory / "assembly.toml"
            assembly.write_text(
                'schema=2\nboard_package="."\nbackend="native"\n'
                'clock="model"\noptimization="Os"\n'
                '[memory]\nmain_stack_bytes=2048\n'
                '[adc.analog]\nbinding="analog"\nmode="low-rate-scan"\n'
                'channels=[8,9]\nsample_times=[0,0]\nreference_mv=3300\n'
                'resolution_bits=6\n')
            result = gate.resolve_ir(assembly, test_support=True)
            self.assertEqual(6, result.controllers[0].options.resolution_bits)
            self.assertRegex(bindings.emit(result)[1],
                             r"s_nx_adc_model_analog, 2u?, 6u?, 3300u?")

    def test_other_providers_require_an_explicit_resolution_declaration(self):
        provider = SimpleNamespace(LIMITS={})
        route = {"controller": "ADC0", "pins": [{"pin": "PA0"}]}
        selected = {"mode": "single-shot", "channels": [0],
                    "sample_times": [0], "reference_mv": 3300}
        adc.validate_selection(selected, route, provider)
        self.assertEqual(12, selected["resolution_bits"])
        for bits in (6, 8, 10):
            with self.subTest(bits=bits):
                with self.assertRaisesRegex(ValueError, "resolution"):
                    adc.validate_selection({**selected, "resolution_bits": bits},
                                           route, provider)


if __name__ == "__main__":
    unittest.main()
