"""Silicon narrowing uses explicit facts and stable protocol validators."""
from types import SimpleNamespace
import copy
import unittest

from tools.configure.policies.selection import SelectionPolicy


def facts():
    return {"schema_version": 1,
        "internal": [{"kind": "cipher", "modes": ["irq-blocks"],
                      "controller": "CIPHER_A", "irq_required": True}],
        "encoder": {"controllers": {"COUNTER_A": 5}},
        "i2c_target": {"controllers": ["TWI_A"], "alternate": 6},
        "block": {"controller": "CARD_A", "alternate": 7,
            "pins": {"1": ["PA1", "PA2"], "4": ["PA1", "PA2"],
                     "8": ["PA1", "PA2", "PA3"]},
            "max_blocks": 12, "poll_budget": 1000},
        "rtc": {"controller": "CALENDAR_A", "crystal_route": "xtal-route",
            "bypass_route": "external-route", "default_context": {
                "backend": "baremetal", "profile": "run-48"},
            "part_contexts": {}},
        "contexts": [], "dma": [], "pwm_period_max": {},
        "adc_internal_sample": 7,
        "i2c_nominal": {"clock_quantum_hz": 1000000,
            "clock_hz": [2000000, 50000000], "fast_min_hz": 4000000,
            "fast_threshold_hz": 100000, "maximum_hz": 400000,
            "standard_ceiling_hz": 88000, "standard_factor": 2,
            "fast_factor": 3, "standard_min_divisor": 4,
            "maximum_divisor": 4095, "rise_quantum_hz": 10000000,
            "rise_budget_hz": 1200000}}


class SelectionPolicyContracts(unittest.TestCase):
    def policy(self):
        return SelectionPolicy.from_validated(facts())

    def test_controller_names_are_author_data_and_not_vendor_guesses(self):
        selected = self.policy()
        hardware = SimpleNamespace(ADC_INTERNAL_CHANNELS={})
        selected.validate({"mode": "irq-blocks", "irq_priority": 3},
            {"kind": "cipher", "controller": "CIPHER_A", "pins": []}, hardware)
        with self.assertRaisesRegex(ValueError, "internal controller"):
            selected.validate({"mode": "irq-blocks", "irq_priority": 3},
                {"kind": "cipher", "controller": "OTHER", "pins": []}, hardware)
        encoder = {"mode": "quadrature", "counting": "x4", "filter_a": 0,
            "filter_b": 15, "invert_a": False, "invert_b": True, "exclusive": True}
        selected.validate(encoder, {"kind": "encoder", "controller": "COUNTER_A",
            "pins": [{"pin": "PC1", "function": "ti1", "af": 5},
                     {"pin": "PD2", "function": "ti2", "af": 5}]}, hardware)

    def test_card_width_and_budgets_are_data_but_protocol_semantics_are_fixed(self):
        selected = self.policy()
        options = {"mode": "sdio-poll", "bus_width": 4, "max_blocks": 12,
                   "poll_budget": 1000, "stop_timeout_us": 20}
        route = {"kind": "block", "controller": "CARD_A", "pins": [
            {"pin": "PA1", "af": 7}, {"pin": "PA2", "af": 7}]}
        selected.validate(options, route, None)
        options["max_blocks"] = 13
        with self.assertRaises(ValueError):
            selected.validate(options, route, None)
        options.update(max_blocks=12, bus_width=8)
        with self.assertRaisesRegex(ValueError, "MMC"):
            selected.validate(options, route, None)

    def test_rtc_context_and_route_remain_exact_without_part_name_branches(self):
        selected = self.policy()
        route = {"kind": "rtc", "controller": "CALENDAR_A", "id": "xtal-route",
                 "pins": []}
        selected.validate({"mode": "adopt-retained"}, route, None)
        selected.validate_context({}, route, backend="baremetal", clock_profile="run-48")
        with self.assertRaises(ValueError):
            selected.validate_context({}, route, backend="freertos", clock_profile="run-48")
        with self.assertRaises(ValueError):
            selected.validate({"mode": "adopt-retained", "lse_bypass": True}, route, None)

    def test_i2c_nominal_projection_uses_an_explicit_divider_model(self):
        selected = self.policy()
        item = SimpleNamespace(kind="i2c", integration=SimpleNamespace(clock_hz=42000000),
            options=SimpleNamespace(max_hz=400000))
        self.assertEqual(selected.selected_format(item, None)["fields"]["nominal_hz"],
                         388888)
        item.integration.clock_hz = 2500000
        with self.assertRaisesRegex(ValueError, "clock"):
            selected.selected_format(item, None)

    def test_malformed_author_records_reject_with_configuration_diagnostics(self):
        for mutate in (
            lambda value: value.update(internal=None),
            lambda value: value["encoder"].update(controllers=None),
            lambda value: value["rtc"]["default_context"].update(parameters=[]),
            lambda value: value["rtc"]["default_context"].update(forbid_auxiliary=1),
        ):
            value = copy.deepcopy(facts())
            mutate(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                SelectionPolicy.from_validated(value)


if __name__ == "__main__":
    unittest.main()
