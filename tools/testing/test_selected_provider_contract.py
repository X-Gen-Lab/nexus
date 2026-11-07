"""Selected provider build inputs reject before firmware targets are requested."""

from pathlib import Path
import shutil
import subprocess
import sys
import unittest

from tools.configure import test_external_package


ROOT = Path(__file__).resolve().parents[2]


class SelectedProviderContractTests(unittest.TestCase):
    def setUp(self):
        self.fixture = test_external_package.ExternalPackageTests(
            methodName="runTest")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root

    def configure_package(self, statement):
        package, assembly = self.fixture.package(
            "provider" + str(len(list(self.root.iterdir()))))
        path = package / "soc/CMakeLists.txt"
        path.write_text(statement, encoding="utf-8")
        self.fixture.reseal(package)
        process = subprocess.run([
            shutil.which("cmake"), "-S", str(ROOT), "-B",
            str(self.root / (package.name + "-build")), "-G", "Ninja",
            "-DPython3_EXECUTABLE=" + sys.executable,
            "-DNEXUS_TEST_SUPPORT=ON",
            "-DNEXUS_ASSEMBLY_FILE=" + str(assembly),
            "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF"],
            capture_output=True, text=True, timeout=90)
        return process, self.root / (package.name + "-build")

    def test_missing_selected_target_rejects_without_a_firmware_target(self):
        process, _ = self.configure_package("# No selected target.\n")
        self.assertNotEqual(process.returncode, 0,
                            "Missing selected provider target was accepted")
        self.assertIn("Selected provider target", process.stdout + process.stderr)

    def test_missing_private_include_rejects_without_a_firmware_target(self):
        process, _ = self.configure_package(
            "add_library(nexus_soc_partner_model INTERFACE)\n"
            'set(NEXUS_SYSTEM_PRIVATE_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/missing"\n'
            "    PARENT_SCOPE)\n")
        self.assertNotEqual(process.returncode, 0,
                            "Missing selected provider include was accepted")
        self.assertIn("NEXUS_SYSTEM_PRIVATE_INCLUDES",
                      process.stdout + process.stderr)

    def test_imported_and_nonlibrary_provider_targets_reject(self):
        for statement in (
                "add_library(nexus_soc_partner_model INTERFACE IMPORTED GLOBAL)\n",
                "add_custom_target(nexus_soc_partner_model)\n",
                'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.c" '
                '"int main(void) { return 0; }\\n")\n'
                'add_executable(nexus_soc_partner_model '
                '"${CMAKE_CURRENT_BINARY_DIR}/main.c")\n'):
            with self.subTest(statement=statement):
                process, _ = self.configure_package(statement)
                self.assertNotEqual(process.returncode, 0)
                self.assertIn("Selected provider target must be a local",
                              process.stdout + process.stderr)

    def test_mcu_startup_and_system_are_required_before_workload_selection(self):
        prefix = (
            "add_library(nexus_soc_partner_model INTERFACE)\n"
            "set(NEXUS_CPU_ARCH cortex-m4 PARENT_SCOPE)\n")
        for field, statement in (
                ("NEXUS_STARTUP_SOURCE", ""),
                ("NEXUS_SYSTEM_SOURCE",
                 'set(NEXUS_STARTUP_SOURCE "${CMAKE_CURRENT_LIST_FILE}" '
                 "PARENT_SCOPE)\n")):
            with self.subTest(field=field):
                process, _ = self.configure_package(prefix + statement)
                self.assertNotEqual(process.returncode, 0)
                self.assertIn("Selected provider is missing " + field,
                              process.stdout + process.stderr)

    def test_provided_startup_system_and_vendor_inputs_are_checked_for_native(self):
        for field, suffix in (
                ("NEXUS_STARTUP_SOURCE", "missing.s"),
                ("NEXUS_SYSTEM_SOURCE", "missing.c"),
                ("NEXUS_SYSTEM_VENDOR_INCLUDES", "missing")):
            with self.subTest(field=field):
                process, _ = self.configure_package(
                    "add_library(nexus_soc_partner_model INTERFACE)\n"
                    f'set({field} "${{CMAKE_CURRENT_SOURCE_DIR}}/{suffix}" '
                    "PARENT_SCOPE)\n")
                self.assertNotEqual(process.returncode, 0)
                self.assertIn("Selected provider " + field,
                              process.stdout + process.stderr)

    def test_declared_private_or_vendor_direct_exports_reject_for_mcu(self):
        for field, exported in (
                ("NEXUS_SYSTEM_PRIVATE_INCLUDES", "${CMAKE_CURRENT_SOURCE_DIR}"),
                ("NEXUS_SYSTEM_VENDOR_INCLUDES",
                 "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"),
                ("NEXUS_SYSTEM_PRIVATE_INCLUDES",
                 "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR};${CMAKE_CURRENT_SOURCE_DIR}>")):
            with self.subTest(field=field, exported=exported):
                process, _ = self.configure_package(
                    'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/provider.c" '
                    '"int provider_private_scope(void) { return 0; }\\n")\n'
                    'add_library(nexus_soc_partner_model STATIC '
                    '"${CMAKE_CURRENT_BINARY_DIR}/provider.c")\n'
                    'set(NEXUS_CPU_ARCH cortex-m4 PARENT_SCOPE)\n'
                    'set(NEXUS_STARTUP_SOURCE "${CMAKE_CURRENT_LIST_FILE}" PARENT_SCOPE)\n'
                    'set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_LIST_FILE}" PARENT_SCOPE)\n'
                    f'set({field} "${{CMAKE_CURRENT_SOURCE_DIR}}" PARENT_SCOPE)\n'
                    'target_include_directories(nexus_soc_partner_model PUBLIC '
                    f'"{exported}")\n')
                self.assertNotEqual(process.returncode, 0,
                                    "Declared private provider directory was publicly exported")
                self.assertIn("Selected provider publicly exports " + field,
                              process.stdout + process.stderr)

    def test_declared_private_inputs_stay_private_with_a_distinct_public_include(self):
        process, _ = self.configure_package(
            'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/provider.c" '
            '"int provider_private_scope(void) { return 0; }\\n")\n'
            'add_library(nexus_soc_partner_model STATIC '
            '"${CMAKE_CURRENT_BINARY_DIR}/provider.c")\n'
            'set(NEXUS_SYSTEM_PRIVATE_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)\n'
            'set(NEXUS_SYSTEM_VENDOR_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../configure" '
            'PARENT_SCOPE)\n'
            'target_include_directories(nexus_soc_partner_model '
            'PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../configure" '
            'PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")\n')
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)

    def test_local_compiled_alias_inherits_exact_selected_target_policy(self):
        process, build = self.configure_package('''\
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/provider.c"
    "int provider_contract(void) { return 0; }\\n")
add_library(external_provider STATIC "${CMAKE_CURRENT_BINARY_DIR}/provider.c")
add_library(nexus_soc_partner_model ALIAS external_provider)
set_target_properties(external_provider PROPERTIES
    C_STANDARD 99 C_EXTENSIONS ON INTERPROCEDURAL_OPTIMIZATION ON
    C_COMPILER_LAUNCHER wrong-launcher CXX_COMPILER_LAUNCHER wrong-launcher)
function(export_provider_policy)
    foreach(property C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
            INTERPROCEDURAL_OPTIMIZATION C_COMPILER_LAUNCHER
            CXX_COMPILER_LAUNCHER)
        get_target_property(value external_provider "${property}")
        file(APPEND "${CMAKE_BINARY_DIR}/provider-properties.txt"
            "${property}=${value}\\n")
    endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${NEXUS_SOURCE_DIR}" CALL export_provider_policy)
''')
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        properties = dict(line.split("=", 1) for line in
                          (build / "provider-properties.txt").read_text()
                          .splitlines())
        self.assertEqual(properties, {
            "C_STANDARD": "11", "C_STANDARD_REQUIRED": "ON",
            "C_EXTENSIONS": "OFF", "INTERPROCEDURAL_OPTIMIZATION": "OFF",
            "C_COMPILER_LAUNCHER": "", "CXX_COMPILER_LAUNCHER": ""})

    def test_local_interface_target_needs_no_native_startup_or_system(self):
        process, _ = self.configure_package(
            "add_library(external_provider INTERFACE)\n"
            "add_library(nexus_soc_partner_model ALIAS external_provider)\n")
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)


if __name__ == "__main__":
    unittest.main()
