"""Public I/O requirements follow selected typed package bindings."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.configure import configure, emission
from tools.configure.data_provider import Binding
from tools.configure.tests.test_data_provider import software


ROOT = Path(__file__).resolve().parents[3]


class IORequirementTests(unittest.TestCase):
    def test_binding_requirements_are_immutable_public_identifiers(self):
        value = software()["bindings"][0]
        value["io_domains"] = ["uart_word", "uart_stream"]
        binding = Binding.create(value)
        self.assertEqual(binding.io_domains, ("uart_word", "uart_stream"))
        with self.assertRaises(FrozenInstanceError):
            binding.io_domains = ()

    def test_invalid_or_duplicate_binding_requirements_reject(self):
        for domains in ("uart", ["uart", "uart"], [None],
                        ["uart;spi"], ["../uart"], [["uart"]]):
            with self.subTest(domains=domains):
                value = software()["bindings"][0]
                value["io_domains"] = domains
                with self.assertRaisesRegex(ValueError, "I/O domains"):
                    Binding.create(value)

    def test_provider_mode_names_do_not_enter_the_common_domain_policy(self):
        from tools.configure.tests.test_data_provider import DataProviderTests
        value = software()
        value["bindings"][0].update(mode="vendor-events",
                                      io_domains=["uart_stream"])
        provider = DataProviderTests().provider(value)
        selected = SimpleNamespace(kind="gpio", mode="vendor-events",
                                   controller="GPIOA")
        self.assertEqual(provider.io_domains(selected), ("uart_stream",))

    def test_real_selected_modes_project_only_their_required_domains(self):
        cases = (
            ("tools/configure/assemblies/qiming-baremetal-empty.toml", ()),
            ("tests/contracts/uart_rx_compact_assembly/f407-irq.toml", ("uart",)),
            ("tests/contracts/io_evolution_assembly/f407-uart9n1.toml",
             ("uart", "uart_word")),
            ("tests/contracts/uart_rx_compact_assembly/f407-irq-blocks.toml",
             ("uart", "uart_stream")),
            ("tests/contracts/uart_rx_compact_assembly/f407-rx-dma-idle.toml",
             ("uart", "uart_stream")),
            ("tests/contracts/uart_rx_compact_assembly/f407-dma-duplex.toml",
             ("uart", "uart_stream")),
            ("tests/contracts/adc_external_assembly/ve-scan.toml", ("adc",)),
            ("tests/contracts/adc_external_assembly/ve-dma.toml",
             ("adc", "adc_stream")),
            ("tests/contracts/dac_assembly/ve-direct.toml", ("dac",)),
            ("tests/contracts/dac_assembly/ve-dma.toml", ("dac", "dac_tx")),
        )
        for name, expected in cases:
            with self.subTest(assembly=name):
                result = configure.resolve_ir(ROOT / name)
                self.assertEqual(result.required_io_domains, expected)
                with tempfile.TemporaryDirectory() as folder:
                    output = Path(folder)
                    emission.emit_bundle(result, output)
                    self.assertIn('set(NEXUS_REQUIRED_IO_DOMAINS "' +
                                  ";".join(expected) + '")',
                                  (output / "selection.cmake").read_text())

    def test_multiple_selected_modes_do_not_duplicate_domains(self):
        result = configure.resolve_ir(ROOT / (
            "tests/contracts/uart_rx_compact_assembly/f407-mixed-duplex-tx.toml"))
        self.assertEqual(result.required_io_domains, ("uart", "uart_stream"))


if __name__ == "__main__":
    unittest.main()
