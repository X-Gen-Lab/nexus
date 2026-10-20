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
    # The configure-only fixture can bind an owned, regular-file source view.
    # Ordinary repository execution always uses ROOT for both fixture families.
    u575_root = ROOT

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

    def configure_u575_variant(self, variants):
        """Use real optional target definitions, with interface-only SDK ports."""
        root = self.u575_root
        copy_component = root / "components/stm32u575_lpdma_copy"
        lptim_component = root / "components/stm32u575_lpdma_lptim1"
        with tempfile.TemporaryDirectory(prefix="nexus-u575-owner-cmake-") as name:
            source = Path(name)
            (source / "main.c").write_text("int main(void) { return 0; }\n")
            libraries = " ".join("Nexus::" + variant for variant in variants)
            (source / "CMakeLists.txt").write_text(f'''\
cmake_minimum_required(VERSION 3.31)
project(u575_owner_selection LANGUAGES C)
set(NEXUS_SOURCE_DIR "{root.as_posix()}")
set(NEXUS_SOC_FAMILY stm32u575)
set(NEXUS_BACKEND baremetal)
set(NEXUS_ARCH_SECURITY_STATE 0)
set(NEXUS_EXACT_PART STM32U575ZIT6Q)
set(NEXUS_MEMORY_SRAM4 1)
function(nexus_apply_target_policy target)
endfunction()
foreach(component U575RetainedTime ClockDomain Arch IO)
    add_library(Nexus::${{component}} INTERFACE IMPORTED)
endforeach()
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
add_subdirectory("{copy_component.as_posix()}" copy_receiver)
if(EXISTS "{lptim_component.as_posix()}/CMakeLists.txt")
    add_subdirectory("{lptim_component.as_posix()}" lptim_receiver)
endif()
add_subdirectory("{(root / 'components/stm32u575_stop2').as_posix()}" owner)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE {libraries})
foreach(name nexus_stm32u575_stop2 nexus_stm32u575_stop2_copy
        nexus_stm32u575_stop2_lptim1 nexus_stm32u575_lpdma_copy
        nexus_stm32u575_lpdma_lptim1)
    if(TARGET ${{name}})
        foreach(property SOURCES COMPILE_DEFINITIONS LINK_LIBRARIES
                INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES
                EXCLUDE_FROM_ALL INTERFACE_NEXUS_U575_STOP2_MODE
                INTERFACE_NEXUS_U575_LPDMA_MODE)
            get_target_property(value ${{name}} ${{property}})
            file(APPEND "${{CMAKE_BINARY_DIR}}/properties.txt" "${{name}}.${{property}}=${{value}}\n")
        endforeach()
    endif()
endforeach()
''', encoding="utf-8")
            cmake = shutil.which("cmake") or str(ROOT / ".venv/bin/cmake")
            process = subprocess.run(
                [cmake, "-S", str(source), "-B", str(source / "build"), "-G", "Ninja"],
                capture_output=True, text=True, check=False, timeout=60)
            properties = source / "build/properties.txt"
            return process, (properties.read_text() if properties.exists() else "")

    def test_u575_copy_is_an_explicit_fixed_variant_of_the_same_owner(self):
        process, text = self.configure_u575_variant(("U575Stop2Copy",))
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        properties = dict(line.split("=", 1) for line in text.splitlines())
        original = "nexus_stm32u575_stop2."
        selected = "nexus_stm32u575_stop2_copy."
        self.assertEqual(properties[original + "SOURCES"], properties[selected + "SOURCES"])
        self.assertNotIn("NEXUS_U575_STOP2_COPY", properties[original + "COMPILE_DEFINITIONS"])
        self.assertIn("NEXUS_U575_STOP2_COPY", properties[selected + "COMPILE_DEFINITIONS"])
        self.assertNotIn("Nexus::U575LpdmaCopy", properties[original + "LINK_LIBRARIES"])
        self.assertIn("Nexus::U575LpdmaCopy", properties[selected + "LINK_LIBRARIES"])
        self.assertEqual("TRUE", properties[original + "EXCLUDE_FROM_ALL"])
        self.assertEqual("TRUE", properties[selected + "EXCLUDE_FROM_ALL"])

    def test_u575_wut_and_copy_physical_owners_cannot_be_linked_together(self):
        process, _ = self.configure_u575_variant(("U575Stop2", "U575Stop2Copy"))
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("NEXUS_U575_STOP2_MODE", process.stdout + process.stderr)

    def test_u575_lptim1_is_a_private_fixed_profile_of_the_same_owner(self):
        process, text = self.configure_u575_variant(("U575Stop2Lptim1",))
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        properties = dict(line.split("=", 1) for line in text.splitlines())
        original = "nexus_stm32u575_stop2."
        selected = "nexus_stm32u575_stop2_lptim1."
        receiver = "nexus_stm32u575_lpdma_lptim1."
        self.assertEqual(properties[original + "SOURCES"],
                         properties[selected + "SOURCES"])
        self.assertEqual(properties[selected + "SOURCES"],
                         (self.u575_root / "soc/stm32u575/stop2.c").as_posix())
        self.assertNotIn("NEXUS_U575_STOP2_LPTIM1",
                         properties[original + "COMPILE_DEFINITIONS"])
        for target in (selected, receiver):
            self.assertEqual(
                set(properties[target + "COMPILE_DEFINITIONS"].split(";")),
                {"STM32U575xx", "NEXUS_U575_STOP2_LPTIM1"})
            self.assertEqual(properties[target + "EXCLUDE_FROM_ALL"], "TRUE")
        self.assertEqual(
            set(properties[receiver + "SOURCES"].split(";")),
            {(self.u575_root / "soc/stm32u575/drivers" / unit).as_posix()
             for unit in ("lpdma_copy.c", "lpdma_lptim1.c")})
        private = (self.u575_root /
                   "components/stm32u575_lpdma_lptim1/private").as_posix()
        for target in (selected, receiver):
            self.assertIn(private,
                          properties[target + "INCLUDE_DIRECTORIES"].split(";"))
            self.assertNotIn(
                private, properties[target +
                                    "INTERFACE_INCLUDE_DIRECTORIES"].split(";"))
        self.assertTrue((self.u575_root / "soc/stm32u575/private" /
                         "stm32u575_lptim1_profile.h").is_file())
        self.assertIn("Nexus::U575LpdmaLptim1",
                      properties[selected + "LINK_LIBRARIES"].split(";"))
        self.assertNotIn("Nexus::U575LpdmaCopy",
                         properties[selected + "LINK_LIBRARIES"].split(";"))
        for target, mode in (
                (selected, "INTERFACE_NEXUS_U575_STOP2_MODE"),
                (receiver, "INTERFACE_NEXUS_U575_LPDMA_MODE")):
            self.assertEqual(properties[target + mode], "lptim1")
        self.assertEqual(properties["nexus_stm32u575_lpdma_copy." +
                                    "INTERFACE_NEXUS_U575_LPDMA_MODE"], "copy")

    def assert_u575_mode_conflict(self, variants, property_name):
        process, _ = self.configure_u575_variant(variants)
        diagnostic = " ".join((process.stdout + process.stderr).split())
        self.assertNotEqual(process.returncode, 0, diagnostic)
        self.assertIn("INTERFACE_" + property_name, diagnostic)
        self.assertIn("does not agree", diagnostic)
        self.assertNotIn("was not found", diagnostic)

    def test_u575_wut_and_lptim1_owners_reject_shared_image(self):
        self.assert_u575_mode_conflict(
            ("U575Stop2", "U575Stop2Lptim1"), "NEXUS_U575_STOP2_MODE")

    def test_u575_copy_and_lptim1_owners_reject_shared_image(self):
        self.assert_u575_mode_conflict(
            ("U575Stop2Copy", "U575Stop2Lptim1"), "NEXUS_U575_STOP2_MODE")

    def test_u575_copy_and_lptim1_receivers_cannot_be_linked_together(self):
        self.assert_u575_mode_conflict(
            ("U575LpdmaCopy", "U575LpdmaLptim1"), "NEXUS_U575_LPDMA_MODE")

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
