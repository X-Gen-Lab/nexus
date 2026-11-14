"""Capture and I2S planning follows package facts and semantic route roles."""
import copy
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CaptureAudioPolicyTests(unittest.TestCase):
    def policy(self, renamed=False):
        from tools.configure.policies.capture_audio import CaptureAudioPolicy
        facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings/capture_audio_policy.json").read_text())["domains"]["capture_audio"]
        if renamed:
            facts = copy.deepcopy(facts)
            facts["capture"]["controllers"] = ["CAMERA_PORT"]
            facts["audio"]["controllers"]["SERIAL_AUDIO"] = facts["audio"]["controllers"].pop("I2S2")
            facts["audio"]["external_sources"] = ["quartz"]
            facts["audio"]["clock_groups"] = {"serializer": ["SERIAL_AUDIO", "I2S3"]}
        return CaptureAudioPolicy.from_validated(facts)

    @staticmethod
    def capture_route(controller="CAMERA_PORT"):
        return dict(id="a-board-choice", kind="capture", controller=controller,
                    pins=[dict(pin="PB1", function="pclk", af=13)] +
                         [dict(pin="PC" + str(i), function="d" + str(i), af=13) for i in range(8)],
                    dma=["DMA2:stream7:channel1"])

    @staticmethod
    def camera():
        return dict(mode="snapshot-raw", pixel_clock_hz=24000000,
                    poll_limit=20, frame_divisor=1, pclk_falling=False,
                    hsync_high=False, vsync_high=False, irq_priority=5,
                    codes=[1, 2, 3, 4])

    @staticmethod
    def audio_route():
        return dict(id="board-audio", kind="audio", controller="SERIAL_AUDIO",
                    pins=[dict(pin="PE" + str(i), function=role, af=5) for i, role in enumerate(("ws", "ck", "sd"))],
                    mode_dma={"master-tx": ["DMA2:stream6:channel7"]})

    @staticmethod
    def audio():
        return dict(mode="master-tx", standard="philips", sample_bits=24,
                    slot_bits=32, mclk=False, cpol_high=False,
                    poll_limit=20, irq_priority=5, sample_rate_hz=48000,
                    pll_n=192, pll_r=2, max_error_ppm=10000)

    def test_capture_uses_signal_roles_without_route_id_or_fixed_gpio_pads(self):
        policy = self.policy(renamed=True)
        policy.validate(self.camera(), self.capture_route())
        resolved = policy.resolve_fields(self.camera(), self.capture_route(), {"core_hz": 168000000}, None)
        self.assertEqual((8, True), (resolved["bits"], resolved["embedded"]))
        self.assertEqual(("DMA2:stream7:channel1",), policy.dma_paths(self.capture_route(), "snapshot-raw"))

    def test_audio_uses_declared_dma_and_the_selected_system_clock_plan(self):
        policy = self.policy(renamed=True)
        policy.validate(self.audio(), self.audio_route())
        resolved = policy.resolve_fields(self.audio(), self.audio_route(),
            {"input_hz": 8000000}, {"parameters": {"pll_m": 8, "source": "quartz", "input_hz": 8000000}})
        self.assertEqual((15, True, 1500000, 31), (resolved["divider"], resolved["odd"], resolved["rate_numerator"], resolved["rate_denominator"]))
        self.assertEqual(("DMA2:stream6:channel7",), policy.dma_paths(self.audio_route(), "master-tx"))
        self.assertTrue(resolved["pll_hse"])
        with self.assertRaisesRegex(ValueError, "System"):
            policy.resolve_fields(self.audio(), self.audio_route(), {"input_hz": 8000000}, None)

    def test_wrong_roles_widths_dma_count_and_clock_ratio_fail_closed(self):
        policy = self.policy(renamed=True)
        bad = self.capture_route()
        bad["pins"] = bad["pins"][:-1]
        with self.assertRaisesRegex(ValueError, "route|width"):
            policy.validate(self.camera(), bad)
        with self.assertRaisesRegex(ValueError, "HCLK"):
            policy.resolve_fields(self.camera(), self.capture_route(), {"core_hz": 40000000}, None)
        bad = self.audio_route()
        bad["mode_dma"]["master-tx"] *= 2
        with self.assertRaisesRegex(ValueError, "DMA"):
            policy.dma_paths(bad, "master-tx")

    def test_all_finite_continuous_and_frame_modes_preserve_their_contracts(self):
        from tools.configure.policies.capture_audio import AUDIO_MODES, CAPTURE_MODES
        policy = self.policy(renamed=True)
        route = self.capture_route()
        route["pins"][1:1] = [dict(pin="PB2", function="hsync", af=13),
                              dict(pin="PB3", function="vsync", af=13)]
        for mode in CAPTURE_MODES:
            with self.subTest(capture=mode):
                selected = {key: value for key, value in self.camera().items()
                            if key != "codes"}
                selected["mode"] = mode
                if mode in {"raw-blocks", "jpeg-blocks"}:
                    selected["max_service_gap_us"] = 10
                policy.validate(selected, route)
                self.assertFalse(policy.resolve_fields(selected, route,
                    {"core_hz": 168000000}, None)["embedded"])
        for mode in AUDIO_MODES:
            with self.subTest(audio=mode):
                selected = {**self.audio(), "mode": mode}
                route = self.audio_route()
                if mode.startswith("slave-"):
                    for key in ("sample_rate_hz", "pll_n", "pll_r", "max_error_ppm"):
                        del selected[key]
                if mode.endswith("-blocks"):
                    selected.update(max_service_gap_us=10, max_sample_rate_hz=96000)
                if "duplex" in mode:
                    route["pins"].append(dict(pin="PE3", function="ext_sd", af=6))
                policy.validate(selected, route)
                plan = policy.resolve_fields(selected, route, {"input_hz": 8000000},
                    {"parameters": {"pll_m": 8, "source": "quartz", "input_hz": 8000000}})
                self.assertEqual(mode.endswith("-blocks"), plan["continuous"])

    def test_shared_clock_group_requires_compatible_master_tuples(self):
        policy = self.policy()
        owners = [dict(kind="audio", controller="I2S2", mode="master-tx",
                       pll_n=192, pll_r=2),
                  dict(kind="audio", controller="I2S3", mode="master-rx",
                       pll_n=192, pll_r=2)]
        policy.validate_controller_set(owners)
        owners[1]["pll_n"] = 256
        with self.assertRaisesRegex(ValueError, "compatible|shared"):
            policy.validate_controller_set(owners)
        owners[1].update(mode="slave-rx", pll_n=0, pll_r=0)
        policy.validate_controller_set(owners)


if __name__ == "__main__":
    unittest.main()
