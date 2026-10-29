"""Exact channel mapping, finite acquisition and physical Flash selection."""
from types import SimpleNamespace, MappingProxyType
from pathlib import Path
import unittest

from tools.configure import bindings, configure, factory
from tools.configure import selection as selection_rules
from tools.configure.providers import common, maintained


class AdcMappingTests(unittest.TestCase):
    def test_adc_channel_mapping_is_per_controller_and_exact_pin(self):
        provider = SimpleNamespace(
            ADC_PIN_CHANNELS=MappingProxyType({("ADC1", "PA0"): 1}),
            validate_selection=lambda selection, route: None)
        route = {"kind": "adc", "controller": "ADC1",
                 "pins": [{"pin": "PA0", "function": "analog", "af": 0}]}
        selection = {"id": "sensor", "binding": "sensor", "mode": "single-shot",
                     "channels": [1], "sample_times": [0], "reference_mv": 3300}
        selection_rules.validate_selection(selection, route, provider, 98304)
        route["controller"] = "ADC2"
        with self.assertRaises(common.ConfigurationError):
            selection_rules.validate_selection(selection, route, provider, 98304)

    def test_real_factory_has_exact_adc_flash_and_i2c_construction(self):
        root = Path(__file__).resolve().parents[5]
        result = configure.resolve_ir(
            root / "tests/contracts/g474_assembly/acquisition.toml")
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32g474_adc_ops", source)
        self.assertIn("nx_i2c_v2_ops", source)
        self.assertIn("nx_i2c_v2_endpoint_ops", source)
        self.assertIn("nx_stm32g474_flash_ops", source)
        self.assertIn(".common = ADC12_COMMON", source)
        capabilities = "\n".join(factory.emit_capabilities(result)[1])
        self.assertIn("NX_FACTORY_CAP_POLLING, 0U, 8U, 1U, 0U", capabilities)
        self.assertIn(".owned_i2c_mux = &s_nx_platform_i2c_mux_owned", source)
        self.assertIn(".owned_adc_mux = &s_nx_platform_adc_mux_owned", source)
        self.assertIn(".flash = true", source)
        self.assertIn(".added_syscfg = &s_nx_platform_syscfg_clock_added", source)
        self.assertIn("irq:FLASH", {claim.key for claim in result.resource_plan.claims})
        self.assertIn("NX_G474_PLATFORM_IRQ_MASK(FLASH_IRQn, 0u)", source)
        self.assertIn("nx_stm32g474_platform_preflight(", source)
        self.assertNotIn("DMA1_Channel1_IRQHandler", source)
        self.assertEqual([item.kind for item in result.controllers],
                         ["adc", "flash", "i2c"])

    def test_finite_dma_factory_has_one_normal_lane_and_no_ccm_domain(self):
        root = Path(__file__).resolve().parents[5]
        result = configure.resolve_ir(
            root / "tests/contracts/g474_assembly/acquisition-dma.toml")
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32g474_adc_stream_ops", source)
        self.assertIn(".channel = DMA1_Channel1", source)
        self.assertIn(".mux = DMAMUX1_Channel0", source)
        self.assertIn("98304u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE", source)
        self.assertNotIn("(uintptr_t)0x10000000u", source)
        self.assertIn("DMA1_Channel1_IRQHandler", source)
        claims = {claim.key for claim in result.resource_plan.claims}
        self.assertIn("controller:TIM6", claims)
        self.assertIn("dma:DMA1:stream1", claims)

    def test_adc_lifecycle_is_c_owned_for_both_current_modes(self):
        root = Path(__file__).resolve().parents[5]
        for filename, implementation in (("acquisition.toml", "adc"),
                                         ("acquisition-dma.toml", "adc_stream")):
            with self.subTest(filename=filename):
                result = configure.resolve_ir(
                    root / "tests/contracts/g474_assembly" / filename)
                item = next(item for item in result.controllers
                            if item.kind == "adc")
                construction = maintained(result.soc_family).constructor(
                    item, result, result.board_bindings, result.routes,
                    result.devices)
                self.assertEqual(construction.initialize.function,
                                 f"nx_stm32g474_{implementation}_prepare")
                source = "\n".join(construction.definitions)
                self.assertIn(f"static const nx_stm32g474_{implementation}_config_t", source)
                self.assertNotIn("NX_G474_WRITE(&GPIOA->PUPDR", source)
                self.assertNotIn("NX_G474_WRITE(&GPIOA->MODER", source)
                self.assertNotIn(".reference_mv = 3300u;", source)

    def test_control_construction_contains_no_runtime_hardware_algorithm(self):
        root = Path(__file__).resolve().parents[5]
        for relative in ("tests/contracts/g474_control_assembly/pwm-group.toml",
                         "cmake/package/verification/io_evolution/g474-pwm-adc.toml"):
            with self.subTest(relative=relative):
                result = configure.resolve_ir(root / relative)
                item = result.controllers[0]
                construction = maintained(result.soc_family).constructor(
                    item, result, result.board_bindings, result.routes,
                    result.devices)
                source = "\n".join(construction.definitions)
                self.assertNotIn("NX_G474_WRITE", source)
                self.assertNotIn("NX_G474_READ", source)
                self.assertNotIn("while (", source)
                self.assertNotIn("static nx_result_t prepare_", source)
                self.assertNotIn("static nx_result_t stop_", source)

    def test_channel_and_sample_profile_cannot_fall_back(self):
        import importlib
        provider = importlib.import_module("soc.stm32.stm32g474.configure")
        route = {"kind": "adc", "controller": "ADC1", "pins": [
            {"pin": "PA0", "function": "analog", "af": 0}]}
        selection = {"mode": "single-shot", "channels": [1],
                     "sample_times": [0], "reference_mv": 3300}
        provider.validate_selection(selection, route)
        for change in ({"channels": [0]}, {"sample_times": [7]},
                       {"reference_mv": 1600}, {"mode": "trigger-dma",
                                                "irq_priority": 4}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                provider.validate_selection({**selection, **change}, route)


if __name__ == "__main__":
    unittest.main()
