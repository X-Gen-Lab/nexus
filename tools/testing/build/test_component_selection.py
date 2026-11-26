"""Execute explicit component target selection and rejection through CMake."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure.contracts import COMPONENTS


ROOT = Path(__file__).resolve().parents[3]


class ComponentSelectionTests(unittest.TestCase):
    def configure(self, body):
        with tempfile.TemporaryDirectory(
                prefix="component selection ") as value:
            directory = Path(value)
            source = directory / "source"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(component_selection NONE)\n"
                f'include("{ROOT / "cmake/platform/Components.cmake"}")\n'
                + body,
                encoding="utf-8")
            return subprocess.run(
                [shutil.which("cmake") or "cmake", "-S", str(source),
                 "-B", str(directory / "build"), "-G", "Ninja"],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, check=False)

    def test_mapping_covers_exact_authored_choices_and_preserves_order(self):
        expected = ";".join(sorted(COMPONENTS))
        result = self.configure(
            "nexus_component_choices(choices)\n"
            "list(SORT choices)\n"
            f'if(NOT choices STREQUAL "{expected}")\n'
            '  message(FATAL_ERROR "Authored component choices drifted")\n'
            "endif()\n"
            "add_library(Nexus::BusOwner INTERFACE IMPORTED)\n"
            "add_library(Nexus::Log INTERFACE IMPORTED)\n"
            "nexus_resolve_components(selected log bus-owner)\n"
            'if(NOT selected STREQUAL "Nexus::Log;Nexus::BusOwner")\n'
            '  message(FATAL_ERROR '
            '"Selection order or target identity differs")\n'
            "endif()\n"
            "nexus_resolve_components(empty)\n"
            'if(NOT empty STREQUAL "")\n'
            '  message(FATAL_ERROR "Empty selection added a component")\n'
            "endif()\n")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_unknown_component_rejects_without_guessing_a_target(self):
        result = self.configure("nexus_resolve_components(selected unknown)\n")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("No owned target for component unknown", result.stdout)

    def test_missing_selected_target_rejects_before_linking(self):
        result = self.configure("nexus_resolve_components(selected log)\n")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Selected component target is unavailable: log",
                      result.stdout)

    def test_new_catalogue_entry_loads_its_leaf_without_central_dispatch(self):
        with tempfile.TemporaryDirectory(prefix="component catalogue ") as value:
            root = Path(value)
            module = root / "cmake/platform/Components.cmake"
            module.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "cmake/platform/Components.cmake", module)
            entry = root / "components/custom/Select.cmake"
            entry.parent.mkdir(parents=True)
            entry.write_text(
                'add_library(Nexus::Custom INTERFACE IMPORTED)\n',
                encoding="utf-8")
            (root / "components/catalog.json").write_text(json.dumps({
                "schema_version": 1, "components": [
                    {"id": "custom", "entry": "components/custom/Select.cmake",
                     "target": "Nexus::Custom"}]}), encoding="utf-8")
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(custom_component NONE)\n'
                f'include("{module}")\n'
                'if(TARGET Nexus::Custom)\n'
                '  message(FATAL_ERROR "Unselected component was created")\n'
                'endif()\n'
                'nexus_resolve_components(selected custom)\n'
                'nexus_resolve_components(again custom)\n'
                'if(NOT selected STREQUAL "Nexus::Custom")\n'
                '  message(FATAL_ERROR "Wrong leaf target")\n'
                'endif()\n', encoding="utf-8")
            result = subprocess.run(
                [shutil.which("cmake") or "cmake", "-S", str(root),
                 "-B", str(root / "build"), "-G", "Ninja"],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stdout)

    def test_selected_leaf_supplies_binding_usage_without_name_special_case(self):
        result = self.configure(
            "add_library(nexus_component_bindings INTERFACE)\n"
            "add_library(nexus_component_api INTERFACE)\n"
            "add_library(log INTERFACE)\n"
            "add_library(Nexus::Log ALIAS log)\n"
            "add_library(log_binding INTERFACE)\n"
            "add_library(log_api INTERFACE)\n"
            "set_property(TARGET log PROPERTY "
            "NEXUS_BINDINGS_USAGE_TARGET log_binding)\n"
            "set_property(TARGET log PROPERTY "
            "NEXUS_BINDINGS_API_TARGET log_api)\n"
            "nexus_resolve_components(selected log)\n"
            "get_property(usage TARGET nexus_component_bindings PROPERTY "
            "INTERFACE_LINK_LIBRARIES)\n"
            'if(NOT usage STREQUAL "log_binding")\n'
            '  message(FATAL_ERROR "Binding usage was not connected")\n'
            "endif()\n"
            "get_property(api TARGET nexus_component_api PROPERTY "
            "INTERFACE_LINK_LIBRARIES)\n"
            'if(NOT api STREQUAL "log_api")\n'
            '  message(FATAL_ERROR "Public API and private usage were mixed")\n'
            "endif()\n")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_platform_only_library_gets_generated_smbus_public_headers(self):
        self.assertIsNotNone(shutil.which("arm-none-eabi-gcc"))
        with tempfile.TemporaryDirectory(prefix="platform component API ") as value:
            source = Path(value)
            build = source / "build"
            (source / "consumer.c").write_text(
                '#include "nexus_smbus_factory.h"\n'
                '#include "nexus_bindings.h"\n'
                'int component_api_probe(void) { return NX_SUCCESS; }\n',
                encoding="utf-8")
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\n'
                'project(component_api C ASM)\n'
                'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
                f'find_package(Nexus CONFIG REQUIRED PATHS "{ROOT}/cmake" '
                'NO_DEFAULT_PATH)\n'
                'nexus_add_platform(ASSEMBLY "' + str(ROOT /
                    'cmake/package/verification/smbus/f407.toml') + '")\n'
                'add_library(public_consumer OBJECT consumer.c)\n'
                'target_link_libraries(public_consumer PRIVATE Nexus::Platform)\n',
                encoding="utf-8")
            commands = (
                [shutil.which("cmake") or "cmake", "-S", str(source),
                 "-B", str(build), "-G", "Ninja",
                 f"-DCMAKE_TOOLCHAIN_FILE={ROOT}/cmake/toolchains/arm-gcc.cmake"],
                [shutil.which("cmake") or "cmake", "--build", str(build),
                 "--target", "public_consumer", "--parallel", "1"],
            )
            for command in commands:
                execution = subprocess.run(
                    command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    text=True, check=False, timeout=120)
                self.assertEqual(execution.returncode, 0, execution.stdout)
            database = json.loads((build / "compile_commands.json").read_text())
            consumer, = [entry for entry in database
                         if entry["file"] == str(source / "consumer.c")]
            command = consumer["command"]
            self.assertIn(str(ROOT / "components/buses/smbus/include"), command)
            self.assertNotIn("clock_domain/private", command)
            self.assertNotIn(str(ROOT / "vendors"), command)

    def test_f407_handoff_compiles_from_the_soc_public_include(self):
        includes = [ROOT / layer / "include"
                    for layer in ("core", "io", "arch")]
        includes.extend((ROOT / "components/runtime/clock_domain/include",
                         ROOT / "soc/stm32/stm32f4/include"))
        with tempfile.TemporaryDirectory(prefix="soc handoff header ") as value:
            source = Path(value) / "consumer.c"
            source.write_text(
                '#include "nexus/soc/stm32f407_handoff.h"\n',
                encoding="utf-8")
            args = [shutil.which("cc") or "cc", "-std=c11", "-Werror",
                    "-fsyntax-only", str(source)]
            for directory in includes:
                args.extend(["-I", str(directory)])
            result = subprocess.run(args, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True,
                                    check=False)
            self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
