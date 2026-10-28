"""Test fixtures exercise the public TOML parser without a JSON input path."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure
from tools.configure.tests.assembly_fixture import render_toml


class FixtureAssemblyTests(unittest.TestCase):
    def fixture(self):
        return {
            "schema_version": 1,
            "board_package": str(configure.ROOT / "boards/native_reference"),
            "backend": "native", "clock_profile": "model",
            "controllers": [{"id": "console", "authored_kind": "uart",
                "binding": "uart0", "mode": "irq-byte-event", "baud": 115200,
                "irq_priority": 5, "calls_os": False, "rx_profile": "bytes",
                "rx_capacity": 32}],
            "devices": [], "memory_budgets": {"main_stack_bytes": 2048},
            "layout": None}

    def resolve(self, document):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "assembly.toml"
            path.write_text(render_toml(document), encoding="utf-8")
            return configure.resolve_ir(path)

    def test_uart_fixture_uses_authored_rx_and_irq_tables(self):
        result = self.resolve(self.fixture())
        uart, = result.controllers
        self.assertEqual(uart.options.baud, 115200)
        self.assertEqual(uart.options.rx_capacity, 32)
        self.assertEqual(uart.options.irq.priority, 5)
        self.assertFalse(uart.options.irq.calls_os)

    def test_unknown_fields_and_boolean_numbers_reach_production_rejection(self):
        document = self.fixture()
        document["business_worker"] = True
        with self.assertRaisesRegex(ValueError, "unknown.*business_worker"):
            self.resolve(document)
        document.pop("business_worker")
        document["controllers"][0]["baud"] = True
        with self.assertRaisesRegex(ValueError, "baud"):
            self.resolve(document)


if __name__ == "__main__":
    unittest.main()
