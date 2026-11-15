"""Execute target language policy across a real parent/child CMake boundary."""

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
PLATFORM_ROOT = ROOT
POLICY = ROOT / "cmake/platform/TargetPolicy.cmake"


class TargetLanguagePolicyTests(unittest.TestCase):
    def test_parent_usb_helpers_carry_exact_configured_target_policy(self):
        with tempfile.TemporaryDirectory(prefix="nexus usb target policy ") as value:
            directory = Path(value)
            assembly = ROOT / "tests/support/native/assemblies/native-empty.toml"
            (directory / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(parent_usb_policy C ASM)\n"
                f'set(NEXUS_ASSEMBLY_FILE "{assembly}" CACHE FILEPATH "")\n'
                'set(NEXUS_BUILD_TESTS OFF CACHE BOOL "")\n'
                'set(NEXUS_TEST_SUPPORT ON CACHE BOOL "")\n'
                'set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "")\n'
                f'add_subdirectory("{ROOT}" nexus EXCLUDE_FROM_ALL)\n'
                'get_target_property(config Nexus::Config ALIASED_TARGET)\n'
                'set_property(TARGET ${config} PROPERTY NEXUS_LTO_REQUIRED ON)\n'
                'set_property(TARGET ${config} PROPERTY '
                'NEXUS_COMPILER_LAUNCHER "/reviewed/launcher")\n'
                "set(CMAKE_C_STANDARD 99)\n"
                "set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)\n"
                "nexus_add_st_usb_host()\n"
                "nexus_add_st_usb_device()\n"
                "foreach(target IN ITEMS nexus_st_usb_host_core "
                "nexus_st_usb_host nexus_st_usb_device_core nexus_st_usb_device)\n"
                "  get_target_property(standard ${target} C_STANDARD)\n"
                "  get_target_property(lto ${target} "
                "INTERPROCEDURAL_OPTIMIZATION)\n"
                "  get_target_property(launcher ${target} C_COMPILER_LAUNCHER)\n"
                '  if(NOT standard STREQUAL "11" OR NOT lto OR '
                'NOT launcher STREQUAL "/reviewed/launcher")\n'
                '    message(FATAL_ERROR "USB helper lost target policy: '
                '${target};${standard};${lto};${launcher}")\n'
                "  endif()\n"
                "endforeach()\n", encoding="utf-8")
            environment = dict(os.environ)
            environment["PYTHONDONTWRITEBYTECODE"] = "1"
            environment["PATH"] = (str(ROOT / ".venv/bin") + os.pathsep +
                                   environment.get("PATH", ""))
            cmake = shutil.which("cmake", path=environment["PATH"])
            self.assertIsNotNone(cmake, "Prepared CMake is required")
            result = subprocess.run(
                [cmake, "-S", str(directory), "-B", str(directory / "build"),
                 "-G", "Ninja", "-DPython3_EXECUTABLE=" + sys.executable],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                env=environment, text=True, check=False, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout)

    def test_parent_firmware_is_c11_and_cpp_boundary_remains_cpp17(self):
        with tempfile.TemporaryDirectory(
                prefix="nexus parent language ") as value:
            directory = Path(value)
            source = directory / "source"
            source.mkdir()
            assembly = (PLATFORM_ROOT / "tests/support/native/assemblies"
                        / "native-empty.toml")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(parent_language C CXX ASM)\n"
                f'set(NEXUS_ASSEMBLY_FILE "{assembly}" CACHE FILEPATH "")\n'
                'set(NEXUS_BUILD_TESTS OFF CACHE BOOL "")\n'
                'set(NEXUS_TEST_SUPPORT ON CACHE BOOL "")\n'
                'set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "")\n'
                "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
                f'add_subdirectory("{PLATFORM_ROOT}" nexus EXCLUDE_FROM_ALL)\n'
                f'include("{POLICY}")\n'
                "if(DEFINED CMAKE_C_STANDARD)\n"
                '  message(FATAL_ERROR "Parent global C standard leaked")\n'
                "endif()\n"
                "nexus_add_firmware(parent_c SOURCES main.c)\n"
                "add_executable(parent_cpp boundary.cpp)\n"
                "set_target_properties(parent_cpp PROPERTIES\n"
                "  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)\n"
                "nexus_apply_target_policy(parent_cpp)\n",
                encoding="utf-8")
            (source / "main.c").write_text(
                '#include "nexus_bindings.h"\n'
                "#if !defined(__STDC_VERSION__) || "
                "__STDC_VERSION__ != 201112L\n"
                '#error "Parent firmware must compile as exact ISO C11"\n'
                "#endif\n"
                "int main(void) {\n"
                "  nx_platform_start_result_t boot = nx_platform_start();\n"
                "  if (boot.primary != NX_SUCCESS || "
                "boot.cleanup != NX_SUCCESS || boot.remaining_effects != 0U)\n"
                "    return 1;\n"
                "  return nx_platform_stop() == NX_SUCCESS ? 0 : 2;\n"
                "}\n", encoding="utf-8")
            (source / "boundary.cpp").write_text(
                "#if __cplusplus != 201703L\n"
                '#error "Host C++ boundary must remain exact C++17"\n'
                "#endif\nint main() { return 0; }\n", encoding="utf-8")
            cmake = shutil.which("cmake") or "cmake"
            environment = dict(os.environ)
            environment["PYTHONDONTWRITEBYTECODE"] = "1"
            result = subprocess.run(
                [cmake, "-S", str(source), "-B", str(directory / "build"),
                 "-G", "Ninja", "-DPython3_EXECUTABLE=" + sys.executable],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                env=environment, text=True, check=False, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout)
            result = subprocess.run(
                [cmake, "--build", str(directory / "build"), "--target",
                 "parent_c", "parent_cpp", "--parallel", "1", "--verbose"],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                env=environment, text=True, check=False, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout)
            entries = json.loads(
                (directory / "build/compile_commands.json").read_text())
            for name, standard in [("main.c", "-std=c11"),
                                   ("boundary.cpp", "-std=c++17")]:
                selected = [entry for entry in entries
                            if Path(entry["file"]).name == name]
                self.assertEqual(len(selected), 1)
                self.assertIn(standard, shlex.split(selected[0]["command"]))
            for name in ("parent_c", "parent_cpp"):
                result = subprocess.run(
                    [str(directory / "build/bin" / name) if name == "parent_c"
                     else str(directory / "build" / name)],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    text=True, check=False, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
