"""Exact extra UARTs keep APB, vector and no-DMA admission independent."""
from tools.configure.tests.native_fixture import native_fixture
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure.stm32f4 import preflight
from tools.configure import bindings, configure as gate
from tools.configure.providers import hardware


class F407UartInstanceTests(unittest.TestCase):
    def test_selected_uart_instances_have_exact_immutable_clock_and_irq(self):
        for controller, apb, irq in (
                ("USART3", 1, 39), ("UART4", 1, 52),
                ("UART5", 1, 53), ("USART6", 2, 71)):
            with self.subTest(controller=controller):
                selected = hardware.maintained('stm32f4', controller)
                self.assertEqual(selected.kind, "uart")
                self.assertEqual(selected.apb, apb)
                self.assertEqual(selected.irqs, ((controller, irq),))
                self.assertEqual(preflight._clock(controller),
                    (f"RCC->APB{apb}ENR",
                     f"RCC_APB{apb}ENR_{controller}EN"))
                with self.assertRaises(FrozenInstanceError):
                    selected.apb = 3 - apb

    def test_new_tx_routes_do_not_authorize_an_unreviewed_receive_dma_path(self):
        for controller in ("USART3", "UART4", "UART5", "USART6"):
            for mode in ("rx-dma-idle", "rx-dma-continuous",
                         "dma-duplex", "dma-duplex-continuous"):
                with self.subTest(controller=controller, mode=mode):
                    selected = hardware.maintained('stm32f4', controller)
                    self.assertIsNone(selected.dma(mode))

    def test_all_six_finite_dma_transmitters_generate_their_exact_static_route(self):
        routes = (("USART1", "usart1-pa9-pa10", 2, 7, 4),
                  ("USART2", "usart2-pa2-pa3", 1, 6, 4),
                  ("USART3", "usart3-pb10-pb11", 1, 3, 4),
                  ("UART4", "uart4-pa0-pa1", 1, 4, 4),
                  ("UART5", "uart5-pc12-pd2", 1, 7, 4),
                  ("USART6", "usart6-pc6-pc7", 2, 6, 5))
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(gate.ROOT /
                              "boards/stm32f407_qiming_v31/board.json")
            board["id"] = "six-uart-dma-software-fixture"
            board["provenance"] = []
            board["unknowns"] = ["Software pad fixture; no PCB or HIL claim"]
            for part in ("STM32F407VET6", "STM32F407ZGT6"):
                for backend in ("baremetal", "freertos"):
                    for controller, route, engine, stream, channel in routes:
                        with self.subTest(part=part, backend=backend,
                                          controller=controller):
                            board["soc"] = part
                            board["bindings"] = [{"id": "serial", "controller": controller,
                                                  "route": route}]
                            (directory / "board.json").write_text(json.dumps(board))
                            assembly = directory / "assembly.toml"
                            assembly.write_text(
                                'schema=2\nboard_package="."\nbackend=' + json.dumps(backend) +
                                '\nclock="hse8-pll168"\noptimization="Os"\n'
                                '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                                'flash_load_limit_bytes=65536\nstatic_ram_limit_bytes=32768\n'
                                '[uart.serial]\nbinding="serial"\nmode="dma-tx"\n'
                                'baud=115200\n[uart.serial.rx]\nformat="events"\ncapacity=2\n'
                                '[uart.serial.irq]\npriority=5\ncalls_os=false\n')
                            result = gate.resolve_ir(assembly)
                            selected, = result.controllers
                            self.assertEqual([(lane.engine, lane.lane, lane.request)
                                              for lane in selected.integration.dma],
                                             [(f"DMA{engine}", stream, channel)])
                            source = bindings.emit(result)[1]
                            self.assertIn("nx_stm32_uart_hal_cold_prepare", source)
                            self.assertIn(f"DMA{engine}_Stream{stream}_IRQHandler", source)
                            self.assertIn(f"{stream}u, {channel}u", source)

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
            result = gate.resolve_ir(assembly, test_support=native_fixture(assembly))
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
