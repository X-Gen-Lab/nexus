"""Manufacturer source layout preserves exact family and SDK boundaries."""
from tools.configure.tests.native_fixture import native_fixture
from pathlib import Path
import importlib.util
import tempfile
import unittest

from tools.configure import configure
from tools.configure.providers import descriptor


ROOT = Path(__file__).resolve().parents[2]
DIRECTORIES = {'stm32f4': 'soc/stm32/stm32f4'}


class SourceLayoutTests(unittest.TestCase):
    def test_exact_family_descriptors_resolve_manufacturer_fact_paths(self):
        self.assertEqual({item.family for item in descriptor.families()},
                         set(DIRECTORIES))
        for family, directory in DIRECTORIES.items():
            with self.subTest(family=family):
                item = descriptor.maintained(family)
                self.assertEqual(item.source_directory, directory)
                self.assertEqual(item.fact_files[:2], (
                    directory + "/soc.json", directory + "/routes.json"))
                self.assertTrue(all((ROOT / path).is_file()
                                    for path in item.fact_files))
                if family != "native":
                    self.assertFalse((ROOT / "soc" / family).exists())

    def test_platform_generation_exports_selected_source_directory(self):
        assembly = ROOT / (
            "tools/configure/assemblies/qiming-baremetal-uart.toml")
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "generated"
            configure.configure(assembly, output, test_support=native_fixture(assembly))
            selection = (output / "selection.cmake").read_text()
            self.assertIn('set(NEXUS_SOC_FAMILY "stm32f4")', selection)
            self.assertIn(
                'set(NEXUS_SOC_SOURCE_DIRECTORY "soc/stm32/stm32f4")',
                selection)

    def test_sdk_payload_and_arch_sources_use_arm_namespace(self):
        path = ROOT / "cmake/package/package_source_sdk.py"
        spec = importlib.util.spec_from_file_location("layout_sdk", path)
        package = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(package)
        required = set(package.REQUIRED_FILES)
        self.assertIn("arch/arm/cortex_m/nx_arch_cortex_m.c", required)
        self.assertNotIn("arch/cortex_m/nx_arch_cortex_m.c", required)
        self.assertTrue((ROOT /
                         "arch/arm/cortex_m/private/compiler.h").is_file())
        self.assertFalse((ROOT / "arch/cortex_m").exists())
        self.assertTrue(all((ROOT / path).is_file() for path in required))



if __name__ == "__main__":
    unittest.main()
