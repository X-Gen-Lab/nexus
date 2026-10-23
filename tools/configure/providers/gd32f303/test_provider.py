"""Exact GD303 geometry, silicon routes and static construction boundaries."""
import importlib
import json
from pathlib import Path
import unittest

from tools.configure.ir import ControllerIR
from tools.configure.providers.common import ConfigurationError


ROOT = Path(__file__).resolve().parents[4]


def uart(**changes):
    return {**{"id": "serial", "binding": "uart0", "kind": "uart",
        "controller": "USART0", "mode": "irq-bytes", "baud": 115200,
        "rx_profile": "bytes", "rx_capacity": 64, "irq_priority": 5,
        "calls_os": False, "pins": [
            {"pin": "PA9", "function": "tx", "af": 0},
            {"pin": "PA10", "function": "rx", "af": 0}]}, **changes}


def gpio(**changes):
    return {**{"id": "output", "binding": "gpio0", "kind": "gpio",
        "controller": "GPIOA", "mode": "output", "mask": 32, "initial": 0,
        "pins": [{"pin": "PA5", "function": "output", "af": 0}]}, **changes}


class Result(dict):
    def __init__(self, *items, backend="baremetal"):
        super().__init__(irq={"external_count": 60, "priority_bits": 4,
            "syscall_priority": 5 if backend == "freertos" else None},
            devices=[], board_bindings={}, routes={}, kernel_profile=(
                {"kernel_policy": "basepri"} if backend == "freertos" else None))
        self.controllers = tuple(ControllerIR.create(item, "gd32f303")
                                 for item in items)


class GD303ProviderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.provider = importlib.import_module(
            "tools.configure.providers.gd32f303")

    def test_cpu_and_clock_are_exact_immutable_contracts(self):
        self.assertEqual(self.provider.EXTERNAL_IRQ_COUNT, 60)
        self.assertEqual(self.provider.CPU_ABI["arch"], "cortex-m4")
        self.assertEqual(self.provider.CPU_ABI["fpu"], "fpv4-sp-d16")
        with self.assertRaises(TypeError):
            self.provider.CLOCK_PROFILES["irc8"]["core_hz"] = 120000000

    def test_capability_does_not_advertise_sdk_peripheral_potential(self):
        for kind, mode in (("gpio", "input"), ("gpio", "output"),
                           ("uart", "irq-bytes"), ("spi", "short-poll"),
                           ("watchdog", "independent"), ("flash", "poll-word"),
                           ("adc", "single-shot"), ("pwm", "fixed-pwm")):
            self.assertEqual(kind, self.provider.mode_implementation(kind, mode))
        self.assertEqual("uart_dma", self.provider.mode_implementation("uart", "dma-tx"))
        self.assertEqual("uart_dma", self.provider.mode_implementation("uart", "dma-duplex"))
        self.assertEqual("i2c_async", self.provider.mode_implementation("i2c", "irq-messages"))
        for kind, mode in (("uart", "rx-dma-continuous"), ("uart", "irq-blocks"),
                           ("spi", "dma-full-duplex"), ("adc", "trigger-dma")):
            with self.subTest(kind=kind, mode=mode):
                with self.assertRaises(ConfigurationError):
                    self.provider.mode_implementation(kind, mode)

    def test_uart_uses_default_legacy_route_not_f470_af7(self):
        self.provider.validate_selection(uart(), uart())
        for route in (uart(controller="USART1"), uart(pins=[
                {"pin": "PA9", "function": "tx", "af": 7},
                {"pin": "PA10", "function": "rx", "af": 7}])):
            with self.assertRaises(ConfigurationError):
                self.provider.validate_selection(uart(), route)

    def test_baud_checks_actual_eight_megahertz_divider(self):
        for baud in (123, 9600, 115200, 500000):
            self.provider.validate_selection(uart(baud=baud), uart())
        for baud in (True, 0, 122, 500001):
            with self.assertRaises(ConfigurationError):
                self.provider.validate_selection(uart(baud=baud), uart())

    def test_events_dma_and_foreign_irq_policy_are_not_silent_fallbacks(self):
        for changes in ({"rx_profile": "events"}, {"rx_profile": "blocks"},
                        {"irq_priority": 0}, {"irq_priority": True}):
            with self.assertRaises(ConfigurationError):
                self.provider.validate_selection(uart(**changes), uart())
        with self.assertRaises(ConfigurationError):
            self.provider.validate_dma_resource("dma:DMA0:channel3")

    def test_gpio_exact_authorization_and_no_boolean_integer(self):
        self.provider.validate_selection(gpio(), gpio())
        for changes in ({"mask": True}, {"mask": 64}, {"initial": 64},
                        {"controller": "GPIOB"}):
            with self.assertRaises(ConfigurationError):
                self.provider.validate_selection(gpio(**changes), gpio(**changes))

    def test_constructor_retained_guard_precedes_state_inputs(self):
        result = Result(uart())
        construction = self.provider.constructor(
            result.controllers[0], result, {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertLess(source.index(".retained"), source.index(".registers ="))
        self.assertIn("static uint8_t s_nx_rx_serial[64]", source)
        self.assertIn("USART0_IRQHandler", source)
        self.assertIn("nx_gd32f303_usart0_pins_prepare()", source)
        self.assertIn("8000000u", source)
        self.assertEqual(construction.stop,
                         ("nx_uart_port_stop(&s_nx_face_serial)",))
        self.assertTrue(construction.retain_on_failed_init)

    def test_foreign_provider_and_mode_cannot_bypass_constructor(self):
        with self.assertRaises(ConfigurationError):
            self.provider.constructor(ControllerIR.create(uart(), "stm32f407"),
                                      Result(), {}, {}, ())
        with self.assertRaises(ConfigurationError):
            self.provider.constructor(uart(mode="rx-dma-continuous"), Result(), {}, {}, ())

    def test_backend_policy_is_generated_not_a_hidden_scheduler(self):
        result = Result(uart(), backend="freertos")
        source = "\n".join(self.provider.constructor(
            result.controllers[0], result, {}, {}, ()).definitions)
        self.assertIn("NX_IRQ_KERNEL_BASEPRI, 60u, 4u, 5u", source)
        self.assertNotIn("xTaskCreate", source)

    def test_gpio_and_uart_share_one_platform_pin_lease(self):
        result = Result(gpio(), uart())
        declaration = "\n".join(self.provider.state_declarations(result))
        self.assertEqual(declaration.count("nx_gd32f303_pin_lease_t"), 1)
        start = "\n".join(self.provider.start_resources(result))
        stop = "\n".join(self.provider.stop_system(result))
        self.assertIn("s_nx_platform_pins, 1568u", start)
        self.assertLess(stop.index("nx_gd32f303_pins_release"),
                        stop.index("nx_gd32f303_system_stop"))
        self.assertNotIn("pins_acquire", "\n".join(
            self.provider.start_resources(Result())))

    def test_exact_geometry_and_package_inventory_are_not_family_wide(self):
        facts = json.loads((ROOT / "soc/gd32/gd32f303/soc.json").read_text())
        self.assertEqual(set(facts["variants"]), {"GD32F303RCT6"})
        part = facts["variants"]["GD32F303RCT6"]
        self.assertEqual(part["flash"]["erase_blocks"], [2048] * 128)
        self.assertEqual(part["memory"][0]["size"], 48 * 1024)
        self.assertEqual(len(part["pins"]), 51)
        self.assertTrue(part["memory"][0]["dma"])
        self.assertIn("timer:TIMER1", facts["reserved_resources"])

    def test_reference_fixture_never_invents_a_board_led_or_crystal(self):
        board = json.loads((ROOT / "boards/gd32f303_reference/board.json").read_text())
        self.assertEqual(board["clocks"], {})
        self.assertIsNone(board["physical_pcb_revision"])
        self.assertNotIn("led0", [item["id"] for item in board["bindings"]])
        self.assertTrue(any("No physical HIL" in item for item in board["unknowns"]))

    def test_spi_watchdog_assembly_resolves_exact_endpoints(self):
        from tools.configure import configure, bindings
        result = configure.resolve_ir(ROOT / "tools/configure/assemblies/"
                                      "gd32-f303-baremetal-spi-watchdog.toml")
        self.assertEqual({item.kind for item in result.controllers},
                         {"spi", "watchdog"})
        source = bindings.emit(result)[1]
        self.assertIn("s_nx_platform_pins, 240u", source)
        self.assertIn("nx_gd32f303_spi_endpoint_ops", source)
        self.assertIn("nx_gd32f303_watchdog_initialize", source)
        self.assertIn("s_nx_cs_peripheral.retained", source)
        self.assertLess(source.index("nx_gd32f303_pins_preflight(240u)"),
                        source.index("nx_gd32f303_system_start("))

    def test_spi_rejects_foreign_route_and_unrealizable_frequency(self):
        route = {"kind": "spi", "controller": "SPI0", "pins": [
            {"pin": "PA5", "function": "sck", "af": 0},
            {"pin": "PA6", "function": "miso", "af": 0},
            {"pin": "PA7", "function": "mosi", "af": 0}]}
        for rate in (31250, 1000000, 4000000):
            self.provider.validate_selection(
                {"mode": "short-poll", "max_hz": rate}, route)
        for rate in (True, 31249, 4000001):
            with self.assertRaises(ConfigurationError):
                self.provider.validate_selection(
                    {"mode": "short-poll", "max_hz": rate}, route)
        with self.assertRaises(ConfigurationError):
            self.provider.validate_selection(
                {"mode": "short-poll", "max_hz": 1000000},
                {**route, "controller": "SPI1"})

    def test_watchdog_has_no_invented_pin_or_startup_activation(self):
        route = {"kind": "watchdog", "controller": "FWDGT", "pins": []}
        self.provider.validate_selection({"mode": "independent"}, route)
        with self.assertRaises(ConfigurationError):
            self.provider.validate_selection({"mode": "independent"},
                                             {**route, "controller": "WWDGT"})
        from tools.configure import configure, bindings
        result = configure.resolve_ir(ROOT / "tools/configure/assemblies/"
                                      "gd32-f303-baremetal-spi-watchdog.toml")
        source = bindings.emit(result)[1]
        self.assertNotIn("nx_watchdog_port_enable", source)
        self.assertNotIn("FWDGT_KEY_ENABLE", source)



if __name__ == "__main__":
    unittest.main()
