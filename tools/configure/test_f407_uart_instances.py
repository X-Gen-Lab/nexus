"""Exact extra UARTs keep APB, vector and no-DMA admission independent."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from soc.stm32.stm32f407.configure import preflight
from tools.configure import bindings, configure as gate
from tools.configure.providers import hardware


class F407UartInstanceTests(unittest.TestCase):
    def test_selected_uart_instances_have_exact_immutable_clock_and_irq(self):
        for controller, apb, irq in (
                ("USART3", 1, 39), ("UART4", 1, 52),
                ("UART5", 1, 53), ("USART6", 2, 71)):
            with self.subTest(controller=controller):
                selected = hardware.maintained("stm32f407", controller)
                self.assertEqual(selected.kind, "uart")
                self.assertEqual(selected.apb, apb)
                self.assertEqual(selected.irqs, ((controller, irq),))
                self.assertEqual(preflight._clock(controller),
                    (f"RCC->APB{apb}ENR",
                     f"RCC_APB{apb}ENR_{controller}EN"))
                with self.assertRaises(FrozenInstanceError):
                    selected.apb = 3 - apb

    def test_new_irq_routes_do_not_authorize_an_unreviewed_dma_path(self):
        for controller in ("USART3", "UART4", "UART5", "USART6"):
            for mode in ("dma-tx", "rx-dma-idle", "rx-dma-continuous",
                         "dma-duplex", "dma-duplex-continuous"):
                with self.subTest(controller=controller, mode=mode):
                    selected = hardware.maintained("stm32f407", controller)
                    self.assertIsNone(selected.dma(mode))

    def test_unreviewed_uart_name_does_not_infer_a_clock_gate(self):
        with self.assertRaisesRegex(ValueError, "Unmaintained controller"):
            preflight._clock("UART7")

    def test_usart2_byte_events_are_selectable_without_dma_claims(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(gate.ROOT /
                              "boards/stm32f407_qiming_v31/board.json")
            board["id"] = "uart2-byte-route-software-fixture"
            board["provenance"] = []
            board["unknowns"] = ["Software pad fixture; no physical result"]
            board["bindings"] = [{"id": "serial2", "controller": "USART2",
                                  "route": "usart2-pa2-pa3"}]
            (directory / "board.json").write_text(json.dumps(board))
            assembly = directory / "assembly.toml"
            assembly.write_text(
                'schema=2\nboard_package="."\nbackend="baremetal"\n'
                'clock="hse8-pll168"\noptimization="Os"\n'
                '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                'flash_load_limit_bytes=65536\n'
                'static_ram_limit_bytes=32768\n'
                '[uart.serial2]\nbinding="serial2"\n'
                'mode="irq-byte-event"\nbaud=115200\n'
                '[uart.serial2.rx]\nformat="bytes"\ncapacity=16\n'
                '[uart.serial2.irq]\npriority=5\n')
            result = gate.resolve_ir(assembly)
            selected, = result.controllers
            self.assertEqual(selected.controller, "USART2")
            self.assertEqual(selected.mode, "irq-byte-event")
            self.assertEqual(selected.integration.clock_hz, 42000000)
            self.assertEqual(selected.integration.dma, ())
            self.assertFalse(any(claim.key.startswith("dma:")
                                 for claim in result.resource_plan.claims))
            source = bindings.emit(result)[1]
            self.assertIn("nx_stm32_uart_byte_cold_prepare", source)
            self.assertIn("void USART2_IRQHandler(void)", source)
            self.assertNotIn("nx_stm32_uart_hal_cold_prepare", source)


if __name__ == "__main__":
    unittest.main()
