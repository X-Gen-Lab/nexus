"""Build and run an independent product consuming Nexus as a source SDK."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ExternalConsumerTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(shutil.which('cmake'), 'CMake is required')
        self.directory = tempfile.TemporaryDirectory(prefix='nexus consumer ')
        self.addCleanup(self.directory.cleanup)
        self.source = Path(self.directory.name) / 'product source'
        self.build = Path(self.directory.name) / 'product build'
        self.source.mkdir()

    def run_checked(self, arguments):
        result = subprocess.run(arguments, text=True, capture_output=True, timeout=90)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def configure(self, body, *arguments):
        (self.source / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.21)\n'
            'project(independent_product C)\n'
            'set(CMAKE_BUILD_TYPE Release CACHE STRING "Product mode" FORCE)\n' + body)
        return subprocess.run(['cmake', '-S', str(self.source), '-B', str(self.build),
                               *arguments], text=True, capture_output=True, timeout=60)

    def add_nexus(self):
        return f'add_subdirectory("{ROOT.as_posix()}" nexus-owned)\n'

    def test_parent_application_links_real_platform_with_isolated_outputs(self):
        (self.source / 'parent.c').write_text('int main(void) { return 0; }\n')
        (self.source / 'application.c').write_text(
            '#include "nexus_config.h"\n'
            '#include "hal/nx_hal.h"\n'
            '#include "osal/osal.h"\n'
            '#ifndef NX_CONFIG_PLATFORM_NATIVE\n#error Wrong effective configuration\n#endif\n'
            '#ifndef NX_CONFIG_BUILD_TYPE_RELEASE\n#error Wrong build mode\n#endif\n'
            'int main(void) {\n'
            '  if (osal_init() != OSAL_OK || nx_hal_init() != NX_OK) return 1;\n'
            '  return nx_hal_deinit() != NX_OK;\n}\n')
        body = (
            'set(CMAKE_C_STANDARD 99)\n'
            'set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/parent-bin")\n'
            'set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/parent-lib")\n'
            'set(CONFIG_PARENT_FEATURE parent-value CACHE STRING "Parent setting")\n'
            + self.add_nexus() +
            'if(NEXUS_BUILD_TESTS OR NEXUS_BUILD_EXAMPLES)\n'
            '  message(FATAL_ERROR "SDK enabled its development targets")\nendif()\n'
            'if(NOT CONFIG_PARENT_FEATURE STREQUAL "parent-value")\n'
            '  message(FATAL_ERROR "SDK removed parent configuration")\nendif()\n'
            'if(NOT CMAKE_C_STANDARD EQUAL 99 OR NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY '
            'STREQUAL "${CMAKE_BINARY_DIR}/parent-bin")\n'
            '  message(FATAL_ERROR "SDK changed parent build defaults")\nendif()\n'
            # These caller variables must not redirect the SDK context.
            'set(NEXUS_PLATFORM foreign-parent-value)\n'
            'set(CONFIG_APP_HEAP_SIZE invalid-parent-value)\n'
            'nexus_add_application(TARGET product_firmware SOURCES application.c)\n'
            'add_executable(parent_sentinel parent.c)\n'
            'foreach(public_target Config HAL HALInterface OSAL OSALInterface Platform '
            'Storage Security Update ModbusRTU Industrial)\n'
            '  if(NOT TARGET Nexus::${public_target})\n'
            '    message(FATAL_ERROR "Missing SDK target ${public_target}")\n'
            '  endif()\nendforeach()\n'
            'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/product-paths.txt" CONTENT '
            '"$<TARGET_FILE:product_firmware>\\n$<TARGET_FILE:parent_sentinel>\\n")\n')
        result = self.configure(body)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_checked(['cmake', '--build', str(self.build), '--parallel', '2',
                          '--target', 'product_firmware', 'parent_sentinel'])
        firmware, sentinel = (self.build / 'product-paths.txt').read_text().splitlines()
        self.assertEqual(Path(firmware).parent, self.build / 'nexus-owned/bin')
        self.assertEqual(Path(sentinel).parent, self.build / 'parent-bin')
        self.run_checked([firmware])
        self.run_checked([sentinel])
        self.assertFalse((self.build / 'generated').exists())
        generated = self.build / 'nexus-owned/generated'
        for name in ('effective.config', 'nexus_config.h', 'config.cmake'):
            self.assertTrue((generated / name).is_file(), name)
        self.assertIn('CONFIG_BUILD_TYPE="Release"',
                      (generated / 'effective.config').read_text())
        self.assertIn('CONFIG_PARENT_FEATURE:STRING=parent-value',
                      (self.build / 'CMakeCache.txt').read_text())
        if sys.platform.startswith('linux'):
            self.assertTrue((Path(firmware).parent / 'product_firmware.map').is_file())

    def test_missing_external_fragment_fails_without_fallback(self):
        result = self.configure(
            'set(NEXUS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/missing.config")\n'
            + self.add_nexus())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Configuration fragment missing', result.stderr)
        self.assertFalse((self.build / 'nexus-owned/generated/nexus_config.h').exists())

    def test_external_build_mode_conflict_is_rejected(self):
        (self.source / 'product.config').write_text(
            'CONFIG_PLATFORM_NATIVE=y\nCONFIG_OSAL_NATIVE=y\nCONFIG_BUILD_TYPE_DEBUG=y\n')
        result = self.configure(
            'set(NEXUS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/product.config")\n'
            + self.add_nexus())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('cannot be honored', result.stderr)
        self.assertFalse((self.build / 'nexus-owned/generated/nexus_config.h').exists())

    def test_parent_multi_configuration_is_not_silently_rewritten(self):
        self.assertIsNotNone(shutil.which('ninja'), 'Ninja is required for this contract')
        result = self.configure(
            'set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Parent modes")\n'
            + self.add_nexus(), '-G', 'Ninja Multi-Config')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('requires one parent-selected configuration', result.stderr)
        self.assertIn('CMAKE_CONFIGURATION_TYPES:STRING=Debug;Release',
                      (self.build / 'CMakeCache.txt').read_text())


if __name__ == '__main__':
    unittest.main()
