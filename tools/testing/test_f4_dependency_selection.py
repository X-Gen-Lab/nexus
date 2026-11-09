"""Selected F4 dependencies exclude inventory and dormant middleware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import unittest.mock as mock

from tools.maintenance import source_catalogue as catalogue

ROOT = Path(__file__).resolve().parents[2]


class SelectedDependencyTests(unittest.TestCase):
    def test_baremetal_f4_uses_only_its_reviewed_core_and_device_sources(self):
        records = catalogue.runtime_selection("stm32f4", backend="baremetal")
        self.assertEqual({item.identifier for item in records}, {
            "cmsis_core", "stm32f4_cmsis", "stm32f4_hal"})
        self.assertEqual(catalogue.dependency("stm32h7_hal").path,
                         "vendors/st/stm32h7_hal")

    def test_backend_and_component_dependencies_are_explicit(self):
        records = catalogue.runtime_selection("stm32f4", backend="freertos",
                                              components=("network",))
        self.assertEqual({item.identifier for item in records}, {
            "cmsis_core", "stm32f4_cmsis", "stm32f4_hal", "freertos", "lwip"})
        records = catalogue.runtime_selection("stm32f4", backend="threadx",
                                              components=("mqtt", "tls"))
        self.assertEqual({item.identifier for item in records}, {
            "cmsis_core", "stm32f4_cmsis", "stm32f4_hal", "threadx",
            "mbedtls", "coremqtt"})

    def test_invalid_production_selection_is_rejected(self):
        for kwargs in ({"family": "stm32h743", "backend": "baremetal"},
                       {"family": "stm32f4", "backend": "automatic"},
                       {"family": "stm32f4", "backend": "baremetal",
                        "components": ("guess-a-library",)}):
            with self.subTest(selection=kwargs), self.assertRaises(ValueError):
                catalogue.runtime_selection(**kwargs)


class PresetDependencySelectionTests(unittest.TestCase):
    def test_native_test_dependencies_do_not_prepare_retired_sdk_inventory(self):
        from tools.dev.dependencies import required_submodules
        selected = required_submodules("native", "native", "native")
        self.assertIn("ext/googletest", selected)
        self.assertIn("vendors/st/cmsis_device_f4", selected)
        self.assertNotIn("ext/tomlc99", selected)
        self.assertNotIn("ext/cjson", selected)
        self.assertNotIn("vendors/st/cmsis_device_h7", selected)
        self.assertNotIn("vendors/st/stm32u5_hal", selected)

    def test_arm_preparation_uses_the_selected_component_scope(self):
        from tools.dev.dependencies import required_submodules
        selected = required_submodules("arm", "stm32f4", "freertos",
                                       components=("network",))
        self.assertEqual(set(selected), {"vendors/arm/CMSIS_6",
            "vendors/st/cmsis_device_f4", "vendors/st/stm32f4_hal",
            "ext/freertos", "ext/lwip"})

    def test_sdk_host_support_does_not_prepare_the_test_framework(self):
        from tools.dev.dependencies import required_submodules
        selected = required_submodules("sdk", backend="threadx",
            components=("network", "tls", "mqtt", "mcuboot"),
            host_support=True)
        self.assertEqual(set(selected), {"vendors/arm/CMSIS_6",
            "vendors/st/cmsis_device_f4", "vendors/st/stm32f4_hal",
            "ext/threadx", "ext/lwip", "ext/mbedtls", "ext/coremqtt",
            "ext/mcuboot"})

    def test_cli_prepares_the_explicit_sdk_profile(self):
        from tools.dev import dependencies
        with mock.patch.object(dependencies, "prepare", return_value={
                "status": "verified"}) as prepare:
            self.assertEqual(dependencies.main(["prepare", "--scope", "sdk",
                "--backend", "threadx", "--host-support", "--component",
                "network", "--component", "tls"]), 0)
        prepare.assert_called_once_with(dependencies.ROOT, "sdk", family=None,
            backend="threadx", components=("network", "tls"),
            host_support=True, allow_network=False)

    def test_preset_cannot_override_its_source_profile(self):
        from tools.dev import dependencies
        with mock.patch.object(dependencies, "prepare_preset") as prepare:
            with self.assertRaises(SystemExit) as failure:
                dependencies.main(["prepare", "--scope", "preset",
                    "--preset", "native-debug", "--component", "tls"])
        self.assertEqual(failure.exception.code, 2)
        prepare.assert_not_called()

    def test_preparation_receipt_binds_the_selected_sdk_profile(self):
        from tools.dev import dependencies
        with mock.patch.object(dependencies, "_inspect_sources",
                               return_value=[]):
            receipt = dependencies.prepare(ROOT, "sdk", backend="threadx",
                components=("tls", "network"), host_support=True)
        self.assertEqual(receipt["components"], ["network", "tls"])
        self.assertTrue(receipt["host_support"])




class DormantNetworkTests(unittest.TestCase):
    def configure(self, selected, conflicting_root=False, present_supplier=False):
        with tempfile.TemporaryDirectory(prefix="f4 optional dependency ",
                                         dir="/dev/shm") as name:
            root = Path(name)
            lines = ["cmake_minimum_required(VERSION 3.31)",
                     "project(optional_network C)",
                     "function(nexus_apply_target_policy target)",
                     "endfunction()",
                     "foreach(policy nexus_abi_options nexus_build_options nexus_owned_warnings)",
                     "  add_library(${policy} INTERFACE)",
                     "endforeach()",
                     "add_library(test_io INTERFACE)",
                     "add_library(Nexus::IO ALIAS test_io)",
                     "add_library(Nexus::IOEthernetFrames ALIAS test_io)",
                     "find_package(Python3 REQUIRED COMPONENTS Interpreter)",
                     'set(NEXUS_SOURCE_DIR "' + ((root / 'wrong-root') if conflicting_root else ROOT).as_posix() + '")',
                     'add_library(nexus_config INTERFACE)',
                     'add_library(Nexus::Config ALIAS nexus_config)',
                     'set_property(TARGET nexus_config PROPERTY NEXUS_SOURCE_DIR "' + ROOT.as_posix() + '")',
                     'set(NEXUS_LWIP_SOURCE "' + ((ROOT / 'ext/lwip') if present_supplier else (root / 'absent')).as_posix() + '")',
                     'add_subdirectory("' + (ROOT / "components/network").as_posix() +
                     '" network)']
            if selected:
                lines.append("nexus_add_lwip()")
            else:
                lines += ['if(TARGET Nexus::LwIP)',
                          '  message(FATAL_ERROR "Dormant network created a target")',
                          'endif()']
            (root / "CMakeLists.txt").write_text("\n".join(lines) + "\n")
            return subprocess.run([shutil.which("cmake") or "cmake", "-S", str(root),
                                   "-B", str(root / "build")], text=True,
                                  capture_output=True, check=False)

    def test_unused_network_does_not_inspect_an_absent_supplier(self):
        result = self.configure(False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_explicit_network_still_rejects_the_absent_supplier(self):
        result = self.configure(True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("lwIP source rejected", result.stdout + result.stderr)


    def test_explicit_network_constructs_its_reviewed_public_targets(self):
        result = self.configure(True, conflicting_root=True, present_supplier=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_constructor_uses_the_selected_config_source_identity(self):
        result = self.configure(True, conflicting_root=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertRegex(result.stdout + result.stderr,
                         r"lwIP source requires regular\s+explicit directories")


class OptionalComponentSelectionTests(unittest.TestCase):
    def test_selected_component_calls_its_real_owned_constructor(self):
        constructors = (("mqtt", "nexus_add_coremqtt", "CoreMQTT"),
                        ("mcuboot", "nexus_add_mcuboot_control", "MCUBootControl"),
                        ("tls", "nexus_add_tls", "TLS"))
        for component, function, target in constructors:
            with self.subTest(component=component), tempfile.TemporaryDirectory(
                    prefix="f4 component selection ", dir="/dev/shm") as name:
                root = Path(name)
                (root / "CMakeLists.txt").write_text(
                    "cmake_minimum_required(VERSION 3.31)\n"
                    "project(selected_component NONE)\n"
                    "function(" + function + ")\n"
                    "  add_library(selected_supplier INTERFACE)\n"
                    "  add_library(Nexus::" + target + " ALIAS selected_supplier)\n"
                    "endfunction()\n"
                    'include("' + (ROOT / "cmake/platform/Components.cmake").as_posix() + '")\n'
                    "nexus_resolve_components(selected " + component + ")\n"
                    'if(NOT selected STREQUAL "Nexus::' + target + '")\n'
                    '  message(FATAL_ERROR "wrong selected component target")\n'
                    "endif()\n")
                result = subprocess.run([shutil.which("cmake") or "cmake", "-S",
                    str(root), "-B", str(root / "build")], text=True,
                    capture_output=True, check=False)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
