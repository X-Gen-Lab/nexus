"""Domain contracts consume explicit facts without choosing an MCU family."""
from dataclasses import FrozenInstanceError
from types import SimpleNamespace
import copy
import unittest

from tools.configure.domain_policies import DomainPolicies


def domain_facts():
    return {
        "schema_version": 1,
        "uart_formats": {"words": ["SERIAL_A"], "irq_probe": True,
                         "byte_frames": [[8, "none"], [7, "even"]]},
        "stream_geometry": {"hardware_targets": 2, "spare_blocks": 1,
                            "counter_max": 65535},
        "can": {"irq_roles": ["tx", "rx0", "rx1", "error"],
                "fifo_order_supported": True},
        "usb_device": {"controller": "USB_A", "irq": 20,
            "clock_hz": 48000000, "fifo_words": 320, "endpoint_numbers": 4,
            "pins": [["PA1", "dm", 5], ["PA2", "dp", 5]],
            "resources": ["clock:USB_A"]},
        "ethernet": {"controller": "NET_A", "irq": 21,
            "clock_hz": 96000000, "pins": [["PB1", "mdio"]],
            "resources": ["clock:NET_A"], "alternate": 7,
            "memory_region": "shared_ram", "mii_pins": []},
        "timers": [{"kind": "timer_capture", "controller": "COUNTER_A",
            "vector": "COUNTER_A", "irq": 22, "clock_hz": 48000000,
            "routes": [{"channel": 1, "pins": [["PC1", "capture", 2]]}],
            "resources": ["timer:COUNTER_A"]}],
        "endpoints": {"spi_transaction": True,
            "spi_dma": {"controllers": ["SPI_A"], "mode": "dma"},
            "spi_dma_owner": {"contains": "sample_owner_contains",
                              "attach_wake": "sample_owner_attach_wake"},
            "spi_lsb_first": True, "i2c_ten_bit": True},
        "limits": {"uart_baud": [1200, 1000000], "timer_hz": 48000000},
        "capabilities": {"pin_electrical": True, "timebase_alarm": True,
            "spi_requires_endpoint": True, "flash_program_unit": 4,
            "flash_input_alignment": 1, "offset_image_alignment": 256,
            "offset_image_vector_symbol": "vectors"},
        "dma": {"vector": "Stream", "engines": [1, 2],
                "lanes": [0, 1, 2], "requests": [0, 1, 2, 3]},
        "shared_irqs": {"exti": ["irq:LINES_5_9"]},
    }


def physical_facts():
    return {"can_domain": {"controllers": ["BUS_A", "BUS_B"],
                           "primary": "BUS_A", "filter_banks": 14},
            "controllers": {name: {"kind": "can", "clock_hz": 24000000}
                            for name in ("BUS_A", "BUS_B")}}


class DomainPolicyContracts(unittest.TestCase):
    def load(self, facts=None):
        return DomainPolicies.from_validated(
            SimpleNamespace(family="unrelated_mcu", vendor="sample"), physical_facts(),
            {"domains": domain_facts() if facts is None else facts})

    def test_explicit_facts_construct_existing_narrow_domain_contracts(self):
        selected = self.load()
        self.assertEqual(selected.UART_FORMATS.words, ("SERIAL_A",))
        self.assertEqual(selected.STREAM_GEOMETRY.hardware_targets, 2)
        self.assertEqual(selected.CAN_CONTROLLERS, ("BUS_A", "BUS_B"))
        self.assertEqual(selected.CAN_INTEGRATION.irq_roles,
                         ("tx", "rx0", "rx1", "error"))
        self.assertEqual(selected.USB_DEVICE.controller, "USB_A")
        self.assertEqual(selected.ETHERNET.memory_region, "shared_ram")
        self.assertEqual(selected.TIMER_ADVANCED.binding(
            "timer_capture", "COUNTER_A").routes[0].pins,
            (("PC1", "capture", 2),))
        self.assertTrue(selected.ENDPOINT_POLICY.i2c_ten_bit)
        self.assertEqual(selected.DMA_TOPOLOGY, (frozenset((1, 2)), (0, 1, 2),
                                                (0, 1, 2, 3)))
        self.assertTrue(selected.shares_irq("exti", "irq:LINES_5_9"))
        self.assertFalse(selected.shares_irq("uart", "irq:LINES_5_9"))

    def test_missing_domains_never_inherit_first_provider_capabilities(self):
        selected = DomainPolicies.from_validated(
            SimpleNamespace(family="new_mcu", vendor="new_vendor"), {}, {})
        self.assertIsNone(selected.USB_DEVICE)
        self.assertIsNone(selected.ETHERNET)
        self.assertIsNone(selected.STREAM_GEOMETRY)
        self.assertIsNone(selected.TIMER_ADVANCED)
        self.assertEqual(selected.CAN_CONTROLLERS, ())
        self.assertFalse(selected.PIN_ELECTRICAL)
        self.assertFalse(selected.TIMEBASE_ALARM)

    def test_input_mutation_cannot_change_the_frozen_contract(self):
        facts = domain_facts()
        selected = self.load(facts)
        facts["uart_formats"]["words"].append("OTHER")
        facts["limits"]["uart_baud"][0] = 1
        self.assertEqual(selected.UART_FORMATS.words, ("SERIAL_A",))
        self.assertEqual(selected.LIMITS["uart_baud"], (1200, 1000000))
        with self.assertRaises(TypeError):
            selected.LIMITS["timer_hz"] = 1
        with self.assertRaises(FrozenInstanceError):
            selected.UART_FORMATS.irq_probe = False

    def test_one_timer_channel_may_have_multiple_reviewed_pin_routes(self):
        facts = domain_facts()
        facts["timers"][0]["routes"].append({
            "channel": 1, "pins": [["PD2", "capture", 3]]})
        binding = self.load(facts).TIMER_ADVANCED.binding(
            "timer_capture", "COUNTER_A")
        self.assertEqual(binding.route((("PD2", "capture", 3),)).channel, 1)

    def test_derived_binding_values_reuse_one_controller_order_and_route(self):
        selected = self.load()
        value = selected.binding_values(SimpleNamespace(kind="can", controller="BUS_B"))
        self.assertEqual(value["controller_index"], 1)
        item = SimpleNamespace(kind="timer_capture", controller="COUNTER_A",
            pins=({"pin": "PC1", "function": "capture", "af": 2},))
        self.assertEqual(selected.binding_values(item)["channel"], 1)
        with self.assertRaises(TypeError):
            value["controller_index"] = 0
        item.pins = ({"pin": "PD2", "function": "capture", "af": 2},)
        with self.assertRaises(ValueError):
            selected.binding_values(item)

    def test_unknown_domains_and_malformed_facts_fail_before_construction(self):
        for mutate in (
            lambda value: value.update(unknown={}),
            lambda value: value["uart_formats"].update(irq_probe=1),
            lambda value: value["uart_formats"].update(byte_frames=None),
            lambda value: value["uart_formats"].update(byte_frames=[[8, []]]),
            lambda value: value["stream_geometry"].update(counter_max=True),
            lambda value: value["timers"][0].update(irq=-1),
            lambda value: value["dma"].update(lanes=[0, 0]),
            lambda value: value["endpoints"].update(spi_dma={"mode": "dma"}),
        ):
            facts = copy.deepcopy(domain_facts())
            mutate(facts)
            with self.subTest(facts=facts), self.assertRaises(ValueError):
                self.load(facts)

    def test_execution_profile_is_one_public_semantic_key(self):
        selected = self.load()
        item = SimpleNamespace(kind="gpio")
        self.assertEqual(selected.execution_profile(item, ()), "default")
        item = SimpleNamespace(kind="uart", mode="irq-byte-event",
            options=SimpleNamespace(rx_profile="events", frame=None,
                                    irq=SimpleNamespace(probe=False)))
        self.assertEqual(selected.execution_profile(item, ()), "events-default")

    def test_private_binding_extensions_are_namespaced_and_deeply_immutable(self):
        class Policy:
            def binding_values(self, item, configuration=None):
                return {"adc_group": {"layout": {"capacity": configuration}}}
        selected = DomainPolicies(ADC_GROUP=Policy())
        result = selected.binding_values(SimpleNamespace(kind="gpio"), configuration=12)
        self.assertEqual(result["adc_group"]["layout"]["capacity"], 12)
        with self.assertRaises(TypeError):
            result["adc_group"]["layout"]["capacity"] = 3


if __name__ == "__main__":
    unittest.main()
