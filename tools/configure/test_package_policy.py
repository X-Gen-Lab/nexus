"""Reviewed package policy keeps family facts outside the shared resolver."""
from dataclasses import FrozenInstanceError
import unittest


class PackagePolicyTests(unittest.TestCase):
    def policy(self, **changes):
        from tools.configure.package_policy import BindingPolicy
        value = {
            "schema_version": 1, "custom_clock_profile": "explicit-run",
            "system": {
                "oscillator_resources": {"hse": ["pin:PH0", "pin:PH1"]},
                "timebase": {"timer": "TIM5", "irq": "TIM5_IRQn",
                             "gate": "RCC_APB1ENR_TIM5EN",
                             "reset": "RCC_APB1RSTR_TIM5RST",
                             "handler": "TIM5_IRQHandler",
                             "clock_domain": "apb1_timer", "counter_bits": 32}},
            "mode_exclusions": [{"device": "part", "kind": "flash",
                                 "option": "maintenance", "equals": True,
                                 "message": "Reviewed maintenance exclusion"}],
        }
        value.update(changes)
        return BindingPolicy.from_validated(value)

    def test_resources_and_custom_clock_are_explicit_immutable_facts(self):
        selected = self.policy()
        self.assertEqual(selected.oscillator_resources("hse"),
                         ("pin:PH0", "pin:PH1"))
        self.assertEqual(selected.oscillator_resources("hsi"), ())
        self.assertEqual(selected.custom_clock_profile, "explicit-run")
        with self.assertRaises(FrozenInstanceError):
            selected.custom_clock_profile = "changed"

    def test_exclusion_uses_exact_device_and_option_value(self):
        selected = self.policy()
        selected.validate_selection("other", "flash", {"maintenance": True})
        selected.validate_selection("part", "flash", {"maintenance": False})
        with self.assertRaisesRegex(ValueError, "Reviewed maintenance"):
            selected.validate_selection("part", "flash", {"maintenance": True})

    def test_timebase_generates_only_restricted_symbols(self):
        selected = self.policy()
        lines = selected.header_lines("NEXUS_STM32")
        self.assertIn("#define NEXUS_STM32_TIMEBASE_TIMER TIM5", lines)
        self.assertIn("#define NEXUS_STM32_TIMEBASE_COUNTER_BITS 32u", lines)
        with self.assertRaises(ValueError):
            self.policy(system={"oscillator_resources": {}, "timebase": {
                "timer": "TIM5; call()", "irq": "TIM5_IRQn",
                "gate": "gate", "reset": "reset", "handler": "handler",
                "clock_domain": "apb1_timer", "counter_bits": 32}})

    def test_unknown_fields_and_untyped_conditions_reject(self):
        for changes in ({"unknown": 0}, {"schema_version": True},
                        {"mode_exclusions": [{"device": "part", "kind": "flash",
                          "option": "maintenance", "equals": {"eval": "code"},
                          "message": "not a scalar"}]}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.policy(**changes)


if __name__ == "__main__":
    unittest.main()
