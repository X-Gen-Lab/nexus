"""Optional F4 pad and clock owners retain only selected hardware algorithms."""
import unittest

from tools.configure import bindings, configure, gpio_phase


class F407PlatformPinOwnerTests(unittest.TestCase):
    def test_pinless_assembly_omits_pin_owner_callback_relocations(self):
        result = configure.resolve_ir(configure.ROOT / (
            "tools/configure/assemblies/sky-baremetal-empty.toml"))
        _, source = bindings.emit(result)
        self.assertIn(".pin_owner = NULL", source,
                      "Pinless config must omit the pin-owner connection")
        self.assertNotIn("nx_stm32_platform_pins_acquire", source,
                         "Unselected pins must not retain GPIO algorithms")

    def test_internal_adc_admits_resources_with_no_gpio_record(self):
        for density, backend in (("ve", "baremetal"), ("zg", "freertos")):
            with self.subTest(density=density):
                result = configure.resolve_ir(configure.ROOT / (
                    "tests/contracts/multi_controller_assembly/"
                    f"stm32-{density}-adc-internal-{backend}.toml"))
                self.assertEqual(gpio_phase.selected_pins(result), ())
                self.assertTrue(gpio_phase.applies(result))
                _, source = bindings.emit(result)
                self.assertIn(".pins = NULL, .pin_count = 0u", source)
                self.assertIn(".pin_owner = &s_nx_platform_pin_owner", source)
                self.assertIn("nx_stm32_platform_selected_preflight", source)
                self.assertNotIn("s_nx_platform_pins_config[]", source)
                self.assertNotIn("s_nx_platform_pin_state[", source)
                for converter in ("ADC1", "ADC2", "ADC3"):
                    self.assertIn(f"RCC_APB2ENR_{converter}EN", source)

    def test_selected_pins_use_one_readonly_fixed_owner_table(self):
        result = configure.resolve_ir(configure.ROOT / (
            "tools/configure/assemblies/sky-baremetal.toml"))
        _, source = bindings.emit(result)
        self.assertIn("static const nx_stm32f4_platform_pin_owner_t", source,
                      "Selected pads require fixed typed owner connections")
        self.assertIn(".pin_owner = &s_nx_platform_pin_owner", source)
        for name in ("selected_preflight", "pins_acquire", "pins_abort",
                     "gpio_clocks_release", "pins_release"):
            self.assertIn("nx_stm32_platform_" + name, source)

    def test_irq_uart_omits_unselected_shared_clock_connections(self):
        result = configure.resolve_ir(configure.ROOT / (
            "tools/configure/assemblies/sky-baremetal.toml"))
        _, source = bindings.emit(result)
        self.assertIn(".clock_owner = NULL", source)
        self.assertNotIn("nx_stm32_platform_resources_acquire", source)
        self.assertNotIn("nx_stm32_platform_resources_release", source)

    def test_dma_selection_retains_one_fixed_shared_clock_owner(self):
        result = configure.resolve_ir(configure.ROOT / (
            "tools/configure/assemblies/qiming-baremetal-uart-dma.toml"))
        _, source = bindings.emit(result)
        self.assertIn("static const nx_stm32f4_platform_clock_owner_t", source)
        self.assertIn(".clock_owner = &s_nx_platform_clock_owner", source)
        self.assertIn(".acquire = nx_stm32_platform_resources_acquire", source)
        self.assertIn(".release = nx_stm32_platform_resources_release", source)


if __name__ == "__main__":
    unittest.main()
