"""F407 DAC choices retain typed static ownership and exact pad admission."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, bindings, configure as gate, factory


class F407DacConfigurationTests(unittest.TestCase):
    def resolve(self, directory, *, density="ve", mode="direct", channels="[1,2]",
                data_format="u12-right", idle="[0,0]", trigger=None,
                extra="", adc_conflict=False):
        fixture = gate.ROOT / "tests/contracts/dac_assembly"
        board = gate.load(fixture / f"{density}_board/board.json")
        board["id"] = f"stm32f407{density}-dac-software-fixture"
        board["bindings"] = [{"id": "output", "controller": "DAC",
                              "route": "dac-pa4-pa5"}]
        if adc_conflict:
            board["bindings"].append({"id": "analog", "controller": "ADC1",
                                      "route": "adc1-ch4-pa4"})
        (directory / "board.json").write_text(json.dumps(board))
        trigger = trigger if trigger is not None else (
            "none" if mode == "direct" else "software")
        source = (
            'schema=2\nboard_package="."\nbackend="baremetal"\n'
            'clock="hse8-pll168"\noptimization="Os"\n'
            '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
            '[dac.output]\nbinding="output"\n'
            f'mode="{mode}"\nchannels={channels}\nformat="{data_format}"\n'
            f'idle_codes={idle}\noutput_buffer=true\ntrigger="{trigger}"\n'
            + extra)
        if adc_conflict:
            source += ('\n[adc.input]\nbinding="analog"\nmode="single-shot"\n'
                       'channels=[4]\nsample_times=[7]\nreference_mv=3300\n')
        assembly = directory / "assembly.toml"
        assembly.write_text(source)
        return gate.resolve_ir(assembly)

    def test_direct_formats_resolve_for_both_exact_parts(self):
        with tempfile.TemporaryDirectory() as temporary:
            for density in ("ve", "zg"):
                for data_format in ("u8", "u12-right", "u12-left"):
                    with self.subTest(density=density, data_format=data_format):
                        result = self.resolve(Path(temporary), density=density,
                                              data_format=data_format)
                        port = result.controllers[0]
                        self.assertEqual("DAC", port.controller)
                        self.assertEqual((1, 2), port.options.channels)
                        self.assertEqual(data_format, port.options.format)
                        self.assertEqual((0, 0), port.options.idle_codes)
                        with self.assertRaises(FrozenInstanceError):
                            port.options.output_buffer = False

    def test_selected_channel_claims_only_its_authorized_pad(self):
        with tempfile.TemporaryDirectory() as temporary:
            for channels, pin in (("[1]", "PA4"), ("[2]", "PA5")):
                with self.subTest(channels=channels):
                    result = self.resolve(Path(temporary), channels=channels,
                                          idle="[0]")
                    self.assertEqual([pin], [row["pin"]
                        for row in result.controllers[0].pins])
                    claimed = {row.key for row in result.claims
                               if row.owner == "output" and
                               row.key.startswith("pin:")}
                    self.assertEqual({"pin:" + pin}, claimed)

    def test_software_trigger_and_hardware_wave_modes_resolve(self):
        with tempfile.TemporaryDirectory() as temporary:
            for mode in ("triggered", "noise", "triangle"):
                with self.subTest(mode=mode):
                    extra = "amplitude=255\n" if mode != "triggered" else ""
                    result = self.resolve(Path(temporary), mode=mode,
                                          extra=extra)
                    self.assertEqual("software", result.controllers[0].
                                     options.trigger)

    def test_factory_keeps_a_nominal_dac_identity_and_c_lifecycle(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.resolve(Path(temporary))
            header, _ = factory.emit(result)
            self.assertIn("nx_factory_dac", header)
            self.assertIn("nx_dac_port_t", header)
            source = bindings.emit(result)[1]
            self.assertIn("nx_stm32_dac_prepare", source)
            self.assertIn("nx_stm32_dac_close", source)
            self.assertNotIn("HAL_DAC_Start", source)

    def test_channel_selection_is_nonempty_unique_and_physical(self):
        with tempfile.TemporaryDirectory() as temporary:
            for channels in ("[]", "[0]", "[3]", "[1,1]", "[true]"):
                with self.subTest(channels=channels):
                    with self.assertRaisesRegex(ValueError, "DAC.*channel"):
                        self.resolve(Path(temporary), channels=channels)

    def test_idle_codes_have_exact_count_and_raw_resolution_range(self):
        with tempfile.TemporaryDirectory() as temporary:
            for data_format, idle in (("u8", "[256,0]"),
                                      ("u12-right", "[4096,0]"),
                                      ("u12-left", "[-1,0]"),
                                      ("u12-right", "[0]"),
                                      ("u12-right", "[false,0]")):
                with self.subTest(data_format=data_format, idle=idle):
                    with self.assertRaisesRegex(ValueError, "DAC.*idle"):
                        self.resolve(Path(temporary), data_format=data_format,
                                     idle=idle)

    def test_direct_mode_cannot_silently_accept_a_trigger(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "DAC.*trigger"):
                self.resolve(Path(temporary), trigger="software")

    def test_wave_amplitude_is_exact_and_cannot_wrap_the_raw_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            for extra, idle in (("amplitude=256\n", "[0,0]"),
                                 ("amplitude=4095\n", "[1,0]")):
                with self.subTest(extra=extra):
                    with self.assertRaisesRegex(ValueError, "DAC.*amplitude"):
                        self.resolve(Path(temporary), mode="triangle",
                                     idle=idle, extra=extra)

    def test_unimplemented_dma_is_not_an_advertised_production_mode(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError,
                                        "[Uu]nsupported|[Uu]nmaintained|not.*support"):
                self.resolve(Path(temporary), mode="trigger-dma")

    def test_adc_and_dac_cannot_own_the_same_physical_pad(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "conflict.*pin:PA4"):
                self.resolve(Path(temporary), adc_conflict=True)

    def test_editor_schema_declares_only_real_dac_storage_formats(self):
        schema = authored.editor_schema()["properties"]["dac"][
            "additionalProperties"]["properties"]
        self.assertEqual(["u8", "u12-right", "u12-left"],
                         schema["format"]["enum"])
        self.assertEqual("boolean", schema["output_buffer"]["type"])


if __name__ == "__main__":
    unittest.main()
