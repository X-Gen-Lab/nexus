"""Manufacturer source layout preserves exact family and SDK boundaries."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import sys
import tempfile
import unittest

from tools.configure import configure
from tools.configure.providers import descriptor


ROOT = Path(__file__).resolve().parents[2]
DIRECTORIES = {
    "native": "soc/native",
    "stm32f103": "soc/stm32/stm32f103",
    "stm32f407": "soc/stm32/stm32f407",
    "stm32g474": "soc/stm32/stm32g474",
    "stm32h743": "soc/stm32/stm32h743",
    "stm32u575": "soc/stm32/stm32u575",
    "gd32f303": "soc/gd32/gd32f303",
    "gd32f470": "soc/gd32/gd32f470",
}


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
            configure.configure(assembly, output)
            selection = (output / "selection.cmake").read_text()
            self.assertIn('set(NEXUS_SOC_FAMILY "stm32f407")', selection)
            self.assertIn(
                'set(NEXUS_SOC_SOURCE_DIRECTORY "soc/stm32/stm32f407")',
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

    def test_legacy_device_modes_configure_shared_engines_once(self):
        for family, fixture in (("stm32f103", "f103"),
                                ("gd32f303", "gd303")):
            with self.subTest(family=family):
                with tempfile.TemporaryDirectory(
                        prefix="shared SoC engines ") as temporary:
                    directory = Path(temporary)
                    generated = directory / "generated"
                    assembly = ROOT / "tests/contracts/mainstream_assembly" / (
                        fixture + "-devices.toml")
                    configure.configure(assembly, generated)
                    target = "nexus_soc_" + family
                    source = directory / "source"
                    source.mkdir()
                    (source / "CMakeLists.txt").write_text(
                        "cmake_minimum_required(VERSION 3.31)\n"
                        "project(shared_soc_engines C)\n"
                        f'set(NEXUS_SOURCE_DIR "{ROOT}")\n'
                        f'set(NEXUS_CONFIG_DIR "{generated}")\n'
                        f'set(Python3_EXECUTABLE "{sys.executable}")\n'
                        "add_library(nexus_io INTERFACE)\n"
                        "add_library(Nexus::Arch INTERFACE IMPORTED)\n"
                        "add_library(nexus_build_options INTERFACE)\n"
                        "add_library(nexus_owned_warnings INTERFACE)\n"
                        f'add_subdirectory("{ROOT / DIRECTORIES[family]}" '
                        '"soc")\n'
                        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/sources" '
                        f'CONTENT "$<JOIN:$<TARGET_PROPERTY:{target},SOURCES>,'
                        '\\n>")\n', encoding="utf-8")
                    build = directory / "build"
                    result = subprocess.run(
                        [shutil.which("cmake") or "cmake", "-S", str(source),
                         "-B", str(build), "-G", "Ninja"],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                        text=True, check=False)
                    self.assertEqual(result.returncode, 0, result.stdout)
                    selected = [(ROOT / DIRECTORIES[family] / name).resolve()
                                for name in (build / "sources").read_text(
                                    encoding="utf-8").splitlines()]
                    for kind in ("adc", "pwm", "flash"):
                        engine = ROOT / "soc/common" / (
                            "legacy_" + kind + ".c")
                        self.assertEqual(selected.count(engine), 1,
                                         (family, kind, selected))
                    self.assertTrue(all(path.is_file() for path in selected))


if __name__ == "__main__":
    unittest.main()
