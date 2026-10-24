"""Clock input identity never invents an external Board oscillator."""
from tools.configure.providers.descriptor import source_directory
import unittest

from tools.configure import configure as gate


class ClockSourceContractTests(unittest.TestCase):
    def test_existing_hse_facts_accept_one_explicit_source_identity(self):
        for family, part in (("stm32f407", "STM32F407ZGT6"),
                             ("gd32f470", "GD32F470ZGT6"),
                             ("stm32g474", "STM32G474RET6")):
            with self.subTest(family=family):
                soc = gate.load(gate.ROOT / source_directory(family) / "soc.json")
                for profile in soc["clock_profiles"].values():
                    profile["source"] = "hse"
                    profile["input_hz"] = profile.pop("hse_hz")
                gate.validate_soc(soc, part)

    def test_legacy_hse_normalizes_once_without_mutating_input(self):
        from tools.configure import clocks
        legacy = {"hse_hz": 8000000, "core_hz": 168000000}
        normalized = clocks.normalize(legacy)
        self.assertEqual(normalized, {"source": "hse", "input_hz": 8000000,
                                      "core_hz": 168000000})
        self.assertEqual(legacy["hse_hz"], 8000000)
        self.assertNotIn("hse_hz", normalized)

    def test_internal_hsi_and_msis_do_not_require_board_hse(self):
        from tools.configure import clocks
        for source, hz in (("hsi", 64000000), ("msis", 4000000)):
            profile = clocks.normalize({"source": source, "input_hz": hz,
                                        "core_hz": hz})
            clocks.match_board(profile, {})
            clocks.match_board(profile, {"hse_hz": 8000000})
            self.assertNotIn("hse_hz", profile)

    def test_external_input_requires_exact_reviewed_board_oscillator(self):
        from tools.configure import clocks
        profile = clocks.normalize({"source": "hse", "input_hz": 8000000,
                                    "core_hz": 168000000})
        clocks.match_board(profile, {"hse_hz": 8000000})
        for board in ({}, {"hse_hz": 24000000}):
            with self.subTest(board=board), self.assertRaises(ValueError):
                clocks.match_board(profile, board)

    def test_dual_authority_unknown_source_and_noninteger_input_fail_closed(self):
        from tools.configure import clocks
        valid = {"source": "hsi", "input_hz": 64000000, "core_hz": 64000000}
        for changed in ({**valid, "hse_hz": 8000000},
                        {**valid, "source": "nearest-clock"},
                        {**valid, "input_hz": False},
                        {**valid, "input_hz": 64000000.0},
                        {**valid, "input_hz": 0},
                        {"source": "hsi", "core_hz": 64000000},
                        {"input_hz": 64000000, "core_hz": 64000000}):
            with self.subTest(profile=changed), self.assertRaises(ValueError):
                clocks.normalize(changed)


if __name__ == "__main__":
    unittest.main()
