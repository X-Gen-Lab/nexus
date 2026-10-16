"""Exact F103RB capability, static construction and cold lease contracts."""
import importlib
import json
from pathlib import Path
import unittest
import tempfile

from tools.configure import bindings, configure
from tools.configure.ir import ControllerIR
from tools.configure.providers.common import (ConfigurationError,
                                              validate_clock_profiles)

ROOT = Path(__file__).resolve().parents[4]


def uart(**changes):
    return {**{"id": "console", "binding": "uart1", "kind": "uart",
               "controller": "USART2", "mode": "irq-bytes", "baud": 115200,
               "rx_profile": "bytes", "rx_capacity": 64, "irq_priority": 5,
               "calls_os": False, "pins": [
                   {"pin": "PA2", "function": "tx", "af": 0},
                   {"pin": "PA3", "function": "rx", "af": 0}]}, **changes}


def gpio(**changes):
    return {**{"id": "led", "binding": "led0", "kind": "gpio",
               "controller": "GPIOA", "mode": "output", "mask": 32,
               "initial": 0, "pins": [
                   {"pin": "PA5", "function": "output", "af": 0}]}, **changes}


class Result(dict):
    def __init__(self, *items, backend="baremetal"):
        super().__init__(irq={"external_count": 43, "priority_bits": 4,
                              "syscall_priority": 5 if backend == "freertos"
                              else None}, devices=[], board_bindings={},
                         routes={}, kernel_profile=(
                             {"kernel_policy": "basepri"}
                             if backend == "freertos" else None))
        self.controllers = tuple(ControllerIR.create(item, "stm32f103")
                                 for item in items)


class F103ProviderTests(unittest.TestCase):
    def setUp(self):
        self.provider = importlib.import_module("tools.configure.providers.stm32f103")

    def test_exact_cpu_has_no_fpu_dsp_mpu_and_43_external_irqs(self):
        self.assertEqual(dict(self.provider.CPU_ABI),
                         {"arch": "cortex-m3", "fpu": "none",
                          "float_abi": "soft"})
        self.assertEqual(self.provider.EXTERNAL_IRQ_COUNT, 43)
        self.assertEqual(self.provider.IRQ_PRIORITY_BITS, 4)
        self.assertEqual(self.provider.FREERTOS_PORT, "GCC/ARM_CM3")
        self.assertEqual(self.provider.CPU_FEATURES["mpu_regions"], 0)
        self.assertFalse(self.provider.CPU_FEATURES["dsp"])

    def test_static_cpu_and_clock_metadata_are_immutable(self):
        with self.assertRaises(TypeError):
            self.provider.CPU_ABI["arch"] = "cortex-m4"
        with self.assertRaises(TypeError):
            self.provider.CLOCK_PROFILES["hsi8-fixed"]["core_hz"] = 72000000

    def test_only_implemented_gpio_and_uart_modes_are_selected(self):
        for kind, mode in (("gpio", "input"), ("gpio", "output"),
                           ("uart", "irq-bytes"), ("watchdog", "independent"),
                           ("exti", "edge-event"), ("spi", "short-poll")):
            self.assertEqual(kind, self.provider.mode_implementation(kind, mode))
        for kind, mode in (("uart", "dma-tx"), ("uart", "irq-blocks"),
                           ("uart", "irq-byte-event"), ("spi", "dma"),
                           ("i2c", "poll-messages"), ("can", "classic-irq")):
            with self.subTest(kind=kind, mode=mode):
                with self.assertRaises(ConfigurationError):
                    self.provider.mode_implementation(kind, mode)

    def test_actual_oversample16_divisor_limits(self):
        for baud in (123, 9600, 115200, 500000):
            self.provider.validate_selection(uart(baud=baud), uart())
        for baud in (True, 0, 122, 500001):
            with self.subTest(baud=baud):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection(uart(baud=baud), uart())

    def test_only_default_usart2_pa2_pa3_route_is_maintained(self):
        for route in (uart(controller="USART1"), uart(pins=[
                {"pin": "PA2", "function": "tx", "af": 7},
                {"pin": "PA3", "function": "rx", "af": 7}])):
            with self.assertRaises(ConfigurationError):
                self.provider.validate_selection(uart(), route)

    def test_unimplemented_profile_and_priority_reject(self):
        for change in ({"rx_profile": "events"}, {"rx_profile": "blocks"},
                       {"irq_priority": 0}, {"irq_priority": True}):
            with self.subTest(change=change):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection(uart(**change), uart())

    def test_gpio_mask_is_exact_reviewed_pa5(self):
        self.provider.validate_selection(gpio(), gpio())
        for change in ({"controller": "GPIOB"}, {"mask": 64},
                       {"initial": 64}, {"mask": True}):
            with self.subTest(change=change):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection(gpio(**change),
                                                     gpio(**change))

    def test_foreign_typed_ir_is_never_reinterpreted(self):
        item = ControllerIR.create(uart(), "stm32f407")
        with self.assertRaisesRegex(ConfigurationError, "provider"):
            self.provider.constructor(item, Result(), {}, {}, ())

    def test_constructor_guard_precedes_state_mutation(self):
        item = ControllerIR.create(uart(), "stm32f103")
        construction = self.provider.constructor(item, Result(uart()), {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertLess(source.index(".retained"), source.index(".registers ="))
        self.assertIn(".registers = USART2;", source)
        self.assertIn("void USART2_IRQHandler", source)
        self.assertIn("static uint8_t s_nx_rx_console[64];", source)
        self.assertIn("8000000u, 1000000u", source)
        self.assertTrue(construction.retain_on_failed_init)
        self.assertEqual(construction.stop,
                         ("nx_uart_port_stop(&s_nx_face_console)",))

    def test_constructor_uses_explicit_kernel_irq_policy(self):
        for backend, kernel, ceiling in (("baremetal", "NONE", 0),
                                         ("freertos", "BASEPRI", 5)):
            result = Result(uart(), backend=backend)
            source = "\n".join(self.provider.constructor(
                result.controllers[0], result, {}, {}, ()).definitions)
            self.assertIn(f"NX_IRQ_KERNEL_{kernel}, 43u, 4u, {ceiling}u", source)

    def test_exact_memory_and_clock_match_fixed_implementation(self):
        facts = json.loads((ROOT / "soc/stm32f103/soc.json").read_text())
        validate_clock_profiles(facts, self.provider)
        part = facts["variants"]["STM32F103RBT6"]
        self.assertEqual(part["flash"]["size"], 128 * 1024)
        self.assertEqual(part["flash"]["erase_blocks"], [1024] * 128)
        self.assertEqual(part["memory"], [{"id": "sram", "origin": 0x20000000,
                                         "size": 20480, "dma": False,
                                         "linker": True}])
        self.assertEqual(len(part["pins"]), 51)
        self.assertEqual(facts["reserved_resources"],
                         ["timer:TIM2", "irq:TIM2_IRQn"])
        facts["clock_profiles"]["hsi8-fixed"]["apb1_hz"] = 4000000
        with self.assertRaises(ConfigurationError):
            validate_clock_profiles(facts, self.provider)

    def test_dma_is_rejected_without_inventing_stream_topology(self):
        for action in (lambda: self.provider.validate_dma_resource(
                "dma:DMA1:stream0:channel0"),
                       lambda: self.provider.dma_irq("DMA1:stream0:channel0"),
                       lambda: self.provider.validate_dma(
                           uart(), "irq-bytes", lambda value: value)):
            with self.assertRaises(ConfigurationError):
                action()

    def test_pin_lease_uses_crl_nibbles_and_never_modern_moder(self):
        source = "\n".join(self.provider.state_declarations(Result(uart(), gpio())))
        self.assertIn("GPIOA->CRL", source)
        self.assertIn("GPIOA->BSRR", source)
        self.assertNotIn("MODER", source)
        self.assertNotIn("AFR[", source)
        self.assertNotIn("AFIO->MAPR,", source)
        self.assertLess(source.index("s_nx_f103_saved_odr |"),
                        source.index("| s_nx_f103_saved_crl"))

    def test_selected_hardware_preflight_precedes_system_effects(self):
        source = "\n".join(self.provider.start_system(Result(uart(), gpio())))
        self.assertLess(source.index("USART2_IRQn"),
                        source.index("nx_stm32f103_system_start("))
        self.assertIn("AFIO_MAPR_USART2_REMAP", source)
        self.assertIn("GPIOA->LCKR", source)

    def test_board_does_not_invent_hse_or_physical_inspection(self):
        board = json.loads((ROOT / "boards/stm32f103_reference/board.json").read_text())
        self.assertEqual(board["clocks"], {})
        self.assertIsNone(board["physical_pcb_revision"])
        self.assertTrue(any("not" in item and "inspected" in item
                            for item in board["unknowns"]))
        self.assertTrue(any("HIL" in item for item in board["unknowns"]))

    def test_watchdog_constructor_cannot_forget_pending_lsi_debug_lease(self):
        item = {"id": "iwdg", "binding": "iwdg", "kind": "watchdog",
                "controller": "IWDG", "mode": "independent", "pins": []}
        ir = ControllerIR.create(item, "stm32f103")
        construction = self.provider.constructor(ir, Result(item), {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertIn(".preparation_retained", source)
        self.assertEqual(construction.stop,
                         ("nx_stm32f103_watchdog_stop(&s_nx_port_iwdg)",))

    def test_exti_raw_edge_capacity_and_exact_line_are_checked(self):
        item = {"id": "edge0", "binding": "exti0", "kind": "exti",
                "controller": "EXTI0", "mode": "edge-event", "edge": "both",
                "event_capacity": 3, "irq_priority": 5, "calls_os": False,
                "pins": [{"pin": "PA0", "function": "input", "af": 0}]}
        self.provider.validate_selection(item, item)
        for change in ({"controller": "EXTI5"}, {"edge": []},
                       {"event_capacity": 0}, {"irq_priority": True}):
            with self.subTest(change=change):
                with self.assertRaises(ConfigurationError):
                    self.provider.validate_selection({**item, **change},
                                                     {**item, **change})
        construction = self.provider.constructor(
            ControllerIR.create(item, "stm32f103"), Result(item), {}, {}, ())
        source = "\n".join(construction.definitions)
        self.assertIn(".mux = AFIO;", source)
        self.assertIn("void EXTI0_IRQHandler", source)

    def test_spi_endpoint_and_cs_are_static_and_one_owned_lease(self):
        path = ROOT / "tools/configure/assemblies/stm32-f103-spi.toml"
        result = configure.resolve(path, ROOT)
        _, source = bindings.emit(result)
        self.assertIn("nx_stm32f103_spi_endpoint_ops", source)
        self.assertIn("s_nx_cs_peripheral", source)
        self.assertIn(".cs_mask = 16u", source)
        self.assertIn("nx_stm32f103_gpio_initialize(&s_nx_cs_peripheral, 16u)", source)
        self.assertIn("(240u & ~s_nx_f103_saved_odr)", source)
        self.assertIn("nx_stm32f103_spi_initialize", source)
        self.assertNotIn("malloc(", source)

    def test_spi_sck_and_led_pin_conflict_is_not_silently_repaired(self):
        path = ROOT / "tools/configure/assemblies/stm32-f103-spi.toml"
        text = path.read_text() + '\n[gpio.led0]\nbinding = "led0"\nmode = "output"\n'
        with tempfile.TemporaryDirectory() as directory:
            assembly = Path(directory) / "conflict.toml"
            assembly.write_text(text)
            with self.assertRaisesRegex(ConfigurationError, "conflict"):
                configure.resolve(assembly, ROOT)

    def test_maintained_assembly_resolves_static_typed_factory(self):
        assembly = ROOT / "tools/configure/assemblies/stm32-f103-baremetal.toml"
        result = configure.resolve(assembly, ROOT)
        self.assertEqual(result["soc_family"], "stm32f103")
        _, source = bindings.emit(result)
        self.assertIn("nx_stm32f103_uart_ops", source)
        self.assertIn("s_nx_port_uart1", source)
        self.assertNotIn("malloc(", source)
