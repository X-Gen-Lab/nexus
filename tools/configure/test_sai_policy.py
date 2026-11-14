"""Free-frame SAI planning is reusable through reviewed package facts."""
import copy
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SaiPolicyTests(unittest.TestCase):
    def policy(self):
        from tools.configure.policies.sai import SaiPolicy
        facts = json.loads((ROOT / "soc/stm32/stm32f4/bindings/sai_policy.json").read_text())["domains"]["sai"]
        facts = copy.deepcopy(facts)
        facts["controllers"] = {"SERIAL_FRAME": facts["controllers"]["SAI1"]}
        facts["controllers"]["SERIAL_FRAME"]["dma_roles"]["A"] = ["DMA2:stream6:channel7"]
        return SaiPolicy.from_validated(facts)

    @staticmethod
    def route():
        return dict(kind="audio", controller="SERIAL_FRAME",
                    pins=[dict(pin="PA"+str(n), function=role, af=6) for n, role in enumerate(("fs_a", "ck_a", "sd_a"))],
                    mode_dma={"master-tx": ["DMA2:stream6:channel7"]})

    @staticmethod
    def selection():
        return dict(mode="master-tx", sample_bits=24, slot_bits=32, slots=2,
                    frame_sync_bits=32, frame_sync_high=False,
                    frame_sync_before=True, frame_sync_channels=True,
                    first_lsb=False, sample_rising=True,
                    max_bit_clock_hz=3072000, poll_limit=10, irq_priority=5)

    def test_renamed_controller_and_board_pads_preserve_selected_frame_semantics(self):
        policy = self.policy()
        self.assertTrue(policy.handles("SERIAL_FRAME"))
        self.assertFalse(policy.handles("SAI1"))
        policy.validate(self.selection(), self.route())
        result = policy.resolve_fields(self.selection(), self.route(),
            {"apb2_hz": 84000000}, {"pll_sai": {"sai1": {"kernel_hz": 12288000}}})
        self.assertEqual(("A", 3, False), (result["source_block"], result["active_slots"], result["continuous"]))
        self.assertEqual(("DMA2:stream6:channel7",), policy.dma_paths(self.route(), "master-tx"))

    def test_sparse_slots_and_continuity_preserve_explicit_service_bounds(self):
        policy = self.policy()
        selected = {**self.selection(), "mode": "slave-tx-blocks", "slots": 4,
                    "active_slots": 9, "max_service_gap_us": 10,
                    "frame_sync_channels": False}
        policy.validate(selected, self.route())
        result = policy.resolve_fields(selected, self.route(), {"apb2_hz": 84000000}, None)
        self.assertEqual((9, True, 10), (result["active_slots"], result["continuous"], result["max_service_gap_us"]))
        with self.assertRaisesRegex(ValueError, "MCLK|role"):
            policy.validate({**selected, "mclk": True}, self.route())


if __name__ == "__main__":
    unittest.main()
