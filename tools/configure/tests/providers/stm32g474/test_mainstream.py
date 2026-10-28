"""Mainstream G474 package, IRQ byte and multi-bank cold assembly contracts."""
import importlib
import json
import tempfile
from pathlib import Path
import unittest

from tools.configure import bindings, configure
from tools.configure.providers.common import ConfigurationError

from .test_provider import Result, fixture_controller, gpio, uart


class G474MainstreamTests(unittest.TestCase):
    def setUp(self):
        self.provider = importlib.import_module(
            "soc.stm32.stm32g474.configure")

    def test_general_uart_controllers_accept_exact_source_routes(self):
        for controller, tx, rx, af in (
                ("USART1", "PA9", "PA10", 7),
                ("USART2", "PA2", "PA3", 7),
                ("USART3", "PB10", "PB11", 7),
                ("UART4", "PC10", "PC11", 5),
                ("UART5", "PC12", "PD2", 5)):
            values = uart(controller=controller, pins=[
                {"pin": tx, "function": "tx", "af": af},
                {"pin": rx, "function": "rx", "af": af}])
            with self.subTest(controller=controller):
                self.provider.validate_selection(values, values)
                item = fixture_controller(values)
                code = "\n".join(self.provider.constructor(
                    item, Result(values), {}, {}, ()).definitions)
                self.assertIn(f".registers = {controller};", code)
                self.assertIn(f".irq = {controller}_IRQn;", code)
                self.assertIn(f"void {controller}_IRQHandler", code)

    def test_gpio_mask_must_equal_route_across_package_banks(self):
        for bank, pin in (("A", 0), ("B", 7), ("C", 3), ("D", 2),
                          ("F", 0), ("G", 10)):
            values = gpio(controller="GPIO" + bank, mask=1 << pin, pins=[
                {"pin": f"P{bank}{pin}", "function": "output", "af": 0}])
            with self.subTest(bank=bank):
                self.provider.validate_selection(values, values)
                item = fixture_controller(values)
                code = "\n".join(self.provider.constructor(
                    item, Result(values), {}, {}, ()).definitions)
                self.assertIn(f".registers = GPIO{bank};", code)

    def test_multibank_factory_has_per_controller_gates_and_pin_snapshots(self):
        root = Path(__file__).resolve().parents[5]
        result = configure.resolve_ir(
            root / "tests/contracts/g474_assembly/mainstream.toml")
        code = bindings.emit(result)[1]
        self.assertEqual(sum(item.kind == "uart" for item in result.controllers), 5)
        self.assertIn("nx_platform_pin_capture(GPIOB, 0u)", code)
        self.assertIn("nx_platform_pin_restore(GPIOC, 0u,", code)
        for register, name in (("APB2ENR", "USART1"),
                               ("APB1ENR1", "USART2"),
                               ("APB1ENR1", "UART5")):
            self.assertIn(f"RCC_{register}_{name}EN", code)
        self.assertNotIn("LPUART1EN", code)

    def test_spi_has_real_endpoints_and_g4_fifo_driver(self):
        root = Path(__file__).resolve().parents[5]
        result = configure.resolve_ir(root / "tests/contracts/g474_assembly/spi.toml")
        code = bindings.emit(result)[1]
        self.assertEqual(len(result.devices), 2)
        self.assertIn("nx_stm32g474_spi_endpoint_ops", code)
        self.assertIn("RCC_APB2ENR_SPI1EN", code)
        self.assertIn("RCC_APB1ENR1_SPI2EN", code)
        self.assertIn("RCC_APB1RSTR1_SPI2RST", code)
        self.assertIn("nx_stm32g474_pin_alternate(GPIOB, 15u, 5u)", code)
        self.assertNotIn("nx_stm32_spi", code)

    def test_spi_rate_exceeding_rated_g4_clock_is_rejected(self):
        values = {"kind": "spi", "mode": "short-poll", "max_hz": 85000000}
        route = {"kind": "spi", "controller": "SPI1", "pins": [
            {"pin": "PA5", "function": "sck", "af": 5},
            {"pin": "PA6", "function": "miso", "af": 5},
            {"pin": "PA7", "function": "mosi", "af": 5}]}
        with self.assertRaises(ConfigurationError):
            self.provider.validate_selection(values, route)

    def test_watchdog_assembly_does_not_enable_or_feed_during_start(self):
        root = Path(__file__).resolve().parents[5]
        result = configure.resolve_ir(root / "tests/contracts/g474_assembly/watchdog.toml")
        code = bindings.emit(result)[1]
        self.assertIn("nx_stm32g474_watchdog_ops", code)
        self.assertIn(".registers = IWDG;", code)
        self.assertNotIn("nx_watchdog_port_enable(", code)
        self.assertNotIn("nx_watchdog_port_feed(", code)

    def test_fixed_hse_and_conditional_nrst_pads_cannot_be_claimed_as_gpio(self):
        root = Path(__file__).resolve().parents[5]
        original = json.loads((root / "boards/stm32g474re_hse24_reference/board.json").read_text())
        for pin in ("PF0", "PF1", "PG10"):
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                board = json.loads(json.dumps(original))
                board["bindings"].append({
                    "id": "forbidden", "controller": "GPIO" + pin[1],
                    "route": "gpio-" + pin.lower(), "mask": 1 << int(pin[2:]),
                    "initial": 0})
                (directory / "board.json").write_text(json.dumps(board))
                assembly = (root / "tests/contracts/g474_assembly/empty.toml").read_text().replace(
                    '"../../../boards/stm32g474re_hse24_reference"',
                    '"' + directory.as_posix() + '"')
                assembly += '\n[gpio.forbidden]\nbinding = "forbidden"\nmode = "output"\n'
                path = directory / "assembly.toml"
                path.write_text(assembly)
                with self.assertRaisesRegex(ConfigurationError, "Resource conflict"):
                    configure.resolve_ir(path)

    def test_all_new_profiles_select_the_real_m4f_backend_without_workers(self):
        root = Path(__file__).resolve().parents[5]
        for name in ("mainstream", "spi", "watchdog", "pwm"):
            with self.subTest(profile=name):
                result = configure.resolve_ir(root / "tests/contracts/g474_assembly" /
                                              (name + "-freertos.toml"))
                self.assertEqual(result.kernel.kernel_port, "GCC/ARM_CM4F")
                code = bindings.emit(result)[1]
                self.assertNotIn("xTaskCreate", code)
                self.assertNotIn("vTaskStartScheduler", code)

    def test_fixed_pwm_has_exact_independent_ch1_routes_and_static_storage(self):
        for controller, wire in (("TIM3", "PA6"), ("TIM4", "PB6")):
            values = {"id": "wave", "binding": "pwm", "kind": "pwm",
                      "controller": controller, "mode": "fixed-pwm",
                      "period_ticks": 1000, "duty_ticks": 250,
                      "tick_hz": 1000000, "initial": 0, "pins": [
                          {"pin": wire, "function": "ch1", "af": 2}]}
            with self.subTest(controller=controller):
                self.assertEqual("pwm", self.provider.mode_implementation(
                    "pwm", "fixed-pwm"))
                self.provider.validate_selection(values, values)
                item = fixture_controller(values)
                construction = self.provider.constructor(
                    item, Result(values), {"pwm": {"mask": 64, "initial": 0}}, {}, ())
                code = "\n".join(construction.definitions)
                self.assertIn(f".registers = {controller};", code)
                self.assertIn(f"RCC_APB1RSTR1_{controller}RST", code)
                self.assertIn("nx_stm32g474_pwm_initialize", code)
                self.assertIn("nx_stm32g474_pwm_close", "\n".join(call.render() for call in construction.stop))

    def test_fixed_pwm_real_factory_has_both_gates_and_rejects_invalid_base(self):
        root = Path(__file__).resolve().parents[5]
        path = root / "tests/contracts/g474_assembly/pwm.toml"
        result = configure.resolve_ir(path)
        code = bindings.emit(result)[1]
        self.assertEqual(["TIM3", "TIM4"], [item.controller for item in result.controllers])
        self.assertIn("RCC_APB1ENR1_TIM3EN | RCC_APB1ENR1_TIM4EN", code)
        self.assertIn("nx_stm32g474_pwm_close(&s_nx_port_wave4)", code)
        for old, new in (("tick_hz = 1000000", "tick_hz = 999999"),
                         ("period_ticks = 1000", "period_ticks = 65536"),
                         ("duty_ticks = 250", "duty_ticks = true")):
            with self.subTest(new=new), tempfile.TemporaryDirectory(dir=path.parent) as temp:
                candidate = Path(temp) / "invalid.toml"
                candidate.write_text(path.read_text().replace(old, new).replace(
                    "../../../boards/", "../../../../boards/"))
                with self.assertRaises(ConfigurationError):
                    configure.resolve_ir(candidate)


if __name__ == "__main__":
    unittest.main()
