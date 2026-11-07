"""Optional F4 pad and clock owners retain only selected hardware algorithms."""
import unittest

from tools.configure import bindings, configure


class F407PlatformPinOwnerTests(unittest.TestCase):
    def test_pinless_assembly_omits_pin_owner_callback_relocations(self):
        result = configure.resolve_ir(configure.ROOT / (
            "tools/configure/assemblies/sky-baremetal-empty.toml"))
        _, source = bindings.emit(result)
        self.assertTrue(".pin_owner = NULL" in source,
                        "Pinless config must omit the pin-owner connection")
        self.assertFalse("nx_stm32_platform_pins_acquire" in source,
                         "Unselected pins must not retain GPIO algorithms")

    def test_selected_pins_use_one_readonly_fixed_owner_table(self):
        result = configure.resolve_ir(configure.ROOT / (
            "tools/configure/assemblies/sky-baremetal.toml"))
        _, source = bindings.emit(result)
        self.assertTrue("static const nx_stm32f4_platform_pin_owner_t" in source,
                        "Selected pads require fixed typed owner connections")
        self.assertTrue(".pin_owner = &s_nx_platform_pin_owner" in source)
        for name in ("selected_preflight", "pins_acquire", "pins_abort",
                     "gpio_clocks_release", "pins_release"):
            self.assertTrue("nx_stm32_platform_" + name in source)

    def test_irq_uart_omits_unselected_shared_clock_connections(self):
        result = configure.resolve_ir(configure.ROOT / (
            "tools/configure/assemblies/sky-baremetal.toml"))
        _, source = bindings.emit(result)
        self.assertTrue(".clock_owner = NULL" in source)
        self.assertFalse("nx_stm32_platform_resources_acquire" in source)
        self.assertFalse("nx_stm32_platform_resources_release" in source)

    def test_dma_selection_retains_one_fixed_shared_clock_owner(self):
        result = configure.resolve_ir(configure.ROOT / (
            "tools/configure/assemblies/qiming-baremetal-uart-dma.toml"))
        _, source = bindings.emit(result)
        self.assertTrue("static const nx_stm32f4_platform_clock_owner_t" in source)
        self.assertTrue(".clock_owner = &s_nx_platform_clock_owner" in source)
        self.assertTrue(".acquire = nx_stm32_platform_resources_acquire" in source)
        self.assertTrue(".release = nx_stm32_platform_resources_release" in source)


if __name__ == "__main__":
    unittest.main()
