"""Persisted enhanced selections traverse the normal atomic bundle path."""
from pathlib import Path
import json
import tempfile
import unittest

from tools.configure import configure


class IOEvolutionFixtures(unittest.TestCase):
    def test_every_fixture_has_a_normal_explicit_retention_preset(self):
        presets = json.loads((configure.ROOT / "CMakePresets.json").read_text())
        configured = {item["name"]: item
                      for item in presets["configurePresets"]}
        built = {item["name"]: item for item in presets["buildPresets"]}
        root = configure.ROOT / "tests/contracts/io_evolution_assembly"
        for fixture in root.glob("*.toml"):
            with self.subTest(fixture=fixture.name):
                name = "fixture-io-evolution-" + fixture.stem
                self.assertIn(name, configured)
                self.assertIn(name, built)
                self.assertEqual(built[name]["configurePreset"], name)
                variables = configured[name]["cacheVariables"]
                self.assertEqual(variables["NEXUS_ASSEMBLY_FILE"],
                    "${sourceDir}/tests/contracts/io_evolution_assembly/" +
                    fixture.name)
                self.assertEqual(variables["NEXUS_BUILD_WORKLOAD"], "OFF")
                self.assertEqual(variables["NEXUS_IO_EVOLUTION_FIXTURE"], "ON")

    def test_all_enhanced_fixtures_generate_real_typed_factories(self):
        fixtures = {
            "native-uart9n1": "nx_factory_uart_word(",
            "f407-uart7e2": "nx_factory_uart(",
            "f407-uart9n1": "nx_factory_uart_word(",
            "gd470-uart7e2": "nx_factory_uart(",
            "gd470-uart9n1": "nx_factory_uart_word(",
            "f407-spi-transaction": "nx_factory_spi_transaction_device(",
            "gd470-spi-transaction": "nx_factory_spi_transaction_device(",
            "f407-spi-dma-transaction": "nx_factory_spi_dma_transaction(",
            "gd470-spi-dma-transaction": "nx_factory_spi_dma_transaction(",
            "g474-i2c-long-timing": "nx_factory_i2c_long_device(",
            "h743-i2c-long-timing": "nx_factory_i2c_long_device(",
            "u575-i2c-long-timing": "nx_factory_i2c_long_device(",
        }
        root = configure.ROOT / "tests/contracts/io_evolution_assembly"
        with tempfile.TemporaryDirectory() as temporary:
            for name, getter in fixtures.items():
                with self.subTest(fixture=name):
                    output = Path(temporary) / name
                    configure.configure(root / (name + ".toml"), output)
                    self.assertTrue((output / ".nexus-config-bundle").is_file())
                    self.assertTrue((output / "resolved.json").is_file())
                    header = (output / "nexus_factory.h").read_text()
                    self.assertIn(getter, header)
                    self.assertIn("_traits_t", header)


if __name__ == "__main__":
    unittest.main()
