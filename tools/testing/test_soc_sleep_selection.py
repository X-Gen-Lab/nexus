"""Inspect real explicit sleep targets from an external CMake caller scope.

This configure-only contract requires host CMake and the maintained pinned SDK
inputs. It compiles neither placeholder SDK code nor ARM firmware. Final ARM
link/resource evidence is maintained separately from these target properties.
"""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class SocSleepSelectionTests(unittest.TestCase):
    def configure_properties(self, family, selected, *, lto=True,
                             launcher="scope-launcher;--contract"):
        """Keep the checked SDK variable inside the port's child scope only."""
        with tempfile.TemporaryDirectory(prefix="nexus-sleep-cmake-") as name:
            source = Path(name)
            port = source / "sdk-port"
            port.mkdir()
            generated = source / "generated"
            generated.mkdir()
            (generated / "resolved.json").write_text(
                json.dumps({"controllers": []}), encoding="utf-8")
            (port / "CMakeLists.txt").write_text(f'''\
set(NEXUS_SOURCE_DIR "{ROOT.as_posix()}")
set(NEXUS_CONFIG_DIR "{generated.as_posix()}")
set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")
include("{(ROOT / 'cmake/platform/TargetPolicy.cmake').as_posix()}")
add_library(Nexus::Config ALIAS nexus_build_options)
set_target_properties(nexus_build_options PROPERTIES
    NEXUS_LTO_REQUIRED {"ON" if lto else "OFF"}
    NEXUS_COMPILER_LAUNCHER "{launcher}")
add_subdirectory("{(ROOT / 'soc' / family).as_posix()}" provider)
''', encoding="utf-8")
            selection = (
                f"nexus_add_{family}_sleep(selected_sleep)"
                if selected else "")
            (source / "CMakeLists.txt").write_text(f'''\
cmake_minimum_required(VERSION 3.31)
project(sleep_external_scope_contract LANGUAGES C ASM)
add_library(nexus_io INTERFACE)
add_library(nexus_arch INTERFACE)
add_library(Nexus::Arch ALIAS nexus_arch)
add_library(nexus_clock_domain INTERFACE)
add_library(Nexus::ClockDomain ALIAS nexus_clock_domain)
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION {"OFF" if lto else "ON"})
set(CMAKE_C_COMPILER_LAUNCHER parent-launcher)
set(CMAKE_CXX_COMPILER_LAUNCHER parent-cxx-launcher)
if(DEFINED NEXUS_SOURCE_DIR)
    message(FATAL_ERROR "External caller unexpectedly has SDK variable")
endif()
add_subdirectory(sdk-port)
if(DEFINED NEXUS_SOURCE_DIR)
    message(FATAL_ERROR "Child SDK variable leaked into external caller")
endif()
{selection}
function(collect_sleep_sources directory result)
    get_property(targets DIRECTORY "${{directory}}" PROPERTY BUILDSYSTEM_TARGETS)
    set(sources)
    foreach(target IN LISTS targets)
        get_target_property(units "${{target}}" SOURCES)
        foreach(unit IN LISTS units)
            if(unit MATCHES "(^|/)sleep[.]c$")
                list(APPEND sources "${{unit}}")
            endif()
        endforeach()
    endforeach()
    get_property(children DIRECTORY "${{directory}}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        collect_sleep_sources("${{child}}" child_sources)
        list(APPEND sources ${{child_sources}})
    endforeach()
    set(${{result}} "${{sources}}" PARENT_SCOPE)
endfunction()
collect_sleep_sources("${{CMAKE_CURRENT_SOURCE_DIR}}" sleep_sources)
file(WRITE "${{CMAKE_BINARY_DIR}}/properties.txt" "sleep_sources=${{sleep_sources}}\n")
if(TARGET selected_sleep)
    file(APPEND "${{CMAKE_BINARY_DIR}}/properties.txt" "selected=TRUE\n")
    foreach(property SOURCES INCLUDE_DIRECTORIES
            INTERFACE_INCLUDE_DIRECTORIES COMPILE_DEFINITIONS LINK_LIBRARIES
            EXCLUDE_FROM_ALL INTERPROCEDURAL_OPTIMIZATION
            C_COMPILER_LAUNCHER CXX_COMPILER_LAUNCHER)
        get_target_property(value selected_sleep "${{property}}")
        file(APPEND "${{CMAKE_BINARY_DIR}}/properties.txt" "${{property}}=${{value}}\n")
    endforeach()
else()
    file(APPEND "${{CMAKE_BINARY_DIR}}/properties.txt" "selected=FALSE\n")
endif()
''', encoding="utf-8")
            cmake = shutil.which("cmake")
            if cmake is None:
                cmake = str(ROOT / ".venv" / "bin" / "cmake")
            process = subprocess.run(
                [cmake, "-S", str(source), "-B", str(source / "build"),
                 "-G", "Ninja"],
                capture_output=True, text=True, check=False, timeout=60,
            )
            self.assertEqual(process.returncode, 0,
                             process.stdout + process.stderr)
            properties = (source / "build" / "properties.txt").read_text(
                encoding="utf-8")
            return dict(line.split("=", 1)
                        for line in properties.splitlines())

    def assert_selected_private_imports(self, family):
        properties = self.configure_properties(family, True)
        self.assertEqual(properties["selected"], "TRUE")
        provider = ROOT / "soc" / family
        self.assertEqual(properties["SOURCES"],
                         (provider / "sleep.c").as_posix())
        self.assertEqual(properties["sleep_sources"],
                         properties["SOURCES"])
        private_imports = {
            (provider / "private").as_posix(),
            (ROOT / "components/clock_domain/private").as_posix(),
        }
        if family == "stm32f407":
            sdk_imports = {
                (ROOT / "vendors/st/cmsis_device_f4/Include").as_posix(),
                (ROOT / "vendors/arm/CMSIS_5/CMSIS/Core/Include").as_posix(),
            }
            definitions = {"STM32F407xx"}
        else:
            firmware = ROOT / "vendors/gigadevice/gd32f4xx/Firmware"
            sdk_imports = {
                (firmware / "CMSIS").as_posix(),
                (firmware / "CMSIS/GD/GD32F4xx/Include").as_posix(),
                (firmware / "GD32F4xx_standard_peripheral/Include").as_posix(),
            }
            definitions = {"GD32F470", "HXTAL_VALUE=25000000"}
        self.assertEqual(set(properties["INCLUDE_DIRECTORIES"].split(";")),
                         private_imports | sdk_imports)
        self.assertEqual(properties["INTERFACE_INCLUDE_DIRECTORIES"],
                         "value-NOTFOUND")
        self.assertEqual(set(properties["COMPILE_DEFINITIONS"].split(";")),
                         definitions)
        self.assertEqual(set(properties["LINK_LIBRARIES"].split(";")), {
            "Nexus::ClockDomain", "Nexus::Arch", "nexus_build_options",
            "nexus_owned_warnings",
        })
        self.assertEqual(properties["EXCLUDE_FROM_ALL"], "TRUE")
        self.assertEqual(properties["INTERPROCEDURAL_OPTIMIZATION"], "ON")
        self.assertEqual(properties["C_COMPILER_LAUNCHER"],
                         "scope-launcher;--contract")
        self.assertEqual(properties["CXX_COMPILER_LAUNCHER"],
                         "scope-launcher;--contract")
        for path in private_imports | sdk_imports:
            self.assertTrue(Path(path).is_dir(), path)

    def test_stm32_external_selection_uses_private_checked_sdk_paths(self):
        self.assert_selected_private_imports("stm32f407")

    def test_gd32_external_selection_uses_private_checked_sdk_paths(self):
        self.assert_selected_private_imports("gd32f470")

    def test_stm32_default_declares_no_automatic_sleep_target(self):
        properties = self.configure_properties("stm32f407", False)
        self.assertEqual(properties, {"sleep_sources": "", "selected": "FALSE"})

    def test_gd32_default_declares_no_automatic_sleep_target(self):
        properties = self.configure_properties("gd32f470", False)
        self.assertEqual(properties, {"sleep_sources": "", "selected": "FALSE"})

    def test_stm32_selected_policy_withdraws_parent_ipo_and_launcher(self):
        properties = self.configure_properties("stm32f407", True,
                                               lto=False, launcher="")
        self.assertEqual(properties["INTERPROCEDURAL_OPTIMIZATION"], "OFF")
        self.assertEqual(properties["C_COMPILER_LAUNCHER"], "")
        self.assertEqual(properties["CXX_COMPILER_LAUNCHER"], "")

    def test_gd32_selected_policy_withdraws_parent_ipo_and_launcher(self):
        properties = self.configure_properties("gd32f470", True,
                                               lto=False, launcher="")
        self.assertEqual(properties["INTERPROCEDURAL_OPTIMIZATION"], "OFF")
        self.assertEqual(properties["C_COMPILER_LAUNCHER"], "")
        self.assertEqual(properties["CXX_COMPILER_LAUNCHER"], "")


if __name__ == "__main__":
    unittest.main()
