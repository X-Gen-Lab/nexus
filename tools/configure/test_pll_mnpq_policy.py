"""Pure PLL domain arithmetic is driven by facts, without register encoding."""
import copy
from types import SimpleNamespace
import unittest

from tools.configure.domains.pll_mnpq import PllMnpqPolicy


def pll_facts():
    parameters = dict(source="hse", input_hz=8000000, pll_m=8, pll_n=336,
        pll_p=2, pll_q=7, ahb_div=1, apb1_div=4, apb2_div=2,
        adc_div=4, supply_mv=3300, css=False)
    return {
        "model": "pll-mnpq", "custom_profile": "custom",
        "default_parameters": parameters,
        "profiles": {"reviewed": {"clock": dict(source="hse", input_hz=8000000,
            core_hz=168000000, apb1_hz=42000000, apb2_hz=84000000),
            "parameters": parameters}},
        "ranges": {"input_hz": [4000000, 26000000], "pll_m": [2, 63],
            "pll_n": [192, 432], "pll_q": [2, 15], "supply_mv": [2700, 3600],
            "pll_input_hz": [1000000, 2000000],
            "pll_vco_hz": [192000000, 432000000]},
        "dividers": {"pll_p": [2, 4, 6, 8],
            "ahb_div": [1, 2, 4, 8, 16, 64, 128, 256, 512],
            "apb1_div": [1, 2, 4, 8, 16], "apb2_div": [1, 2, 4, 8, 16],
            "adc_div": [2, 4, 6, 8]},
        "internal_sources": {"hsi": 16000000}, "external_sources": ["hse"],
        "maximum_hz": {"pll_sysclk": 168000000, "core": 168000000,
            "apb1": 50000000, "apb2": 100000000, "adc": 36000000},
        "timer_domains": {
            "apb1_timer": {"parent": "apb1", "divider": "apb1_div", "multiplier": 2},
            "apb2_timer": {"parent": "apb2", "divider": "apb2_div", "multiplier": 2}},
        "timebase": {"domain": "apb1_timer", "tick_hz": 1000000},
        "flash_latency_step_hz": 30000000,
        "auxiliary": {"m": [2, 63], "n": [50, 432],
            "input_hz": [1000000, 2000000], "vco_hz": [100000000, 432000000],
            "output_max_hz": 216000000,
            "outputs": {
                "sai1": {"divider": "q", "range": [2, 15],
                    "post_dividers": list(range(1, 33)), "frequency": "kernel_hz"},
                "ltdc": {"divider": "r", "range": [2, 7],
                    "post_dividers": [2, 4, 8, 16], "frequency": "pixel_hz"}}},
        "controller_domains": {"SERIAL_A": "apb1", "COUNTER_A": "apb2_timer"},
        "timer_clock_domains": {"COUNTER_A": "apb2_timer"},
        "external_clock_modes": {},
        "consumers": {"usb": [], "ethernet_min_hz": 25000000,
            "entropy_hz": 48000000, "adc_min_hz": 600000,
            "uart_divisor": [16, 65535], "timer_prescaler_max": 65536,
            "i2c_quantum_hz": 1000000, "i2c_standard_min_hz": 2000000,
            "i2c_fast_min_hz": 4000000, "i2c_fast_threshold_hz": 100000,
            "auxiliary_audio_controllers": [], "auxiliary_display_controller": None},
        "reviews": {},
    }


class PllMnpqPolicyTests(unittest.TestCase):
    def setUp(self):
        self.facts = pll_facts()
        self.policy = PllMnpqPolicy.from_validated(self.facts)

    def test_named_profile_has_one_exact_domain_plan_without_encoded_registers(self):
        selected = self.policy.plan("reviewed", self.facts["profiles"]["reviewed"]["clock"])
        self.assertEqual(selected["domains"]["core"]["nominal_hz"], 168000000)
        self.assertEqual(selected["domains"]["apb1_timer"]["nominal_hz"], 84000000)
        self.assertEqual(selected["domains"]["pll48"]["nominal_hz"], 48000000)
        self.assertFalse({"pllcfgr", "cfgr", "sw"} & selected["parameters"].keys())
        self.assertFalse(selected["physical_qualified"])

    def test_direct_input_does_not_leave_an_unselected_pll_domain(self):
        parameters = dict(self.facts["default_parameters"], source="hsi",
            input_hz=16000000, pll_m=16, sysclk_source="input",
            apb1_div=1, apb2_div=1)
        selected = self.policy.plan("custom", parameters)
        self.assertEqual(selected["domains"]["core"]["nominal_hz"], 16000000)
        self.assertEqual(selected["domains"]["pll48"]["nominal_hz"], 0)

    def test_named_and_custom_paths_reject_inexact_or_unreviewed_values(self):
        for parameters in (
            dict(self.facts["default_parameters"], pll_m=9),
            dict(self.facts["default_parameters"], apb1_div=3),
            dict(self.facts["default_parameters"], css=1),
            dict(self.facts["default_parameters"], source="hsi"),
        ):
            with self.subTest(parameters=parameters), self.assertRaises(ValueError):
                self.policy.plan("custom", parameters)
        summary = dict(self.facts["profiles"]["reviewed"]["clock"], core_hz=167000000)
        with self.assertRaises(ValueError):
            self.policy.plan("reviewed", summary)

    def test_auxiliary_outputs_share_explicit_input_and_independent_post_dividers(self):
        parameters = dict(self.facts["default_parameters"], pll_sai={
            "m": 8, "n": 192, "sai1": {"q": 2, "post_divider": 3},
            "ltdc": {"r": 4, "post_divider": 2}})
        selected = self.policy.plan("custom", parameters)
        self.assertEqual(selected["pll_sai"]["sai1"]["kernel_hz"], 32000000)
        self.assertEqual(selected["pll_sai"]["ltdc"]["pixel_hz"], 24000000)

    def test_controller_domains_are_explicit_and_do_not_depend_on_mcu_names(self):
        result = SimpleNamespace(clock={"apb1_hz": 42000000},
            clock_plan=None, controllers=())
        self.assertEqual(self.policy.controller_hz(result, "SERIAL_A"), 42000000)
        self.assertIsNone(self.policy.controller_hz(result, "USART1"))
        self.assertEqual(self.policy.timer_domain("COUNTER_A"), "apb2_timer")
        with self.assertRaises(ValueError):
            self.policy.timer_domain("TIM1")

    def test_policy_snapshot_is_immutable_and_unknown_models_do_not_fall_back(self):
        self.facts["maximum_hz"]["core"] = 1
        self.assertGreater(self.policy.data["maximum_hz"]["core"], 1)
        for changes in ({"model": "new_mcu"}, {"unknown": 1}):
            malformed = dict(pll_facts(), **changes)
            with self.assertRaises(ValueError):
                PllMnpqPolicy.from_validated(malformed)
        malformed = copy.deepcopy(pll_facts())
        malformed["dividers"]["pll_p"] = [2, 2]
        with self.assertRaises(ValueError):
            PllMnpqPolicy.from_validated(malformed)

    def test_inconsistent_domain_facts_reject_before_planning(self):
        for change in (
            lambda facts: facts["timer_domains"]["apb1_timer"].update(divider="pll_p"),
            lambda facts: facts["auxiliary"]["outputs"]["sai1"].update(frequency="pixel_hz"),
        ):
            facts = pll_facts()
            change(facts)
            with self.subTest(facts=facts), self.assertRaises(ValueError):
                PllMnpqPolicy.from_validated(facts)


if __name__ == "__main__":
    unittest.main()
