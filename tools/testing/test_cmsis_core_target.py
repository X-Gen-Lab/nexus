"""Execute CMSIS Core ownership and include visibility through real CMake."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "cmake/platform/CMSISCore.cmake"
CORE = ROOT / "vendors/arm/CMSIS_6/CMSIS/Core/Include"


class CMSISCoreTargetTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="CMSIS Core target ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.case = 0

    def configure(self, body, *, included=True, module=MODULE):
        self.case += 1
        directory = self.root / str(self.case)
        directory.mkdir()
        (directory / "provider.c").write_text(
            '#include <cmsis_version.h>\n'
            '_Static_assert(__CM_CMSIS_VERSION_MAIN == 6U, "CMSIS 6");\n'
            '#ifndef CMSIS_DISABLE_DEPRECATED\n'
            '#error "Legacy CMSIS aliases remain enabled"\n'
            '#endif\n'
            'int provider(void) { return 0; }\n')
        (directory / "consumer.c").write_text(
            '#if __has_include(<cmsis_version.h>)\n'
            '#error "Private CMSIS Core headers escaped the provider"\n'
            '#endif\n'
            'int provider(void);\n'
            'int main(void) { return provider(); }\n')
        (directory / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(cmsis_core_target C)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            + (f'include("{module}")\n' if included else '') + body)
        result = subprocess.run([
            shutil.which("cmake") or "cmake", "-S", str(directory),
            "-B", str(directory / "build"), "-G", "Ninja"],
            capture_output=True, text=True, timeout=60)
        return directory, result

    def build(self, directory):
        result = subprocess.run([
            shutil.which("cmake") or "cmake", "--build",
            str(directory / "build"), "--parallel", "1"],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_private_provider_uses_core_without_exposing_headers_to_consumer(self):
        directory, result = self.configure(
            'add_library(provider STATIC provider.c)\n'
            'nexus_use_cmsis_core(provider)\n'
            'add_executable(consumer consumer.c)\n'
            'target_link_libraries(consumer PRIVATE provider)\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.build(directory)
        commands = json.loads(
            (directory / "build/compile_commands.json").read_text())
        provider = next(row for row in commands
                        if Path(row["file"]).name == "provider.c")
        consumer = next(row for row in commands
                        if Path(row["file"]).name == "consumer.c")
        self.assertIn(str(CORE), provider["command"])
        self.assertNotIn(str(CORE), consumer["command"])

    def test_getter_works_after_module_was_first_included_by_child_directory(self):
        child = self.root / "child"
        child.mkdir()
        (child / "CMakeLists.txt").write_text(f'include("{MODULE}")\n')
        directory, result = self.configure(
            f'add_subdirectory("{child}" child)\n'
            'unset(NEXUS_CMSIS_CORE_INCLUDE_DIR)\n'
            'nexus_cmsis_core_include(core)\n'
            f'if(NOT core STREQUAL "{CORE}")\n'
            '  message(FATAL_ERROR "Core source changed with call scope")\n'
            'endif()\n'
            'add_library(provider STATIC provider.c)\n'
            'nexus_use_cmsis_core(provider)\n', included=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.build(directory)

    def test_selected_interface_implementation_forwards_core(self):
        directory, result = self.configure(
            'add_library(implementation INTERFACE)\n'
            'nexus_use_cmsis_core(implementation)\n'
            'add_library(provider STATIC provider.c)\n'
            'target_link_libraries(provider PRIVATE implementation)\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.build(directory)

    def test_native_without_core_sources_builds_and_explicit_use_rejects(self):
        module = self.root / "source/cmake/platform/CMSISCore.cmake"
        module.parent.mkdir(parents=True)
        shutil.copyfile(MODULE, module)
        directory, result = self.configure(
            'add_library(native STATIC consumer.c)\n', module=module)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.build(directory)
        _, result = self.configure(
            'add_library(provider STATIC provider.c)\n'
            'nexus_use_cmsis_core(provider)\n', module=module)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing reviewed CMSIS 6 Core",
                      result.stdout + result.stderr)

    def test_unknown_targets_and_extra_arguments_reject(self):
        for statement in ('nexus_use_cmsis_core(missing)\n',
                          'add_library(provider STATIC provider.c)\n'
                          'nexus_use_cmsis_core(provider LEGACY)\n'):
            with self.subTest(statement=statement):
                _, result = self.configure(statement)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("CMSIS Core target", result.stdout + result.stderr)

    def test_f407_generated_bindings_use_strict_core_privately(self):
        build = self.root / "actual F407 bindings"
        result = subprocess.run([
            shutil.which("cmake") or "cmake", "--preset",
            "stm32-qiming-baremetal", "-B", str(build),
            "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF"],
            cwd=ROOT, capture_output=True, text=True, timeout=90)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        commands = json.loads((build / "compile_commands.json").read_text())
        binding = next(row for row in commands
                       if Path(row["file"]).name == "bindings.c")
        self.assertIn("-DCMSIS_DISABLE_DEPRECATED=1", binding["command"])
        self.assertIn(str(CORE), binding["command"])


if __name__ == "__main__":
    unittest.main()
