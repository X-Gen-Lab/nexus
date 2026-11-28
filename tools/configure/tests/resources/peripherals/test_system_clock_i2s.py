"""Fixed direct SYSCLK and route-fixed pads for portable finite I2S."""
import unittest

from tools.configure.pinmux import FixedRoutePinMuxPolicy
from tools.configure.policies.capture_audio import CaptureAudioPolicy


class FixedSystemClockI2sTests(unittest.TestCase):
    def setUp(self):
        self.policy = CaptureAudioPolicy.from_validated({
            "mechanism": "i2s-system-clock-v1",
            "controllers": ["I2S2"],
            "source": "hsi",
            "nominal_hz": 8000000,
            "sample_rate_hz": [8000, 96000],
            "prescaler": [4, 511],
            "format": {"standard": "philips", "sample_bits": 16,
                       "slot_bits": 16, "cpol_high": False},
        }, pinmux=FixedRoutePinMuxPolicy())
        self.route = {"kind": "audio", "controller": "I2S2",
                      "pins": [{"function": role} for role in ("ws", "ck", "sd")]}
        self.selected = {"mode": "master-tx", "standard": "philips",
                         "sample_bits": 16, "slot_bits": 16,
                         "mclk": False, "cpol_high": False, "irq_priority": 5,
                         "sample_rate_hz": 16000, "max_error_ppm": 23438}
        self.clock = {"source": "hsi", "core_hz": 8000000, "input_hz": 8000000}

    def test_exact_rate_and_odd_divisor(self):
        self.policy.validate(self.selected, self.route)
        plan = self.policy.resolve_fields(self.selected, self.route, self.clock,
                                          {"parameters": None})
        self.assertEqual((plan["rate_numerator"], plan["rate_denominator"],
                          plan["divider"], plan["odd"]), (15625, 1, 8, False))
        selected = dict(self.selected, sample_rate_hz=11025, max_error_ppm=20000)
        plan = self.policy.resolve_fields(selected, self.route, self.clock, {})
        self.assertEqual((plan["rate_numerator"], plan["rate_denominator"],
                          plan["divider"], plan["odd"]), (250000, 23, 11, True))

    def test_rejects_unreviewed_wire_source_and_too_tight_nominal_error(self):
        for invalid in (dict(self.selected, mclk=True),
                        dict(self.selected, pll_n=192),
                        dict(self.selected, mode="slave-tx"),
                        dict(self.selected, standard="pcm-long")):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                self.policy.validate(invalid, self.route)
        with self.assertRaisesRegex(ValueError, "direct SYSCLK"):
            self.policy.resolve_fields(self.selected, self.route,
                dict(self.clock, core_hz=4000000), {"parameters": None})
        with self.assertRaisesRegex(ValueError, "ppm"):
            self.policy.resolve_fields(dict(self.selected, max_error_ppm=23437),
                                       self.route, self.clock, {})


class FixedSystemClockI2sAssemblyTests(unittest.TestCase):
    def test_exact_controller_and_dma_lanes_are_exclusive(self):
        from pathlib import Path
        import json
        import shutil
        import tempfile

        from tools.configure.configure import resolve_ir

        root = Path(__file__).resolve().parents[5]
        fixture = root / "tests/integration/assemblies/stm32f1_i2s2"
        for resource in ("controller:SPI2", "dma:DMA1:channel5"):
            with self.subTest(resource=resource), tempfile.TemporaryDirectory() as temporary:
                copy = Path(temporary) / fixture.name
                shutil.copytree(fixture, copy)
                board_path = copy / "board/board.json"
                board = json.loads(board_path.read_bytes())
                board["reserved_resources"].append(resource)
                board_path.write_text(json.dumps(board), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "Resource conflict " + resource):
                    resolve_ir(copy / "f103ze-i2s2.toml", root)
