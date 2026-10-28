"""Exact H743 continuous RX selection, static ownership and derived policy."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure, selected_contracts
from tools.configure.providers.common import ConfigurationError


ROOT = Path(__file__).resolve().parents[5]
ASSEMBLIES = ROOT / "tools/configure/assemblies"


class H743ContinuousConfiguration(unittest.TestCase):
    def source(self, directory, transform=lambda value: value):
        body = (ASSEMBLIES / "h743-uart-continuous.toml").read_text()
        body = body.replace("../../../boards/stm32h743zi_hsi_reference",
                            str(ROOT / "boards/stm32h743zi_hsi_reference"))
        path = Path(directory) / "assembly.toml"
        path.write_text(transform(body))
        return path

    def test_selected_cache_policy_is_derived_without_an_author_field(self):
        for suffix, line, policy in (("", 0, "provider_domain"),
                                      ("-cache-on", 32, "domain_cache_line")):
            with self.subTest(suffix=suffix):
                result = configure.resolve_ir(
                    ASSEMBLIES / ("h743-uart-continuous" + suffix + ".toml"))
                item = result.controllers[0]
                record = selected_contracts.describe(item)
                self.assertEqual((line, policy, "double_buffer"),
                                 (record.cache_line_bytes, record.dma_policy,
                                  record.continuity))
                self.assertEqual((1, "UINT32_MAX"),
                                 (record.buffer_alignment,
                                  record.max_transfer_units))
                self.assertFalse(result.physical_qualified)
        with tempfile.TemporaryDirectory() as directory:
            path = self.source(directory, lambda text: text.replace(
                'mode = "rx-dma-continuous"',
                'mode = "rx-dma-continuous"\ndma_cache_line_bytes = 0'))
            with self.assertRaises(ConfigurationError):
                configure.resolve_ir(path)

    def test_normal_generation_keeps_only_one_rx_lane_and_no_hidden_pool(self):
        with tempfile.TemporaryDirectory() as directory:
            source = self.source(directory)
            output = Path(directory) / "generated"
            configure.configure(source, output)
            body = (output / "bindings.c").read_text()
            self.assertIn("nx_stm32h743_uart_continuous_state_t", body)
            self.assertIn("nx_stm32h743_uart_continuous_initialize", body)
            self.assertIn(".uart.rx_storage = NULL", body)
            self.assertIn(".uart.rx_capacity = 0u", body)
            self.assertIn(".rx = DMA1_Stream1", body)
            self.assertIn("DMA1_Stream1_IRQHandler", body)
            self.assertNotIn("DMA1_Stream0_IRQHandler", body)
            self.assertNotIn("static uint8_t s_nx_rx_", body)
            self.assertNotIn("nx_stm32h743_uart_receive_initialize", body)

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
                path = self.source(directory, transform)
                with self.assertRaises(ConfigurationError):
                    configure.resolve_ir(path)


if __name__ == "__main__":
    unittest.main()
