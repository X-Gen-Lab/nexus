"""Fixed G474 capability, route and cold emitter boundary contracts."""
import importlib
import json
from pathlib import Path
import unittest

from tools.configure import bindings, configure
from tools.configure.ir import ControllerIR
from tools.configure.providers.common import (ConfigurationError,
                                              validate_clock_profiles)


ROOT = Path(__file__).resolve().parents[4]


def uart(**changes):
    values = {"id": "console", "binding": "uart1", "kind": "uart",
              "controller": "LPUART1", "mode": "irq-bytes", "baud": 115200,
              "rx_profile": "bytes", "rx_capacity": 64, "irq_priority": 5,
              "calls_os": False, "pins": [
                  {"pin": "PA2", "function": "tx", "af": 12},
                  {"pin": "PA3", "function": "rx", "af": 12}]}
    return {**values, **changes}


def gpio(**changes):
    return {**{"id": "led", "binding": "led0", "kind": "gpio",
              "controller": "GPIOA", "mode": "output", "mask": 32,
              "initial": 0, "pins": [
                  {"pin": "PA5", "function": "output", "af": 0}]},
            **changes}


class Result(dict):
    def __init__(self, *items, backend="baremetal"):
        super().__init__(irq={"external_count": 102, "priority_bits": 4,
                              "syscall_priority": 5 if backend == "freertos"
                              else None}, devices=[], board_bindings={},
                         routes={}, kernel_profile=(
                             {"kernel_policy": "basepri"}
                             if backend == "freertos" else None))
        self.controllers = tuple(ControllerIR.create(item, "stm32g474")
                                 for item in items)


class G474ProviderTests(unittest.TestCase):
    def setUp(self):
        self.provider = importlib.import_module("tools.configure.providers.stm32g474")

    def test_cpu_contract_matches_official_irq_extent(self):
        self.assertEqual(self.provider.PREFIX, "stm32g474")
        self.assertEqual(self.provider.CPU_ABI,
                         {"arch": "cortex-m4", "fpu": "fpv4-sp-d16",
                          "float_abi": "hard"})
        self.assertEqual(self.provider.EXTERNAL_IRQ_COUNT, 102)
        self.assertEqual(self.provider.IRQ_PRIORITY_BITS, 4)
        self.assertEqual(self.provider.FREERTOS_PORT, "GCC/ARM_CM4F")
        self.assertTrue(self.provider.CPU_FEATURES["dsp"])

    def test_cpu_and_clock_capabilities_are_immutable(self):
        with self.assertRaises(TypeError):
            self.provider.CPU_ABI["arch"] = "cortex-m0"
        with self.assertRaises(TypeError):
            self.provider.CLOCK_PROFILES["hse24-pll170"]["core_hz"] = 168000000

    def test_uart_gpio_and_exact_exti_are_the_maintained_mode_set(self):
        for kind, mode in (("gpio", "input"), ("gpio", "output"),
                           ("uart", "irq-bytes"), ("exti", "edge-event")):
            self.assertEqual(self.provider.mode_implementation(kind, mode), kind)
        for kind, mode in (("uart", "irq-byte-event"), ("uart", "irq-blocks"),
                           ("uart", "dma-tx"), ("spi", "short-poll"),
                           ("flash", "poll-word"), ("can", "classic-irq")):
            with self.subTest(kind=kind, mode=mode):
                with self.assertRaises(ConfigurationError):
                    self.provider.mode_implementation(kind, mode)

    def test_baud_is_checked_against_actual_presc16_divisor(self):
        for value in (2594, 9600, 115200, 1000000):
            self.provider.validate_selection(uart(baud=value), uart())
        for value in (True, 0, 2593, 1000001):
            with self.subTest(baud=value):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection(uart(baud=value), uart())

    def test_nonbyte_profile_and_unimplemented_priority_are_rejected(self):
        for changes in ({"rx_profile": "events"}, {"rx_profile": "blocks"},
                        {"irq_priority": 4}, {"irq_priority": True}):
            with self.subTest(changes=changes):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection(uart(**changes), uart())

    def test_only_actual_lpuart1_and_fixed_af12_route_are_accepted(self):
        for route in (uart(controller="USART2"), uart(pins=[
                {"pin": "PA2", "function": "tx", "af": 7},
                {"pin": "PA3", "function": "rx", "af": 7}])):
            with self.assertRaises(ConfigurationError):
                self.provider.validate_selection(uart(), route)

    def test_only_gpioa_and_exact_authorized_mask_are_accepted(self):
        self.provider.validate_selection(gpio(), gpio())
        for changes in ({"controller": "GPIOB"}, {"mask": 64},
                        {"initial": 64}, {"mask": True}):
            with self.subTest(changes=changes):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection(gpio(**changes),
                                                     gpio(**changes))

    def test_dma_resource_and_dma_routes_are_unimplemented(self):
        for call in (lambda: self.provider.validate_dma_resource(
                "dma:DMA1:stream0:channel0"),
                     lambda: self.provider.dma_irq("DMA1:stream0:channel0"),
                     lambda: self.provider.validate_dma(
                         uart(), "irq-bytes", lambda item: item)):
            with self.assertRaises(ConfigurationError):
                call()
        self.assertEqual(self.provider.DMA_TOPOLOGY[0], frozenset())
        self.assertFalse(self.provider.shares_irq("uart", "irq:LPUART1"))

    def test_clock_metadata_cannot_drift_from_fixed_implementation(self):
        facts = json.loads((ROOT / "soc/stm32g474/soc.json").read_text())
        validate_clock_profiles(facts, self.provider)
        facts["clock_profiles"]["hse24-pll170"]["apb1_hz"] = 85000000
        with self.assertRaises(ConfigurationError):
            validate_clock_profiles(facts, self.provider)

    def test_memory_alias_is_counted_once_without_dma_advertisement(self):
        facts = json.loads((ROOT / "soc/stm32g474/soc.json").read_text())
        variant = facts["variants"]["STM32G474RET6"]
        self.assertEqual(sum(item["size"] for item in variant["memory"]),
                         128 * 1024)
        self.assertEqual(variant["memory"], [
            {"id": "cpu-sram", "origin": 0x20000000, "size": 131072,
             "dma": False, "linker": True}])
        self.assertEqual(variant["flash"]["erase_blocks"], [2048] * 256)
        self.assertEqual(facts["reserved_resources"],
                         ["timer:TIM2", "irq:TIM2_IRQn"])

    def test_reference_board_is_explicit_about_physical_uncertainty(self):
        board = json.loads((ROOT / "boards/stm32g474re_hse24_reference/"
                            "board.json").read_text())
        self.assertEqual(board["id"], "stm32g474re-hse24-reference")
        self.assertEqual(board["clocks"]["hse_hz"], 24000000)
        self.assertIsNone(board["physical_pcb_revision"])
        self.assertTrue(any("not inspected" in text for text in board["unknowns"]))
        self.assertTrue(any("HIL" in text for text in board["unknowns"]))
        self.assertEqual(board["bindings"][0]["initial"], 0)

    def test_uart_ctor_is_guarded_before_overwriting_retained_state(self):
        item = ControllerIR.create(uart(), "stm32g474")
        construction = self.provider.constructor(item, Result(uart()), {}, {}, ())
        source = "\n".join(construction["definitions"])
        self.assertLess(source.index(".retained"), source.index(".registers ="))
        self.assertIn("NX_UART_RX_BYTES", source)
        self.assertIn("static uint8_t s_nx_rx_console[64];", source)
        self.assertIn("nx_stm32g474_lpuart1_pins_prepare()", source)
        self.assertIn("170000000u, 1000000u", source)
        self.assertEqual(construction["stop"],
                         ["nx_uart_port_stop(&s_nx_face_console)"])
        self.assertIn("LPUART1_IRQHandler", source)
        self.assertNotIn("DMA", source)

    def test_constructor_projection_cannot_bypass_fixed_mode_validation(self):
        for changes in ({"mode": "irq-byte-event"}, {"rx_profile": "events"},
                        {"irq_priority": 0}, {"controller": "USART2"}):
            with self.subTest(changes=changes):
                with self.assertRaises(ConfigurationError):
                    self.provider.constructor(uart(**changes), Result(), {}, {}, ())

    def test_foreign_typed_provider_decision_cannot_be_reinterpreted(self):
        item = ControllerIR.create(uart(), "stm32f407")
        with self.assertRaisesRegex(ConfigurationError, "provider"):
            self.provider.constructor(item, Result(), {}, {}, ())

    def test_uart_ctor_uses_selected_kernel_policy_without_hidden_os(self):
        for backend, policy, ceiling in (("baremetal", "NONE", 0),
                                          ("freertos", "BASEPRI", 5)):
            result = Result(uart(), backend=backend)
            source = "\n".join(self.provider.constructor(
                result.controllers[0], result, {}, {}, ())["definitions"])
            self.assertIn(f"NX_IRQ_KERNEL_{policy}, 102u, 4u, {ceiling}u", source)
            self.assertNotIn("xTaskCreate", source)

    def test_empty_profile_retains_no_io_lease_or_provider_source(self):
        result = configure.resolve_ir(
            ROOT / "tests/contracts/g474_assembly/empty.toml")
        self.assertEqual(self.provider.state_declarations(result), [])
        self.assertEqual(self.provider.start_resources(result), [])
        self.assertEqual(self.provider.shared_irqs(result), [])
        self.assertNotIn("gpioa", "\n".join(self.provider.stop_system(result)))

    def test_gpio_mask_snapshot_precedes_any_pin_effect_and_has_readback(self):
        result = configure.resolve_ir(
            ROOT / "tests/contracts/g474_assembly/io.toml")
        source = bindings.emit(result)[1]
        self.assertIn("NX_G474_READ(&GPIOA->MODER)", source)
        self.assertIn("NX_G474_READ(&RCC->AHB2ENR)", source)
        acquire = source[source.index("nx_platform_pins_acquire(void)"):
                         source.index("nx_platform_pins_release(void)")]
        self.assertLess(acquire.index("NX_G474_WRITE(&RCC->AHB2ENR"),
                        acquire.index("nx_platform_pins_admit(false)"))
        self.assertLess(acquire.index("nx_platform_pins_admit(false)"),
                        acquire.index("nx_platform_pin_capture("))
        self.assertLess(acquire.index("nx_platform_pin_capture("),
                        acquire.index("s_nx_platform_pins_captured = true"))
        start = source[source.index("nx_platform_start_result_t "
                                    "nx_platform_start(void)"):]
        self.assertLess(start.index("nx_platform_pins_acquire()"),
                        start.index("nx_stm32g474_system_start("))
        self.assertLess(start.index("nx_platform_pins_acquire()"),
                        start.index("prepare_"))
        release = source[source.index("nx_platform_pins_release(void)"):
                         source.index("nx_result_t nx_platform_stop(void)")]
        self.assertLess(release.index("nx_platform_pin_restore("),
                        release.index("nx_platform_gpio_clocks_release()"))
        self.assertIn("return NX_ERROR_IO;", release)

    def test_gpios_and_uart_use_real_readonly_tables_without_forwarders(self):
        for values in (gpio(), uart()):
            item = ControllerIR.create(values, "stm32g474")
            self.assertEqual(self.provider.operations(item),
                             f"nx_stm32g474_{item.kind}_ops")
            self.assertEqual(self.provider.instance_declarations(item, Result()), [])


if __name__ == "__main__":
    unittest.main()
