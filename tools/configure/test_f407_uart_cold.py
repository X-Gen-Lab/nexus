"""Selected F407 UART construction keeps hardware lifecycle in C."""
import unittest

from tools.configure import configure as gate
from tools.configure.providers import maintained


class F407UartColdTests(unittest.TestCase):
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
