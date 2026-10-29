"""Explicit H743 duplex selection never changes IRQ-TX receive modes."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure, selected_contracts
from tools.configure.providers.common import ConfigurationError


ROOT = Path(__file__).resolve().parents[5]
ASSEMBLIES = ROOT / "tools/configure/assemblies"


class H743DuplexConfiguration(unittest.TestCase):
    def source(self, directory, transform=lambda text: text):
        body = (ASSEMBLIES / "h743-uart-continuous.toml").read_text()
        body = body.replace("../../../boards/stm32h743zi_hsi_reference",
                            str(ROOT / "boards/stm32h743zi_hsi_reference"))
        body = body.replace('mode = "rx-dma-continuous"',
                            'mode = "dma-duplex-continuous"')
        path = Path(directory) / "assembly.toml"
        path.write_text(transform(body))
        return path

    def test_duplex_resolves_two_separate_lanes_and_finite_tx_limits(self):
        for cached in (False, True):
            with self.subTest(cached=cached), tempfile.TemporaryDirectory() as directory:
                source = self.source(directory, lambda text: text.replace(
                    "hsi64-fixed-cache-off", "hsi64-fixed-cache-on")
                    if cached else text)
                result = configure.resolve_ir(source)
                item = result.controllers[0]
                self.assertEqual(("DMA1:stream0:channel46",
                                  "DMA1:stream1:channel45"), item.dma)
                record = selected_contracts.describe(item)
                self.assertEqual("double_buffer", record.continuity)
                self.assertEqual((32, "65504") if cached else
                                 (1, "UINT16_MAX"),
                                 (record.buffer_alignment,
                                  record.max_transfer_units))
                self.assertFalse(result.physical_qualified)

    def test_generation_has_one_uart_owner_and_both_explicit_vectors(self):
        with tempfile.TemporaryDirectory() as directory:
            source = self.source(directory)
            output = Path(directory) / "generated"
            configure.configure(source, output)
            body = (output / "bindings.c").read_text()
            self.assertIn("nx_stm32h743_uart_duplex_state_t", body)
            self.assertIn("nx_stm32h743_uart_duplex_prepare", body)
            self.assertIn(".storage = NULL", body)
            self.assertIn(".capacity = 0u", body)
            result = configure.resolve_ir(source)
            self.assertEqual(
                [("DMA1", 0, 46), ("DMA1", 1, 45)],
                [(lane.engine, lane.lane, lane.request)
                 for lane in result.dma_topology.lanes])
            self.assertIn("nx_stm32h743_uart_duplex_receive_irq("
                          "&s_nx_port_console)", body)
            self.assertIn("nx_stm32h743_uart_duplex_dma_irq("
                          "&s_nx_port_console)", body)
            self.assertIn("DMA1_Stream0_IRQHandler", body)
            self.assertIn("DMA1_Stream1_IRQHandler", body)
            self.assertNotIn("static uint8_t s_nx_rx_", body)
            self.assertNotIn("nx_stm32h743_uart_dma_state_t", body)

    def test_unreviewed_clock_backend_and_irq_policy_reject(self):
        transforms = [lambda text, profile=profile: text.replace(
            "hsi64-fixed-cache-off", profile) for profile in
            ("hsi32-fixed-cache-off", "hsi32-fixed-cache-on",
             "hsi64-pll384-cache-off")]
        transforms += [lambda text, backend=backend: text.replace(
            'backend = "baremetal"', 'backend = "' + backend + '"')
            for backend in ("freertos", "threadx")]
        transforms += [lambda text: text.replace("calls_os = false",
                                                "calls_os = true")]
        for transform in transforms:
            with tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(ConfigurationError):
                    configure.resolve_ir(self.source(directory, transform))


if __name__ == "__main__":
    unittest.main()
