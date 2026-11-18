"""Public discovery and explicit assembly use the actual source package."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
ASSEMBLY = ROOT / "tests/support/native/assemblies/native-empty.toml"


class PlatformEntryTests(unittest.TestCase):
    def configure(self, body):
        with tempfile.TemporaryDirectory(prefix="nexus public entry ") as directory:
            source = Path(directory)
            (source / "main.c").write_text("int main(void) { return 0; }\n")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.31)\n"
                "project(public_consumer LANGUAGES C ASM)\n"
                f'set(Python3_EXECUTABLE "{sys.executable}")\n'
                f'set(Nexus_DIR "{ROOT}/cmake")\n'
                "find_package(Nexus 0.2.1 EXACT CONFIG REQUIRED)\n" + body)
            return subprocess.run(
                [shutil.which("cmake"), "-S", str(source), "-B",
                 str(source / "build"), "-G", "Ninja"],
                capture_output=True, text=True, timeout=90,
                env={**os.environ, "CMAKE_BUILD_PARALLEL_LEVEL": "1"})

    def assert_success(self, body):
        result = self.configure(body)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_discovery_has_no_assembly_or_development_side_effects(self):
        self.assert_success('''\
find_package(Nexus CONFIG REQUIRED)
foreach(target Nexus::Platform Nexus::Config nexus_firmware nexus_google_contracts)
    if(TARGET "${target}")
        message(FATAL_ERROR "Discovery created ${target}")
    endif()
endforeach()
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/generated")
    message(FATAL_ERROR "Discovery generated an assembly")
endif()
''')

    def test_explicit_platform_preserves_caller_scope(self):
        self.assert_success(f'''\
set(NEXUS_TEST_SUPPORT ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/caller-bin")
nexus_add_platform(ASSEMBLY "{ASSEMBLY}")
if(NOT TARGET Nexus::Platform OR NOT TARGET Nexus::OS OR NOT TARGET Nexus::ABI)
    message(FATAL_ERROR "Public assembly targets missing")
endif()
if(NOT CMAKE_C_STANDARD EQUAL 99 OR
   NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY STREQUAL "${{CMAKE_BINARY_DIR}}/caller-bin")
    message(FATAL_ERROR "Assembly changed caller policy")
endif()
if(DEFINED NEXUS_CONFIG_DIR OR DEFINED NEXUS_SOC_TARGET)
    message(FATAL_ERROR "Private assembly selections escaped")
endif()
nexus_add_firmware(app SOURCES main.c)
get_target_property(standard app C_STANDARD)
if(NOT standard EQUAL 11)
    message(FATAL_ERROR "Firmware lost C11")
endif()
''')

    def test_duplicate_assembly_rejects(self):
        result = self.configure(f'''\
set(NEXUS_TEST_SUPPORT ON)
nexus_add_platform(ASSEMBLY "{ASSEMBLY}")
nexus_add_platform(ASSEMBLY "{ASSEMBLY}")
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("only one resolved Nexus assembly", result.stdout + result.stderr)

    def test_unknown_or_repeated_arguments_reject(self):
        for arguments in ('', 'UNKNOWN value',
                          f'ASSEMBLY "{ASSEMBLY}" ASSEMBLY "{ASSEMBLY}"'):
            with self.subTest(arguments=arguments):
                result = self.configure(f"nexus_add_platform({arguments})\n")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("nexus_add_platform requires", result.stdout + result.stderr)

    def test_relative_assembly_is_relative_to_calling_directory(self):
        self.assert_success(f'''\
set(NEXUS_TEST_SUPPORT ON)
file(RELATIVE_PATH local_assembly "${{CMAKE_CURRENT_SOURCE_DIR}}" "{ASSEMBLY}")
nexus_add_platform(ASSEMBLY "${{local_assembly}}")
if(NOT TARGET Nexus::Platform)
    message(FATAL_ERROR "Relative assembly was not created")
endif()
''')


if __name__ == "__main__":
    unittest.main()
