"""Project analog channel decisions from fixed physical routes."""
from types import SimpleNamespace
import unittest

from tools.configure import adc
from tools.configure.pinmux import FixedRoutePinMuxPolicy


class FixedAdcRouteTests(unittest.TestCase):
    def test_single_shot_uses_the_package_route_without_dma_metadata(self):
        provider = SimpleNamespace(PINMUX_POLICY=FixedRoutePinMuxPolicy(), LIMITS={})
        route = {"kind": "adc", "controller": "ADC1",
                 "pins": [{"pin": "PA0", "function": "in0"}]}
        selection = {"mode": "single-shot", "channels": [0],
                     "sample_times": [5], "reference_mv": 3300}
        adc.validate_selection(selection, route, provider)
        self.assertEqual(adc.selected_route(selection, route, provider)["pins"], route["pins"])
        with self.assertRaisesRegex(ValueError, "channel sequence"):
            adc.validate_selection({**selection, "channels": [1]}, route, provider)
        for role in ("analog", "in00", "in16"):
            malformed = {**route, "pins": [{"pin": "PA0", "function": role}]}
            with self.subTest(role=role), self.assertRaisesRegex(ValueError, "analog channel"):
                adc.validate_selection(selection, malformed, provider)
