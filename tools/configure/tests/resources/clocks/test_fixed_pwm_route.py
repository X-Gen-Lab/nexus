"""Fixed-route timer channels come from the reviewed signal role."""
from types import SimpleNamespace
import unittest

from tools.configure.domain_policies import DomainPolicies
from tools.configure.pinmux import FixedRoutePinMuxPolicy, NumericPinMuxPolicy


class FixedPwmRouteTests(unittest.TestCase):
    def test_fixed_route_publishes_the_selected_channel(self):
        context = SimpleNamespace(provider_context=SimpleNamespace(
            provider=SimpleNamespace(PWM_PIN_CHANNELS={})))
        policy = DomainPolicies(PINMUX_POLICY=FixedRoutePinMuxPolicy())
        item = SimpleNamespace(kind="pwm", controller="TIM3",
                               pins=({"pin": "PA6", "function": "ch1"},))
        self.assertEqual(policy.binding_values(item, context)["channel"], 1)
        for invalid in ("ch0", "ch5", "ch01", "output"):
            item.pins = ({"pin": "PA6", "function": invalid},)
            with self.subTest(role=invalid), self.assertRaisesRegex(ValueError, "channel"):
                policy.binding_values(item, context)

    def test_numeric_pinmux_keeps_its_independent_reviewed_channel(self):
        context = SimpleNamespace(provider_context=SimpleNamespace(
            provider=SimpleNamespace(PWM_PIN_CHANNELS={("TIM3", "PA6"): 4})))
        policy = DomainPolicies(PINMUX_POLICY=NumericPinMuxPolicy((2,)))
        item = SimpleNamespace(kind="pwm", controller="TIM3",
                               pins=({"pin": "PA6", "function": "ch1", "af": 2},))
        self.assertEqual(policy.binding_values(item, context)["channel"], 4)
