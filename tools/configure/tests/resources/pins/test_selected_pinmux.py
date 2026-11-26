"""The selected package owns one numeric mux policy for all route admission."""
import json
import unittest

from tools.configure import configure
from tools.configure.package_policy import BindingPolicy
from tools.configure.tests.f4_fixture import provider


class SelectedPinMuxTests(unittest.TestCase):
    def document(self):
        return json.loads((configure.ROOT /
            "soc/stm32/stm32f4/facts/bindings.json").read_text())

    def test_one_selected_policy_is_shared_by_domain_and_private_bindings(self):
        selected = provider("STM32F407VET6")
        self.assertEqual(selected.PINMUX_POLICY.selectors, tuple(range(16)))
        self.assertIs(selected.PINMUX_POLICY, selected.binding_policy.pinmux_policy)

    def test_numeric_selector_range_comes_from_explicit_package_policy(self):
        value = self.document()
        value["pinmux_policy"] = {"encoding": "numeric", "selectors": list(range(32))}
        value["pinmux"]["USART1"] = [31]
        policy = BindingPolicy.from_validated(value)
        policy.validate_route({"controller": "USART1", "pins": [{"af": 31}]})
        with self.assertRaises(ValueError):
            policy.validate_route({"controller": "USART1", "pins": [{"af": 7}]})

    def test_unknown_encoding_and_undeclared_controller_selectors_reject(self):
        value = self.document()
        value["pinmux_policy"] = {"encoding": "register-script", "selectors": [0]}
        with self.assertRaisesRegex(ValueError, "encoding"):
            BindingPolicy.from_validated(value)
        value["pinmux_policy"] = {"encoding": "numeric", "selectors": [0]}
        with self.assertRaisesRegex(ValueError, "selector|alternate"):
            BindingPolicy.from_validated(value)

    def test_pwm_projection_uses_the_same_selector_policy_without_another_range(self):
        from tools.configure.hardware_bindings import _pwm
        from tools.configure.pinmux import NumericPinMuxPolicy
        timers, _ = _pwm({"COUNTER_A": {"apb": 1}},
            {"COUNTER_A": {"clock_hz": 24000000}},
            {"route": {"kind": "pwm", "controller": "COUNTER_A", "pins": [
                {"pin": "PAD_A", "function": "ch1", "af": 31}]}},
            {"pwm": [1, 4]}, NumericPinMuxPolicy((0, 31)))
        self.assertEqual(timers["COUNTER_A"].af, 31)


if __name__ == "__main__":
    unittest.main()
