"""Inspect real explicit sleep targets from an external CMake caller scope.

This configure-only contract requires host CMake and the maintained pinned SDK
inputs. It compiles neither placeholder SDK code nor ARM firmware. Final ARM
link/resource evidence is maintained separately from these target properties.
"""
from tools.configure.providers.descriptor import source_directory

import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class SocSleepSelectionTests(unittest.TestCase):
    # The configure-only fixture can bind an owned, regular-file source view.
    # Ordinary repository execution uses the selected F4 platform identity.

    def configure_properties(self, family, selected, *, lto=True,
                             launcher="scope-launcher;--contract",
                             platform_family="stm32f4", device="STM32F407xx",
                             rejected=False):
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
set(NEXUS_STM32_DEVICE {device})
set(NEXUS_STM32_STARTUP startup_stm32f407xx.s)
set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")
include("{(ROOT / 'cmake/platform/TargetPolicy.cmake').as_posix()}")
add_library(Nexus::Config ALIAS nexus_build_options)
set_target_properties(nexus_build_options PROPERTIES
    NEXUS_LTO_REQUIRED {"ON" if lto else "OFF"}
    NEXUS_COMPILER_LAUNCHER "{launcher}")
add_subdirectory("{(ROOT / source_directory(family)).as_posix()}" provider)
''', encoding="utf-8")
            selection = (
                "nexus_add_stm32f4_sleep(selected_sleep)"
                if selected else "")
            platform = ("" if platform_family is None else
                "add_library(nexus_platform INTERFACE)\n"
                "add_library(Nexus::Platform ALIAS nexus_platform)\n"
                'set_target_properties(nexus_platform PROPERTIES '
                f'NEXUS_SOC_FAMILY "{platform_family}" '
                f'NEXUS_STM32_DEVICE "{device}")\n')
            (source / "CMakeLists.txt").write_text(f'''\
cmake_minimum_required(VERSION 3.31)
project(sleep_external_scope_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(nexus_io INTERFACE)
add_library(nexus_arch INTERFACE)
add_library(Nexus::Arch ALIAS nexus_arch)
add_library(nexus_clock_domain INTERFACE)
add_library(Nexus::ClockDomain ALIAS nexus_clock_domain)
add_library(nexus_rtc INTERFACE)
add_library(Nexus::IORTC ALIAS nexus_rtc)
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION {"OFF" if lto else "ON"})
set(CMAKE_C_COMPILER_LAUNCHER parent-launcher)
set(CMAKE_CXX_COMPILER_LAUNCHER parent-cxx-launcher)
if(DEFINED NEXUS_SOURCE_DIR)
    message(FATAL_ERROR "External caller unexpectedly has SDK variable")
endif()
add_subdirectory(sdk-port)
{platform}
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
            if(unit MATCHES "(^|/)(sleep|power)[.]c$")
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
            if rejected:
                return process
            self.assertEqual(process.returncode, 0,
                             process.stdout + process.stderr)
            properties = (source / "build" / "properties.txt").read_text(
                encoding="utf-8")
            result = dict(line.split("=", 1)
                          for line in properties.splitlines())
            if selected:
                database = json.loads(
                    (source / "build/compile_commands.json").read_text())
                units = [unit for unit in database
                         if Path(unit["file"]) ==
                         ROOT / source_directory(family) / "sleep.c"]
                self.assertEqual(len(units), 1)
                result["compile_arguments"] = (
                    units[0].get("arguments") or
                    shlex.split(units[0]["command"]))
            return result

    def assert_selected_private_imports(self, family):
        properties = self.configure_properties(family, True)
        self.assertEqual(properties["selected"], "TRUE")
        provider = ROOT / source_directory(family)
        self.assertEqual(properties["SOURCES"],
                         ";".join((provider / unit).as_posix()
                                  for unit in ("sleep.c", "power.c")))
        self.assertEqual(properties["sleep_sources"],
                         properties["SOURCES"])
        private_imports = {
            (provider / "private").as_posix(),
            (ROOT / "components/clock_domain/private").as_posix(),
        }
        public_imports = {(provider / "include").as_posix()}
        sdk_imports = {
            (ROOT / "vendors/st/cmsis_device_f4/Include").as_posix(),
        }
        definitions = {"STM32F407xx"}
        self.assertEqual(set(properties["INCLUDE_DIRECTORIES"].split(";")),
                         private_imports | public_imports | sdk_imports)
        self.assertEqual(
            set(properties["INTERFACE_INCLUDE_DIRECTORIES"].split(";")),
            public_imports)
        self.assertEqual(set(properties["COMPILE_DEFINITIONS"].split(";")),
                         definitions)
        self.assertEqual(set(properties["LINK_LIBRARIES"].split(";")), {
            "Nexus::ClockDomain", "Nexus::IORTC", "Nexus::Arch",
            "nexus_build_options",
            "nexus_owned_warnings", "nexus_cmsis_core",
        })
        arguments = properties["compile_arguments"]
        self.assertIn("-DCMSIS_DISABLE_DEPRECATED=1", arguments)
        includes = set()
        for index, argument in enumerate(arguments):
            if argument in {"-I", "-isystem"}:
                includes.add(arguments[index + 1])
            elif argument.startswith("-I"):
                includes.add(argument[2:])
        core = (ROOT / "vendors/arm/CMSIS_6/CMSIS/Core/Include").as_posix()
        self.assertEqual(
            includes, private_imports | public_imports | sdk_imports | {core})
        self.assertEqual(properties["EXCLUDE_FROM_ALL"], "TRUE")
        self.assertEqual(properties["INTERPROCEDURAL_OPTIMIZATION"], "ON")
        self.assertEqual(properties["C_COMPILER_LAUNCHER"],
                         "scope-launcher;--contract")
        self.assertEqual(properties["CXX_COMPILER_LAUNCHER"],
                         "scope-launcher;--contract")
        for path in private_imports | sdk_imports:
            self.assertTrue(Path(path).is_dir(), path)









    def test_sleep_rejects_missing_selected_platform(self):
        process = self.configure_properties("stm32f4", True,
                                            platform_family=None, rejected=True)
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("selected F4 Platform", process.stdout + process.stderr)

    def test_sleep_rejects_non_f4_platform_identity(self):
        for family in ("native", "foreign"):
            with self.subTest(family=family):
                process = self.configure_properties("stm32f4", True,
                    platform_family=family, rejected=True)
                self.assertNotEqual(process.returncode, 0)
                self.assertIn("selected F4 Platform", process.stdout + process.stderr)

    def test_stm32_external_selection_uses_private_checked_sdk_paths(self):
        self.assert_selected_private_imports("stm32f4")


    def test_new_parts_select_the_same_power_sources_with_exact_device_macros(self):
        for device in ("STM32F411xE", "STM32F446xx", "STM32F479xx"):
            with self.subTest(device=device):
                properties = self.configure_properties("stm32f4", True,
                                                       device=device)
                self.assertEqual(properties["COMPILE_DEFINITIONS"], device)
                self.assertIn("power.c", properties["SOURCES"])
                self.assertIn("-D" + device, properties["compile_arguments"])

    def test_stm32_default_declares_no_automatic_sleep_target(self):
        properties = self.configure_properties("stm32f4", False)
        self.assertEqual(properties, {"sleep_sources": "", "selected": "FALSE"})


    def test_stm32_selected_policy_withdraws_parent_ipo_and_launcher(self):
        properties = self.configure_properties("stm32f4", True,
                                               lto=False, launcher="")
        self.assertEqual(properties["INTERPROCEDURAL_OPTIMIZATION"], "OFF")
        self.assertEqual(properties["C_COMPILER_LAUNCHER"], "")
        self.assertEqual(properties["CXX_COMPILER_LAUNCHER"], "")



if __name__ == "__main__":
    unittest.main()
