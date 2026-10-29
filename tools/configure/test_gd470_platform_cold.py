"""GD470 global cold ownership is static data connected to real C behavior."""
import unittest

from tools.configure import bindings, configure, gpio_phase
from tools.configure.providers import maintained


class Gd470PlatformColdConfiguration(unittest.TestCase):
    def resolve(self, name):
        return configure.resolve_ir(configure.ROOT /
            "tests/contracts/multi_controller_assembly" / name)

    def test_selected_admission_is_a_concrete_c_call(self):
        result = self.resolve("gd32.toml")
        source = bindings.emit(result)[1]
        self.assertIn("nx_gd32_platform_selected_preflight("
                      "&s_nx_platform_config)", source)
        policy = maintained(result.soc_family)
        selected = "\n".join(policy.selected_preflight(result))
        self.assertNotIn("NVIC->", selected)
        self.assertNotIn("DMA_CHCTL", selected)
        self.assertNotIn("RCU_APB", selected)

    def test_pin_snapshot_slots_and_resource_masks_are_immutable_wiring(self):
        result = self.resolve("gd32.toml")
        declarations = "\n".join(gpio_phase.declarations(result))
        self.assertIn("static const nx_gd32_platform_pin_config_t", declarations)
        self.assertIn("static const nx_gd32_platform_config_t", declarations)
        self.assertIn("&s_nx_platform_pin_state[", declarations)
        self.assertIn("static nx_gd32_platform_pin_receipt_t", declarations)
        self.assertNotIn("static nx_result_t nx_platform_pins_", declarations)
        self.assertNotIn("NX_PLATFORM_REGISTER_WRITE", declarations)

    def test_selected_dma_clocks_use_retained_c_acquire_and_release(self):
        result = self.resolve("gd32.toml")
        source = bindings.emit(result)[1]
        self.assertIn("nx_gd32_platform_dma_acquire(", source)
        self.assertIn("nx_gd32_platform_dma_release(", source)
        self.assertNotIn("s_nx_platform_dma_clocks_added = ~RCU_AHB1EN", source)
        self.assertNotIn("RCU_AHB1EN |= (RCU_AHB1EN_DMA", source)

    def test_uart_dma_construction_connects_one_c_lease_and_readonly_config(self):
        result = self.resolve("gd32.toml")
        provider = maintained(result.soc_family)
        for item in result.controllers:
            if item.kind != "uart":
                continue
            with self.subTest(controller=item.controller):
                construction = provider.constructor(
                    item, result, result.board_bindings, result.routes,
                    result.devices)
                self.assertEqual(construction.initialize.function,
                                 "nx_gd32_uart_dma_cold_prepare")
                self.assertEqual(tuple(call.function
                                       for call in construction.stop),
                                 ("nx_gd32_uart_dma_cold_close",))
                self.assertTrue(construction.retain_on_failed_init)
                declarations = "\n".join(construction.definitions)
                self.assertIn("static const nx_gd32_uart_dma_cold_config_t",
                              declarations)
                self.assertNotIn("NVIC->", declarations)
                self.assertNotIn("NX_PLATFORM_REGISTER_WRITE", declarations)

    def test_control_construction_uses_c_with_static_selected_facts(self):
        for fixture in ("pwm.toml", "adc-scan.toml", "adc-continuous.toml"):
            result = configure.resolve_ir(configure.ROOT /
                "tests/contracts/gd32_assembly" / fixture)
            provider = maintained(result.soc_family)
            for item in result.controllers:
                construction = provider.constructor(item, result,
                    result.board_bindings, result.routes, result.devices)
                self.assertTrue(construction.initialize.function.startswith(
                    "nx_gd32_"), construction.initialize.function)
                self.assertNotIn("static nx_result_t prepare_",
                                 "\n".join(construction.definitions))


if __name__ == "__main__":
    unittest.main()
