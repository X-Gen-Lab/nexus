"""Controller ordinals and optional cold metadata use resolved identities."""
from tools.configure.tests.native_fixture import native_fixture, canonical_assembly
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure


ROOT = configure.ROOT


class FactoryDiagnosticTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def generate(self, name, *, capabilities=False):
        contents = (canonical_assembly(ROOT, name)).read_text()
        if capabilities:
            contents += "\n[factory]\ncapabilities = true\n"
        assembly = self.root / "assembly.toml"
        assembly.write_text(contents)
        output = self.root / "generated"
        configure.configure(assembly, output, test_support=native_fixture(assembly))
        return output

    def test_default_has_kind_qualified_ordinals_without_runtime_metadata(self):
        output = self.generate("native-gpio.toml")
        bindings = (output / "nexus_bindings.h").read_text()
        self.assertIn("NX_PLATFORM_CONTROLLER_INDEX_GPIO_LED0 = 0", bindings)
        self.assertIn("NX_PLATFORM_CONTROLLER_INDEX_COUNT = 1", bindings)
        self.assertNotIn("nx_factory_controller_label",
                         (output / "nexus_factory.h").read_text())
        self.assertNotIn("nx_factory_controller_capability",
                         (output / "bindings.c").read_text())

    def test_optional_diagnostic_lookup_reuses_one_existing_capability(self):
        output = self.generate("native-gpio.toml", capabilities=True)
        source = (output / "bindings.c").read_text()
        header = (output / "nexus_factory.h").read_text()
        self.assertIn("nx_factory_controller_label", header)
        self.assertIn("nx_factory_controller_capability", header)
        self.assertIn("NX_FACTORY_CONTROLLER_HARDWARE", header)
        self.assertEqual(source.count(
            "static const nx_factory_io_capability_t s_nx_capability_gpio_led0"), 1)
        self.assertNotIn("nx_factory_controller_descriptor_t", header)

    def test_empty_assembly_has_zero_ordinal_count(self):
        output = self.generate("native-empty.toml", capabilities=True)
        self.assertIn("NX_PLATFORM_CONTROLLER_INDEX_COUNT = 0",
                      (output / "nexus_bindings.h").read_text())
        self.assertIn("nx_factory_controller_label",
                      (output / "nexus_factory.h").read_text())




if __name__ == "__main__":
    unittest.main()
