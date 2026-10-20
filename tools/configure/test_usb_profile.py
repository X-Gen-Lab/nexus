"""Selected USB-specific profile generation and metadata opt-out."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure


class UsbProfileGenerationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def generate(self, family, capabilities=True):
        source = configure.ROOT / "tests/contracts/usb_assembly" / (family + ".toml")
        text = source.read_text().replace(
            'board_package="' + family + '-board"',
            'board_package=' + json.dumps(str(source.parent / (family + "-board"))))
        if not capabilities:
            text = text.replace("capabilities=true", "capabilities=false")
        assembly = self.root / (family + str(capabilities) + ".toml")
        assembly.write_text(text)
        output = self.root / (family + str(capabilities))
        configure.configure(assembly, output)
        return ((output / "nexus_factory.h").read_text(),
                (output / "bindings.c").read_text())

    def test_selected_usb_has_a_distinct_typed_usb_profile_getter(self):
        for family in ("stm32-zg", "stm32-ve", "gd32"):
            with self.subTest(family=family):
                header, source = self.generate(family)
                self.assertIn("nx_factory_usb_profile", header)
                self.assertIn("const nx_usb_device_profile_t*", header)
                self.assertIn("nx_factory_usb_profile", source)
                # Existing generic capability getters retain their own ABI.
                self.assertIn("nx_factory_usb_device_capability", header)
                self.assertIn("nx_factory_usb_endpoint_capability", header)

    def test_capabilities_false_does_not_emit_usb_profile_storage_or_api(self):
        for family in ("stm32-zg", "gd32"):
            with self.subTest(family=family):
                header, source = self.generate(family, capabilities=False)
                self.assertNotIn("nx_factory_usb_profile", header)
                self.assertNotIn("nx_usb_device_profile_t", source)
                self.assertNotIn("nx_usb_endpoint_profile_t", source)


if __name__ == "__main__":
    unittest.main()
