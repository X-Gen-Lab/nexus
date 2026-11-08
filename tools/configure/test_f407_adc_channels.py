"""F407 external channels and ranks follow the reviewed physical pad map."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import adc, bindings, configure as gate
from tools.configure.stm32f4 import contract


class F407AdcChannelTests(unittest.TestCase):
    PINS = tuple([f"PA{i}" for i in range(8)] + ["PB0", "PB1"] +
                 [f"PC{i}" for i in range(6)])

    def selection(self, pins, channels, samples=None, mode="low-rate-scan"):
        route = {"controller": "ADC1", "kind": "adc",
                 "pins": [{"pin": pin} for pin in pins]}
        selected = {"mode": mode, "channels": list(channels),
                    "sample_times": list(samples or [7] * len(channels)),
                    "reference_mv": 3300}
        return selected, route

    def test_all_external_pad_mappings_accept_a_single_shot(self):
        for channel, pin in enumerate(self.PINS):
            with self.subTest(channel=channel, pin=pin):
                selected, route = self.selection([pin], [channel],
                                                  mode="single-shot")
                adc.validate_selection(selected, route, contract)
                contract.validate_selection(selected, route)

    def test_scan_order_and_consistent_repeated_ranks_remain_explicit(self):
        for pins, channels in ((self.PINS, list(reversed(range(16)))),
                               (["PC5"], [15, 15, 15])):
            with self.subTest(channels=channels):
                selected, route = self.selection(pins, channels)
                adc.validate_selection(selected, route, contract)
                contract.validate_selection(selected, route)

    def test_conflicting_sample_times_are_not_a_representable_sequence(self):
        selected, route = self.selection(["PA0"], [0, 0], [1, 7])
        with self.assertRaisesRegex(ValueError, "sample time"):
            adc.validate_selection(selected, route, contract)

    def test_an_unconnected_channel_or_wrong_bank_is_rejected(self):
        for pins, channels in ((["PC0"], [0]), (["PB0"], [0]),
                               (["PA0"], [16]), (["PC6"], [6])):
            with self.subTest(pins=pins, channels=channels):
                selected, route = self.selection(pins, channels,
                                                  mode="single-shot")
                with self.assertRaises(ValueError):
                    adc.validate_selection(selected, route, contract)

    def test_malformed_ranks_report_configuration_errors(self):
        for channels in ([[0]], [True], ["0"], [None]):
            with self.subTest(channels=channels):
                selected, route = self.selection(["PA0"], channels,
                                                  mode="single-shot")
                with self.assertRaises(ValueError):
                    adc.validate_selection(selected, route, contract)

    def test_every_external_input_generates_static_c_configuration(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(gate.ROOT /
                              "boards/stm32f407_qiming_v31/board.json")
            board["id"] = "adc-external-input-software-fixture"
            board["provenance"] = []
            board["unknowns"] = ["Software fixture; no PCB wiring or HIL claim"]
            for channel in range(16):
                with self.subTest(channel=channel):
                    board["bindings"] = [{"id": "input", "controller": "ADC1",
                                          "route": f"adc1-ch{channel}-{self.PINS[channel].lower()}"}]
                    (directory / "board.json").write_text(json.dumps(board))
                    assembly = directory / "assembly.toml"
                    assembly.write_text(
                        'schema=2\nboard_package="."\nbackend="baremetal"\n'
                        'clock="hse8-pll168"\noptimization="Os"\n'
                        '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                        'flash_load_limit_bytes=65536\n'
                        'static_ram_limit_bytes=32768\n'
                        '[adc.input]\nbinding="input"\nmode="single-shot"\n'
                        f'channels=[{channel}]\nsample_times=[7]\n'
                        'reference_mv=3300\n')
                    result = gate.resolve_ir(assembly)
                    selected, = result.controllers
                    self.assertEqual(selected.options.channels, (channel,))
                    source = bindings.emit(result)[1]
                    self.assertIn("nx_stm32_adc_prepare", source)
                    self.assertIn(f"= {{ {channel}u }};", source)
                    self.assertNotIn("ADC1->", source)
                    claims = {claim.key for claim in result.resource_plan.claims
                              if claim.owner == "input"}
                    self.assertIn("pin:" + self.PINS[channel], claims)
                    self.assertNotIn("dma:DMA2:stream4", claims)

    def test_full_scan_fixtures_preserve_order_and_claim_all_three_banks(self):
        directory = gate.ROOT / "tests/contracts/adc_external_assembly"
        for part in ("ve", "zg"):
            with self.subTest(part=part):
                result = gate.resolve_ir(directory / f"{part}-scan.toml")
                selected, = result.controllers
                self.assertEqual(selected.options.channels,
                                 tuple(reversed(range(16))))
                claims = {claim.key for claim in result.resource_plan.claims
                          if claim.owner == "analog"}
                self.assertEqual({key for key in claims if key.startswith("pin:")},
                                 {"pin:" + pin for pin in self.PINS})
                self.assertIn("nx_stm32_adc_prepare", bindings.emit(result)[1])

    def test_route_bank_selects_only_authored_channels_before_resource_claims(self):
        fixture = gate.ROOT / "tests/contracts/adc_external_assembly"
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(fixture / "ve_board/board.json")
            (directory / "board.json").write_text(json.dumps(board))
            source = (fixture / "ve-scan.toml").read_text()
            source = source.replace('board_package = "ve_board"',
                                    'board_package = "."')
            source = source.replace(
                'channels = [15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0]',
                'channels = [15, 9, 3, 10, 8, 0, 15]')
            source = source.replace(
                'sample_times = [7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7]',
                'sample_times = [7, 7, 7, 7, 7, 7, 7]')
            assembly = directory / "assembly.toml"
            assembly.write_text(source)
            result = gate.resolve_ir(assembly)
            selected, = result.controllers
            self.assertEqual(selected.options.channels, (15, 9, 3, 10, 8, 0, 15))
            expected = {"PC5", "PB1", "PA3", "PC0", "PB0", "PA0"}
            self.assertEqual({pin["pin"] for pin in selected.pins}, expected)
            claims = {claim.key for claim in result.resource_plan.claims
                      if claim.owner == "analog" and claim.key.startswith("pin:")}
            self.assertEqual(claims, {"pin:" + pin for pin in expected})
            generated = bindings.emit(result)[1]
            self.assertIn(".channel_count = 7u", generated)
            self.assertIn(".pin_count = 6u", generated)
            self.assertEqual(len(result.routes["adc1-scan-external16"]["pins"]), 16)


if __name__ == "__main__":
    unittest.main()
