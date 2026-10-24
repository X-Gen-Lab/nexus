"""Exact U575 SRAM4 selection retains separate placement and DMA facts."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from tools.configure import configure as gate, emission


class U575Sram4ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def configure(self, region=""):
        assembly = self.root / "assembly.toml"
        assembly.write_text(
            'schema=2\nboard="stm32u575_nucleo_zi_q"\n'
            'backend="baremetal"\nclock="msis4"\n'
            '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
            'static_ram_limit_bytes=4096\n' + region)
        output = self.root / "bundle"
        return gate.configure(assembly, output), output

    def test_default_keeps_sram1_and_rejects_unselected_sram4_sections(self):
        result, output = self.configure()
        regions = {item["id"]: item for item in result["memory"]}
        self.assertEqual([item["id"] for item in result["memory"]
                          if item["linker"]], ["sram1"])
        self.assertEqual(regions["sram4"]["origin"], 0x28000000)
        self.assertEqual(regions["sram4"]["size"], 16384)
        self.assertFalse(regions["sram4"]["linker"])
        self.assertEqual(regions["sram4"]["dma_engines"],
                         {"GPDMA1": True, "LPDMA1": True})
        self.assertIn(".nx_sram4_unselected", (output / "memory.ld").read_text())
        self.assertIn("#define NEXUS_MEMORY_SRAM4 0",
                      (output / "nexus_config.h").read_text())

    def test_selected_sram4_has_exact_linker_symbols_and_independent_budget(self):
        result, output = self.configure(
            '[memory.regions.sram4]\nlimit_bytes=256\n')
        regions = {item["id"]: item for item in result["memory"]}
        self.assertTrue(regions["sram1"]["linker"])
        self.assertTrue(regions["sram4"]["linker"])
        budget = json.loads((output / "resource-budget.json").read_text())
        self.assertEqual(budget["ram_reserved_max"],
                         {"sram1": 4096, "sram4": 256})
        self.assertIn("SRAM4 (rw) : ORIGIN = 0x28000000, LENGTH = 16384",
                      (output / "memory.ld").read_text())
        for suffix in ("region_start", "region_end", "data_load",
                       "data_start", "data_end", "bss_start", "bss_end",
                       "noinit_start", "noinit_end"):
            self.assertIn("__nx_sram4_" + suffix,
                          (output / "memory.ld").read_text())
        self.assertIn("#define NEXUS_MEMORY_SRAM4 1",
                      (output / "nexus_config.h").read_text())
        self.assertIn("set(NEXUS_MEMORY_SRAM4 1)",
                      (output / "selection.cmake").read_text())
        declarations, count = emission.dma_regions("copy", result,
                                                    engine="LPDMA1")
        self.assertEqual(count, 1)
        self.assertIn("0x28000000u, 16384u", "\n".join(declarations))
        self.assertNotIn("0x20000000", "\n".join(declarations))

    def test_sram4_rejects_invalid_limits_without_selecting_another_region(self):
        for value in ("0", "true", "3", "16388"):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "region"):
                    self.configure('[memory.regions.sram4]\n'
                                   f'limit_bytes={value}\n')

    def test_cmake_resolver_exports_sram4_selection_to_its_caller(self):
        cmake = (shutil.which("cmake") or
                 shutil.which("cmake", path=str(Path(sys.executable).parent)))
        self.assertIsNotNone(cmake, "Actual CMake is required")
        for selected in (0, 1):
            with self.subTest(selected=selected):
                self.configure('[memory.regions.sram4]\nlimit_bytes=512\n'
                               if selected else "")
                script = self.root / "resolve.cmake"
                script.write_text(
                    'cmake_minimum_required(VERSION 3.24)\n'
                    f'set(Python3_EXECUTABLE "{sys.executable}")\n'
                    f'set(NEXUS_SOURCE_DIR "{gate.ROOT}")\n'
                    f'set(NEXUS_BINARY_DIR "{self.root / "cmake-output"}")\n'
                    f'set(NEXUS_ASSEMBLY_FILE "{self.root / "assembly.toml"}")\n'
                    f'include("{gate.ROOT / "cmake/platform/Configure.cmake"}")\n'
                    'nexus_resolve_assembly()\n'
                    'if(NOT DEFINED NEXUS_MEMORY_SRAM4)\n'
                    '  message(FATAL_ERROR "SRAM4 selection missing in caller scope")\n'
                    'endif()\n'
                    f'if(NOT "${{NEXUS_MEMORY_SRAM4}}" STREQUAL "{selected}")\n'
                    '  message(FATAL_ERROR "SRAM4 selection differs in caller scope")\n'
                    'endif()\n'
                    'if(NOT NEXUS_MEMORY_SRAM1 EQUAL 1)\n'
                    '  message(FATAL_ERROR "Default SRAM1 selection changed")\n'
                    'endif()\n')
                result = subprocess.run([cmake, "-P", str(script)],
                                        capture_output=True, text=True,
                                        check=False, timeout=30)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)

    def test_soc_exports_selected_startup_definition_and_initializer_together(self):
        cmake = (shutil.which("cmake") or
                 shutil.which("cmake", path=str(Path(sys.executable).parent)))
        self.assertIsNotNone(cmake, "Actual CMake is required")
        for selected in (0, 1):
            with self.subTest(selected=selected):
                _, output = self.configure(
                    '[memory.regions.sram4]\nlimit_bytes=512\n'
                    if selected else "")
                project = self.root / ("startup-" + str(selected))
                project.mkdir()
                (project / "CMakeLists.txt").write_text(
                    'cmake_minimum_required(VERSION 3.24)\n'
                    'project(sram4_startup_contract LANGUAGES NONE)\n'
                    f'set(Python3_EXECUTABLE "{sys.executable}")\n'
                    f'set(NEXUS_SOURCE_DIR "{gate.ROOT}")\n'
                    f'set(NEXUS_CONFIG_DIR "{output}")\n'
                    f'set(NEXUS_MEMORY_SRAM4 {selected})\n'
                    f'add_subdirectory("{gate.ROOT / "soc/stm32/stm32u575"}" soc)\n'
                    'if(NOT "STM32U575xx" IN_LIST NEXUS_SYSTEM_COMPILE_DEFINITIONS)\n'
                    '  message(FATAL_ERROR "Exact startup part define missing")\n'
                    'endif()\n'
                    'set(actual 0)\n'
                    'if("NEXUS_MEMORY_SRAM4=1" IN_LIST NEXUS_SYSTEM_COMPILE_DEFINITIONS)\n'
                    '  set(actual 1)\n'
                    'endif()\n'
                    f'if(NOT actual EQUAL {selected})\n'
                    '  message(FATAL_ERROR "Selected startup C macro missing or leaked")\n'
                    'endif()\n'
                    'set(initializer 0)\n'
                    'foreach(source IN LISTS NEXUS_SYSTEM_SOURCE)\n'
                    '  if(source MATCHES "/memory_init[.]c$")\n'
                    '    set(initializer 1)\n'
                    '  endif()\n'
                    'endforeach()\n'
                    f'if(NOT initializer EQUAL {selected})\n'
                    '  message(FATAL_ERROR "Selected initializer source missing or leaked")\n'
                    'endif()\n')
                result = subprocess.run(
                    [cmake, "-S", str(project), "-B", str(project / "build")],
                    capture_output=True, text=True, check=False, timeout=30)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)

    def compiler(self):
        compiler = shutil.which("gcc")
        self.assertIsNotNone(compiler, "Actual GNU compiler is required")
        return compiler

    def source(self):
        source = self.root / "image.c"
        source.write_text(
            '#include "nexus/core/memory.h"\n'
            'void Reset_Handler(void) { for (;;) {} }\n'
            'NX_MEMORY_SRAM4_DATA unsigned char sram4_data = 0x5a;\n'
            'NX_MEMORY_SRAM4_BSS unsigned char sram4_bss[3];\n'
            'NX_MEMORY_SRAM4_NOINIT unsigned char sram4_noinit[3];\n')
        return source

    def compile(self, output):
        return subprocess.run(
            [self.compiler(), "-std=c11", "-Wall", "-Wextra", "-Werror",
             "-ffunction-sections", "-fdata-sections", "-fno-common",
             "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
             "-fno-pie", "-no-pie", "-nostdlib", "-Wl,--build-id=none",
             "-I", str(output), "-I", str(gate.ROOT / "core/include"),
             "-T", str(output / "memory.ld"), str(self.source()),
             "-o", str(self.root / "image.elf")],
            capture_output=True, text=True, check=False, timeout=30)

    def test_selected_public_attributes_link_to_separate_sram4_sections(self):
        _, output = self.configure('[memory.regions.sram4]\nlimit_bytes=256\n')
        result = self.compile(output)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        compiler = self.compiler()
        tool = subprocess.run([compiler, "-print-prog-name=nm"],
                              capture_output=True, text=True, check=True,
                              timeout=30).stdout.strip()
        result = subprocess.run([tool, "-P", str(self.root / "image.elf")],
                                capture_output=True, text=True, check=True,
                                timeout=30)
        symbols = {entry[0]: int(entry[2], 16)
                   for line in result.stdout.splitlines()
                   if len(entry := line.split()) >= 3}
        self.assertEqual(symbols["sram4_data"], 0x28000000)
        self.assertEqual(symbols["sram4_bss"], 0x28000004)
        self.assertEqual(symbols["sram4_noinit"], 0x28000008)
        self.assertEqual(symbols["__nx_sram4_noinit_end"], 0x2800000c)
        self.assertEqual(symbols["_estack"], 0x20030000)
        self.assertGreaterEqual(symbols["__nx_sram4_data_load"], 0x08000000)
        self.assertLess(symbols["__nx_sram4_data_load"], 0x08200000)

    def test_unselected_public_sram4_attributes_are_a_compile_error(self):
        _, output = self.configure()
        result = self.compile(output)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("SRAM4 memory region is not selected", result.stderr)


if __name__ == "__main__":
    unittest.main()
