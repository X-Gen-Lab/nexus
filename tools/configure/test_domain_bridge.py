"""Public provider hooks delegate only to the selected immutable domains."""
from types import SimpleNamespace
import unittest

from tools.configure.domain_bridge import DomainBridge
from tools.configure.domain_policies import DomainPolicies
from tools.configure.test_domain_policies import domain_facts, physical_facts


class Provider(DomainBridge):
    def __init__(self, domains):
        self.domain_policies = domains
        self.hardware_facts = SimpleNamespace(
            ADC_INTERNAL_CHANNELS={"CONVERTER_A": (16,)},
            CONTROLLER_BINDINGS=(), PWM_PIN_CHANNELS={}, PWM_TIMERS={})
        self.soc = {"controllers": {"SERIAL_A": {"clock_hz": 24000000}}}
        self.package = SimpleNamespace(family="unrelated", vendor="other")

    def binding(self, kind, mode, controller=None):
        if controller is None:
            raise ValueError("Controller is required for unique package binding selection")
        return SimpleNamespace(limits={})

    def mode_implementation(self, kind, mode, controller=None):
        if controller is None:
            raise ValueError("Controller is required for unique package binding selection")
        return "selected_c_driver"


class DomainBridgeContracts(unittest.TestCase):
    def provider(self):
        return Provider(DomainPolicies.from_validated(None, physical_facts(),
            {"domains": domain_facts()}))

    def test_selected_properties_are_explicit_and_never_fall_back_to_a_family(self):
        provider = self.provider()
        self.assertIs(provider.UART_FORMATS, provider.domain_policies.UART_FORMATS)
        self.assertEqual(provider.ADC_INTERNAL_CHANNELS, {"CONVERTER_A": (16,)})
        self.assertIsNone(provider.PWM_SHARED)
        with self.assertRaises(AttributeError):
            provider.an_undeclared_provider_attribute

    def test_dma_directions_are_selected_once_and_cannot_share_a_lane(self):
        provider = self.provider()
        route = {"kind": "uart", "controller": "SERIAL_A",
                 "dma": ["DMA1:stream0:channel2", "DMA2:stream1:channel3"]}
        provider.validate_dma(route, "dma-duplex", lambda value: value)
        self.assertEqual(provider.dma_irq("DMA2:stream1:channel3"),
                         "irq:DMA2_Stream1")
        route["dma"][1] = "DMA1:stream0:channel3"
        with self.assertRaisesRegex(ValueError, "alias"):
            provider.validate_dma(route, "dma-duplex", lambda value: value)
        route["dma"] = ["DMA1:stream0:channel2"]
        with self.assertRaisesRegex(ValueError, "directions"):
            provider.validate_dma(route, "dma-duplex", lambda value: value)

    def test_framing_and_os_contract_are_not_replaced_by_a_second_8n1_gate(self):
        provider = self.provider()
        provider.validate_selection({"mode": "irq-byte-event", "baud": 9600,
            "calls_os": True, "frame": {"data_bits": 7, "parity": "even",
            "stop_bits": 1, "flow": "none"}},
            {"kind": "uart", "controller": "SERIAL_A", "pins": []})

    def test_absent_clock_uses_fixed_explicit_controller_facts(self):
        provider = self.provider()
        self.assertEqual(provider.controller_clock_hz(
            SimpleNamespace(clock={"core_hz": 48000000}), "SERIAL_A"), 24000000)
        self.assertTrue(provider.shares_irq("exti", "irq:LINES_5_9"))
        self.assertFalse(provider.shares_irq("uart", "irq:LINES_5_9"))


if __name__ == "__main__":
    unittest.main()
