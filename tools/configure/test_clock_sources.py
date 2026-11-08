"""Clock input identity never invents an external Board oscillator."""
from tools.configure.tests.device_fixture import load_soc_facts
from tools.configure.providers.descriptor import source_directory
import unittest

from tools.configure import configure as gate


class ClockSourceContractTests(unittest.TestCase):


    def test_uart_spi_clock_domains_bind_to_reviewed_apb_facts(self):
        from tools.configure.stm32f4.hardware import CONTROLLER_BINDINGS
        soc = load_soc_facts(gate.ROOT / source_directory("stm32f4") / "soc.json")
        for binding in CONTROLLER_BINDINGS:
            if binding.kind not in {"uart", "spi"}:
                continue
            with self.subTest(controller=binding.controller):
                domain = soc["controllers"][binding.controller]["clock_domain"]
                self.assertEqual(domain, f"apb{binding.apb}")
                for clock in soc["clock_profiles"].values():
                    self.assertGreater(clock[domain + "_hz"], 0)
        gate.validate_soc(soc, "STM32F407ZGT6")

    def test_missing_uart_spi_clock_domain_is_rejected(self):
        for controller in ("USART1", "SPI2"):
            soc = load_soc_facts(gate.ROOT / source_directory("stm32f4") / "soc.json")
            soc["controllers"][controller].pop("clock_domain", None)
            with self.subTest(controller=controller), self.assertRaisesRegex(
                    ValueError, "clock domain"):
                gate.validate_soc(soc, "STM32F407ZGT6")

    def test_unknown_or_wrong_reviewed_clock_domain_is_rejected(self):
        for domain in ("ahb", "apb1", False):
            soc = load_soc_facts(gate.ROOT / source_directory("stm32f4") / "soc.json")
            soc["controllers"]["USART1"]["clock_domain"] = domain
            with self.subTest(domain=domain), self.assertRaisesRegex(
                    ValueError, "clock domain"):
                gate.validate_soc(soc, "STM32F407ZGT6")

    def test_existing_hse_facts_accept_one_explicit_source_identity(self):
        for family, part in (("stm32f4", "STM32F407ZGT6"),):
            with self.subTest(family=family):
                soc = load_soc_facts(gate.ROOT / source_directory(family) / "soc.json")
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
