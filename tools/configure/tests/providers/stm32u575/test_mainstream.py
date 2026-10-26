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
        self.assertIn(".registers = USART2", source)
        self.assertIn(".irq = USART2_IRQn", source)
        self.assertIn("USART2_IRQHandler", source)
        resource = "\n".join(stm32u575.start_resources(result))
        self.assertIn("APB1ENR1", resource)
        self.assertIn("RCC_APB1ENR1_USART2EN", resource)

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
                self.assertIn(f".registers = GPIO{port}",
                              "\n".join(construction.definitions))
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
        self.assertIn(".registers = IWDG", source)
        self.assertNotIn("port_enable", source)
        self.assertNotIn("0xCCCC", source)
        self.assertIn("nx_stm32u575_watchdog_stop", source)
        self.assertEqual(stm32u575.operations(result.controllers[0]),
                         "nx_stm32u575_watchdog_ops")

    def test_vddio2_gpio_has_read_only_supply_admission_before_effects(self):
        item = gpio(controller="GPIOG", mask=4, pins=[
            {"pin": "PG2", "function": "output", "af": 0}])
        result = Result(item)
        result.soc_family = "stm32u575"
        source = "\n".join(preflight.emit(result))
        self.assertIn("PWR_SVMCR_IO2SV", source)
        self.assertIn("RCC_AHB3ENR_PWREN", source)
        self.assertNotIn("WRITE", source)


if __name__ == "__main__":
    unittest.main()
