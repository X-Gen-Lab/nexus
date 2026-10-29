"""Exact U575 GPIO, USART2 and inactive IWDG construction contracts."""
import unittest

from soc.stm32.stm32u575 import configure as stm32u575
from tools.configure.providers.common import ConfigurationError
from tools.configure import preflight
from .test_provider import (
    Result, gpio, uart)


class U575MainstreamTests(unittest.TestCase):
    def test_usart2_exact_route_and_mapped_irq(self):
        item = uart(controller="USART2", pins=[
            {"pin": "PA2", "function": "tx", "af": 7},
            {"pin": "PA3", "function": "rx", "af": 7}])
        stm32u575.validate_selection(item, item)
        result = Result(item)
        construction = stm32u575.constructor(
            result.controllers[0], result, {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertIn(".controller = 2u", source)
        self.assertIn("nx_stm32u575_uart_prepare(&s_nx_port_console, "
                      "&s_nx_config_console)", construction.initialize.render())
        self.assertIn("USART2_IRQHandler", source)
        resource = "\n".join(stm32u575.state_declarations(result))
        self.assertIn(".selected = NX_U575_SELECT_USART2", resource)
        self.assertIn(".apb1_added = &s_nx_peripheral_apb1enr1_added", resource)
        self.assertIn("nx_stm32u575_platform_acquire(&s_nx_platform_cold)",
                      "\n".join(stm32u575.start_resources(result)))

    def test_gpio_ports_follow_exact_pin_inventory_and_mask(self):
        for port, pin in (("B", 0), ("D", 0), ("E", 0), ("F", 0),
                          ("G", 0), ("H", 3)):
            with self.subTest(port=port):
                item = gpio(controller="GPIO" + port, mask=1 << pin,
                    pins=[{"pin": f"P{port}{pin}", "function": "output",
                           "af": 0}])
                stm32u575.validate_selection(item, item)
                result = Result(item)
                construction = stm32u575.constructor(
                    result.controllers[0], result, {}, {}, ())
                self.assertIn(f"GPIO{port}, {1 << pin}u, 0u,",
                              "\n".join(construction.definitions))
                self.assertEqual(
                    "nx_stm32u575_gpio_prepare(&s_nx_port_led, &s_nx_config_led)",
                    construction.initialize.render())
        for port, pin in (("B", 12), ("C", 4), ("C", 5), ("G", 11),
                          ("H", 2), ("I", 0)):
            item = gpio(controller="GPIO" + port, mask=1 << pin,
                pins=[{"pin": f"P{port}{pin}", "function": "output",
                       "af": 0}])
            with self.subTest(port=port, pin=pin):
                with self.assertRaises(ConfigurationError):
                    stm32u575.validate_selection(item, item)

    def test_watchdog_assembly_never_enables_hardware(self):
        item = {"id": "watchdog", "binding": "iwdg", "kind": "watchdog",
                "controller": "IWDG", "mode": "independent", "pins": []}
        stm32u575.validate_selection(item, item)
        result = Result(item)
        construction = stm32u575.constructor(
            result.controllers[0], result, {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertEqual("nx_stm32u575_watchdog_prepare(&s_nx_port_watchdog)",
                         construction.initialize.render())
        self.assertNotIn("port_enable", source)
        self.assertNotIn("0xCCCC", source)
        self.assertEqual(
            ("nx_stm32u575_watchdog_stop(&s_nx_port_watchdog)",),
            tuple(call.render() for call in construction.stop))
        self.assertEqual(stm32u575.operations(result.controllers[0]),
                         "nx_stm32u575_watchdog_ops")

    def test_vddio2_gpio_has_read_only_supply_admission_before_effects(self):
        item = gpio(controller="GPIOG", mask=4, pins=[
            {"pin": "PG2", "function": "output", "af": 0}])
        result = Result(item)
        result.soc_family = "stm32u575"
        source = "\n".join(preflight.emit(result))
        self.assertIn("nx_stm32u575_platform_preflight(&s_nx_platform_cold)",
                      source)
        self.assertNotIn("WRITE", source)
        self.assertNotIn("platform_acquire", source)
        selected = "\n".join(stm32u575.state_declarations(result))
        self.assertIn("GPIOG, &s_nx_platform_receipts[0u], 4u, 6u", selected)
        system = "\n".join(stm32u575.start_system(result))
        self.assertLess(system.index("nx_stm32u575_platform_preflight("),
                        system.index("nx_stm32u575_system_start("))


if __name__ == "__main__":
    unittest.main()
