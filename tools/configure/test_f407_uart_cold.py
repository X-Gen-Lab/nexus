"""Selected F407 UART construction keeps hardware lifecycle in C."""
from dataclasses import replace
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate
from tools.configure.ir import UartFrameOptions
from tools.configure.providers import maintained


class F407UartColdTests(unittest.TestCase):
    def dma_result(self, *, events=False, frame=None):
        source = gate.ROOT / (
            "tests/contracts/uart_rx_compact_assembly/f407-dma-tx.toml")
        content = source.read_text()
        if events:
            content = content.replace('format = "bytes"', 'format = "events"')
        if frame is not None:
            content += "\n[uart.uart1.frame]\n" + frame + "\n"
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "assembly.toml"
            path.write_text(content)
            return gate.resolve_ir(path)

    def test_dma_tx_selects_hal_state_ops_and_same_retained_cold_resources(
            self):
        provider = maintained("stm32f407")
        for relative in (
                "tests/contracts/uart_rx_compact_assembly/f407-dma-tx.toml",
                "tests/contracts/multi_controller_assembly/stm32-zg.toml",
                "tests/contracts/multi_controller_assembly/"
                "stm32-ve-freertos.toml"):
            result = gate.resolve_ir(gate.ROOT / relative)
            header, source = bindings.emit(result)
            self.assertIn('#include "stm32f407_uart_hal.h"', source)
            for private in ("HAL_", "UART_HandleTypeDef", "uart_hal"):
                self.assertNotIn(private, header)
            for item in result.controllers:
                if item.kind != "uart" or item.mode != "dma-tx":
                    continue
                with self.subTest(assembly=relative,
                                  controller=item.controller):
                    constructed = provider.constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    self.assertEqual(provider.mode_implementation(
                        item.kind, item.mode), "uart_hal")
                    self.assertEqual(constructed.state_type,
                                     "nx_stm32_uart_hal_state_t")
                    self.assertEqual(provider.operations(item),
                                     "nx_stm32_uart_hal_ops")
                    self.assertEqual(constructed.initialize.function,
                                     "nx_stm32_uart_hal_cold_prepare")
                    self.assertEqual(constructed.stop[0].function,
                                     "nx_stm32_uart_hal_cold_close")
                    self.assertEqual(len(constructed.initialize.arguments), 3)
                    self.assertEqual(constructed.initialize.arguments,
                                     constructed.stop[0].arguments)
                    self.assertTrue(constructed.retain_on_failed_init)
                    self.assertIn("nx_stm32_uart_dma_cold_config_t", source)
                    self.assertIn(".rx_storage = s_nx_rx_", source)

    def test_hal_dma_tx_preserves_byte_event_rx_and_both_irq_owners(self):
        default = 'data_bits=8\nparity="none"\nstop_bits=1\nflow="none"'
        for events in (False, True):
            for frame in (None, default):
                with self.subTest(events=events,
                                  explicit_frame=frame is not None):
                    result = self.dma_result(events=events, frame=frame)
                    _, source = bindings.emit(result)
                    storage = "nx_uart_rx_event_t" if events else "uint8_t"
                    profile = ("NX_UART_RX_EVENTS" if events else
                               "NX_UART_RX_BYTES")
                    self.assertIn(f"static {storage} s_nx_rx_uart1[32]", source)
                    self.assertIn(".profile = " + profile, source)
                    self.assertIn(
                        "void USART1_IRQHandler(void) { "
                        "nx_stm32_uart_hal_irq(&s_nx_port_uart1); }", source)
                    self.assertIn(
                        "void DMA2_Stream7_IRQHandler(void) { "
                        "nx_stm32_uart_hal_dma_irq(&s_nx_port_uart1); }",
                        source)
                    self.assertNotIn("nx_stm32_uart_dma_uart_irq(", source)
                    self.assertNotIn("nx_stm32_uart_dma_irq(", source)

    def test_internal_format_cannot_bypass_the_selected_hal_composite_irq(self):
        result = self.dma_result()
        item, = result.controllers
        # This isolated constructor input does not expand the authored gate.
        changed = replace(item, options=replace(
            item.options, frame=UartFrameOptions(7, "even", 2, "none")))
        provider = maintained("stm32f407")
        constructed = provider.constructor(
            changed, result, result.board_bindings, result.routes,
            result.devices)
        source = "\n".join(constructed.definitions)
        self.assertIn("NX_UART_PARITY_EVEN", source)
        self.assertIn("nx_stm32_uart_hal_irq(&s_nx_port_uart1)", source)
        self.assertNotIn("nx_stm32_uart_format_irq(", source)

    def test_nondefault_authored_dma_frame_remains_explicitly_rejected(self):
        with self.assertRaisesRegex(ValueError, "reviewed byte IRQ mode"):
            self.dma_result(frame='data_bits=7\nparity="even"\n'
                                  'stop_bits=2\nflow="none"')

    def test_other_uart_modes_keep_their_existing_concrete_owners(self):
        provider = maintained("stm32f407")
        for suffix, implementation in (
                ("irq", "uart"), ("rx-dma-idle", "uart_receive"),
                ("dma-duplex", "uart_duplex"),
                ("dma-duplex-continuous", "uart_duplex")):
            result = gate.resolve_ir(gate.ROOT / (
                "tests/contracts/uart_rx_compact_assembly/f407-" + suffix +
                ".toml"))
            item, = result.controllers
            with self.subTest(mode=item.mode):
                self.assertEqual(provider.mode_implementation(
                    item.kind, item.mode), implementation)
                _, source = bindings.emit(result)
                self.assertNotIn('#include "stm32f407_uart_hal.h"', source)
                self.assertNotIn("nx_stm32_uart_hal_", source)

    def test_every_maintained_uart_shape_is_static_wiring_to_typed_c(self):
        inputs = [
            "tools/configure/assemblies/qiming-baremetal-uart.toml",
            "tools/configure/assemblies/qiming-freertos-uart.toml",
            "tests/contracts/io_evolution_assembly/f407-uart7e2.toml",
            "tests/contracts/io_evolution_assembly/f407-uart9n1.toml",
            "tests/contracts/io_evolution_assembly/f407-uart9n1-probe.toml",
            "tests/contracts/stm32_assembly/zg-blocks.toml",
            "tests/contracts/multi_controller_assembly/stm32-zg.toml",
            "tests/contracts/multi_controller_assembly/stm32-zg-freertos.toml",
        ]
        inputs += ["tests/contracts/uart_rx_compact_assembly/f407-" + mode
                   + ".toml" for mode in (
                       "irq", "dma-tx", "rx-dma-idle", "rx-dma-continuous",
                       "dma-duplex", "dma-duplex-continuous",
                       "mixed-rx-tx", "mixed-duplex-tx")]
        seen = set()
        for relative in inputs:
            with self.subTest(assembly=relative):
                result = gate.resolve_ir(gate.ROOT / relative)
                provider = maintained("stm32f407")
                for item in result.controllers:
                    if item.kind != "uart":
                        continue
                    seen.add(item.mode)
                    constructed = provider.constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    self.assertTrue(constructed.initialize.function.startswith(
                        "nx_stm32_uart_"))
                    self.assertTrue(constructed.initialize.function.endswith(
                        "_cold_prepare"))
                    self.assertTrue(constructed.stop[0].function.endswith(
                        "_cold_close"))
                    self.assertTrue(constructed.retain_on_failed_init)
                    self.assertEqual(set(constructed.pin_snapshots),
                                     {pin["pin"] for pin in item.pins})
                    self.assertEqual(len(set(
                        constructed.pin_snapshots.values())), 2)
                    source = "\n".join(constructed.definitions)
                    for behavior in ("RCC->", "NVIC_", "goto ",
                                     "static nx_result_t prepare_",
                                     "static nx_result_t stop_"):
                        self.assertNotIn(behavior, source)
        self.assertEqual(seen, {
            "irq-byte-event", "irq-words", "irq-blocks", "dma-tx",
            "rx-dma-idle", "rx-dma-continuous", "dma-duplex",
            "dma-duplex-continuous"})


if __name__ == "__main__":
    unittest.main()
