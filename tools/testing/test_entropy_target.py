"""Execute explicit F407 entropy target admission and its source closure."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "soc/stm32/stm32f4/Entropy.cmake"
PLATFORM_ROOT = ROOT


class EntropyTargetTests(unittest.TestCase):
    def configure(self, body, family="stm32f4"):
        with tempfile.TemporaryDirectory(prefix="entropy target ") as value:
            directory = Path(value)
            source = directory / "source"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(entropy_target C)\n"
                "add_library(platform INTERFACE)\n"
                "add_library(Nexus::Platform ALIAS platform)\n"
                f'set_property(TARGET platform PROPERTY NEXUS_SOC_FAMILY '
                f'"{family}")\n'
                'set_property(TARGET platform PROPERTY NEXUS_STM32_DEVICE '
                '"STM32F407xx")\n'
                f'set_property(TARGET platform PROPERTY NEXUS_SOURCE_DIR '
                f'"{PLATFORM_ROOT}")\n'
                "foreach(name nexus_io nexus_arch nexus_build_options "
                "nexus_owned_warnings)\n"
                "  add_library(${name} INTERFACE)\n"
                "endforeach()\n"
                "add_library(Nexus::IO ALIAS nexus_io)\n"
                "add_library(Nexus::Arch ALIAS nexus_arch)\n"
                "function(nexus_apply_target_policy target)\n"
                "  set_property(TARGET ${target} PROPERTY "
                "NEXUS_TEST_POLICY_APPLIED TRUE)\n"
                "endfunction()\n"
                "function(nexus_use_cmsis_core target)\n"
                "endfunction()\n"
                f'include("{HELPER}")\n' + body,
                encoding="utf-8")
            return subprocess.run(
                [shutil.which("cmake") or "cmake", "-S", str(source),
                 "-B", str(directory / "build"), "-G", "Ninja"],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, check=False, timeout=30)

    def test_empty_platform_resolves_all_explicit_target_dependencies(self):
        compiler = os.environ.get("NEXUS_ARM_GCC") or shutil.which(
            "arm-none-eabi-gcc")
        self.assertIsNotNone(compiler, "Prepared ARM compiler required")
        with tempfile.TemporaryDirectory(
                prefix="empty entropy platform ") as value:
            directory = Path(value)
            source = directory / "source"
            source.mkdir()
            assembly = (PLATFORM_ROOT / "tools/configure/assemblies"
                        / "qiming-baremetal-empty.toml")
            include_dir = ROOT / "soc/stm32/stm32f4/include"
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(empty_entropy_platform C ASM)\n"
                f'set(NEXUS_ASSEMBLY_FILE "{assembly}" CACHE FILEPATH "")\n'
                "set(NEXUS_BUILD_TESTS OFF CACHE BOOL \"\")\n"
                "set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL \"\")\n"
                f'add_subdirectory("{PLATFORM_ROOT}" nexus EXCLUDE_FROM_ALL)\n'
                f'include("{HELPER}")\n'
                "get_target_property(configuration Nexus::Platform NEXUS_CONFIG_DIR)\n"
                'file(READ "${configuration}/resolved.json" resolved)\n'
                'string(JSON controller_count LENGTH "${resolved}" controllers)\n'
                "if(NOT controller_count EQUAL 0)\n"
                '  message(FATAL_ERROR "Fixture unexpectedly selects I/O")\n'
                "endif()\n"
                "nexus_add_stm32f407_entropy(entropy)\n"
                "get_target_property(excluded entropy EXCLUDE_FROM_ALL)\n"
                "get_target_property(includes entropy "
                "INTERFACE_INCLUDE_DIRECTORIES)\n"
                "if(NOT excluded)\n"
                '  message(FATAL_ERROR "Entropy is no longer optional")\n'
                "endif()\n"
                f'if(NOT includes STREQUAL "{include_dir}")\n'
                '  message(FATAL_ERROR "Private or vendor headers exported")\n'
                "endif()\n", encoding="utf-8")
            environment = dict(os.environ)
            environment["PATH"] = str(Path(compiler).parent) + os.pathsep + (
                environment.get("PATH", ""))
            result = subprocess.run(
                [shutil.which("cmake") or "cmake", "-S", str(source),
                 "-B", str(directory / "build"), "-G", "Ninja",
                 "-DCMAKE_TOOLCHAIN_FILE=" + str(
                     PLATFORM_ROOT / "cmake/toolchains/arm-gcc.cmake"),
                 "-DPython3_EXECUTABLE=" + sys.executable],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                env=environment, text=True, check=False, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout)

    def test_second_target_for_same_hardware_is_rejected(self):
        result = self.configure(
            "nexus_add_stm32f407_entropy(first)\n"
            "nexus_add_stm32f407_entropy(second)\n")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("one explicit F407 entropy target", result.stdout)

    def test_other_family_is_not_silently_selected(self):
        result = self.configure(
            "nexus_add_stm32f407_entropy(entropy)\n", family="native")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("requires an STM32F407 Platform", result.stdout)

    def test_extra_arguments_are_rejected(self):
        result = self.configure(
            "nexus_add_stm32f407_entropy(entropy unexpected)\n")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("requires one unused target name", result.stdout)

    def test_unselected_helper_adds_no_target(self):
        result = self.configure(
            "get_property(selected GLOBAL PROPERTY "
            "NEXUS_STM32F407_ENTROPY_TARGET)\n"
            "if(selected)\n"
            '  message(FATAL_ERROR "Unselected entropy target exists")\n'
            "endif()\n")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_official_rng_inline_headers_close_the_packaged_source_graph(self):
        spec = importlib.util.spec_from_file_location(
            "entropy_f4_lock", ROOT / "tools/maintenance/stm32f4_hal_lock.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        required = {f"Inc/stm32f4xx_ll_{kind}.h"
                    for kind in ("bus", "rcc", "rng")}
        self.assertLessEqual(required, module.REVIEWED_FILES)
        lock = json.loads((ROOT / "dependencies/stm32f4-hal.lock.json")
                          .read_text())
        self.assertLessEqual(required, module.contract(lock))


if __name__ == "__main__":
    unittest.main()
