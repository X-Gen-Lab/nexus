"""UART layout profiles select static recipes without conditional expressions."""
from dataclasses import replace
import unittest

from tools.configure import configure, uart_formats


class ExecutionProfileTests(unittest.TestCase):
    def test_public_receive_and_format_semantics_determine_one_profile(self):
        for relative, expected in (
                ("uart_rx_compact_assembly/f407-irq.toml", "bytes-default"),
                ("io_evolution_assembly/f407-uart7e2.toml", "events-formatted"),
                ("io_evolution_assembly/f407-uart9n1.toml", "words-default"),
                ("io_evolution_assembly/f407-uart9n1-probe.toml", "words-default-probed"),
                ("stm32_assembly/zg-blocks.toml", "blocks-default")):
            with self.subTest(assembly=relative):
                result = configure.resolve_ir(configure.ROOT / "tests/contracts" / relative)
                item = next(owner for owner in result.controllers if owner.kind == "uart")
                self.assertEqual(uart_formats.execution_profile(item), expected)
                self.assertEqual(item.integration.execution_profile, expected)
                if item.mode == "irq-byte-event":
                    events = replace(item, options=replace(item.options, rx_profile="events"))
                    self.assertEqual(uart_formats.execution_profile(events),
                                     expected.replace("bytes", "events"))


if __name__ == "__main__":
    unittest.main()
