"""Exact G474 projection verifies source identity through the existing boundary."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate


class G474ConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.fixture = gate.ROOT / "tests/contracts/g474_assembly/io.toml"

    def assembly(self, change=lambda text: text):
        text = self.fixture.read_text().replace(
            '"../../../boards/stm32g474re_hse24_reference"',
            '"' + str(gate.ROOT / 'boards/stm32g474re_hse24_reference') + '"')
        path = self.directory / "assembly.toml"
        path.write_text(change(text))
        return path

    def resolve(self, path):
        return gate.resolve_ir(path)

    def test_single_ram_alias_exact_irqs_and_initial_provider_scope(self):
        result = self.resolve(self.assembly())
        self.assertEqual(result.part, "STM32G474RET6")
        self.assertEqual(result.irq.external_count, 102)
        self.assertEqual(result.clock["core_hz"], 170000000)
        self.assertEqual([(item["origin"], item["size"]) for item in result.memory],
                         [(0x20000000, 98304), (0x10000000, 32768)])
        self.assertEqual([(item.kind, item.mode) for item in result.controllers],
                         [("gpio", "output"), ("uart", "irq-bytes")])
        self.assertFalse(result.physical_qualified)
        self.assertEqual(sum(result.flash["erase_blocks"]), 524288)

    def test_g4_source_dependency_uses_g4_checkout_identity(self):
        with patch.object(gate, "dependency_identity", wraps=gate.dependency_identity) as dependency:
            result = gate.resolve_ir(self.assembly())
        paths = [call.args[1] for call in dependency.call_args_list]
        self.assertEqual(paths, ["vendors/arm/CMSIS_6", "vendors/st/cmsis_device_g4",
                                 "vendors/st/stm32g4_hal"])
        self.assertNotIn("vendors/st/cmsis_device_f4", paths)
        self.assertEqual(result.source_dependencies[1]["commit"],
                         "626ee412334a5ed2e5b320af5a8d77d69f03a558")
        self.assertEqual(result.source_dependencies[2]["commit"],
                         "f5929f431f9effe45fbe18f5337e4753ced9ac92")

    def test_byte_mode_emits_exact_static_ring_and_counts_before_cold_call(self):
        output = self.directory / "generated"
        gate.configure(self.assembly(), output)
        source = (output / "bindings.c").read_text()
        self.assertIn("static uint8_t s_nx_rx_console[64];", source)
        self.assertIn("static nx_stm32g474_uart_state_t s_nx_port_console;", source)
        self.assertIn("nx_stm32g474_uart_ops", source)
        start = source.index("nx_platform_start_result_t nx_platform_start")
        for index, name in ((1, "led"), (2, "console")):
            self.assertLess(source.index(f"s_nx_platform_initialized = {index}u;", start),
                            source.index(f"result.primary = nx_stm32g474_{'gpio' if name == 'led' else 'uart'}_prepare(&s_nx_port_{name},", start))
        self.assertNotIn("nx_stm32_uart_ops", source)

    def test_fixed_lpuart_rejects_events_blocks_dma_and_priority_drift(self):
        for old, new in (('format = "bytes"', 'format = "events"'),
                         ('format = "bytes"', 'format = "blocks"'),
                         ('mode = "irq-bytes"', 'mode = "dma-tx"'),
                         ("priority = 5", "priority = 6")):
            with self.subTest(new=new), self.assertRaises(ValueError):
                self.resolve(self.assembly(lambda text: text.replace(old, new)))

    def test_freertos_policy_and_exact_clock_rejection(self):
        result = self.resolve(self.assembly(lambda text: text.replace(
            'backend = "baremetal"', 'backend = "freertos"')))
        self.assertEqual(result.kernel.kernel_port, "GCC/ARM_CM4F")
        self.assertEqual(result.irq.syscall_priority, 5)
        with self.assertRaises(ValueError):
            self.resolve(self.assembly(lambda text: text.replace(
                'clock = "hse24-pll170"', 'clock = "hse8-pll168"')))
        with self.assertRaises(ValueError):
            self.resolve(self.assembly(lambda text: text.replace(
                'backend = "baremetal"', 'backend = "threadx"')))


if __name__ == "__main__":
    unittest.main()
