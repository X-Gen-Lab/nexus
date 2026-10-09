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
            '#include "runtime/nx_runtime.h"\n'
            '#ifndef NX_CONFIG_PLATFORM_NATIVE\n#error Wrong effective configuration\n#endif\n'
            '#ifndef NX_CONFIG_BUILD_TYPE_RELEASE\n#error Wrong build mode\n#endif\n'
            'int main(void) {\n'
            '  if (nx_runtime_bootstrap(0) != NX_OK) return 1;\n'
            '  return nx_runtime_shutdown(0) != NX_OK;\n}\n')
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
            'set(CONFIG_FIRMWARE_LIBC_HEAP_SIZE invalid-parent-value)\n'
            'nexus_add_application(TARGET product_firmware SOURCES application.c)\n'
            'add_executable(parent_sentinel parent.c)\n'
            'foreach(public_target Config HAL HALInterface OSAL OSALInterface Platform Runtime Firmware '
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

    def test_minimal_product_links_and_runs_without_optional_services_or_openssl(self):
        (self.source / 'application.c').write_text(
            '#include "runtime/nx_runtime.h"\n'
            '#include "runtime/nx_platform_info.h"\n'
            '#include <string.h>\n'
            'int main(void) {\n'
            '  if (strcmp(nx_platform_get_info()->board, "native-reference")) return 1;\n'
            '  if (nx_runtime_bootstrap(0) != NX_OK) return 2;\n'
            '  return nx_runtime_shutdown(0) != NX_OK;\n}\n')
        body = (
            f'set(NEXUS_CONFIG_FILE "{(ROOT / "configs/native_minimal_defconfig").as_posix()}")\n'
            'set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL TRUE)\n'
            + self.add_nexus() +
            'foreach(excluded Security Storage Update ModbusRTU Industrial '
            'ConfigManager Log Shell Init)\n'
            '  if(TARGET Nexus::${excluded})\n'
            '    message(FATAL_ERROR "Disabled component target was created: ${excluded}")\n'
            '  endif()\nendforeach()\n'
            'if(TARGET OpenSSL::Crypto)\n'
            '  message(FATAL_ERROR "Disabled crypto provider was discovered")\nendif()\n'
            'nexus_add_application(TARGET minimal_product SOURCES application.c)\n'
            'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/minimal-path.txt" CONTENT '
            '"$<TARGET_FILE:minimal_product>\\n")\n')
        result = self.configure(body)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_checked(['cmake', '--build', str(self.build), '--parallel', '2',
                          '--target', 'minimal_product'])
        self.run_checked([(self.build / 'minimal-path.txt').read_text().strip()])

    def test_external_build_mode_conflict_is_rejected(self):
        (self.source / 'product.config').write_text(
            'CONFIG_PLATFORM_NATIVE=y\nCONFIG_OSAL_NATIVE=y\nCONFIG_BUILD_TYPE_DEBUG=y\n')
        result = self.configure(
            'set(NEXUS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/product.config")\n'
            + self.add_nexus())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('cannot be honored', result.stderr)
        self.assertFalse((self.build / 'nexus-owned/generated/nexus_config.h').exists())

    def test_hal_only_consumer_does_not_inherit_osal_headers(self):
        (self.source / 'hal_consumer.c').write_text(
            '#include "hal/nx_hal.h"\n'
            '#if __has_include("osal/osal.h")\n'
            '#error HAL public target leaked OSAL headers\n#endif\n'
            '#include <string.h>\n'
            'int main(void) { return nx_status_to_string(NX_OK) == 0; }\n')
        body = (
            f'set(NEXUS_CONFIG_FILE "{(ROOT / "configs/native_minimal_defconfig").as_posix()}")\n'
            'set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL TRUE)\n'
            + self.add_nexus() +
            'add_executable(hal_only hal_consumer.c)\n'
            'target_link_libraries(hal_only PRIVATE Nexus::HAL)\n'
            'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/hal-path.txt" CONTENT '
            '"$<TARGET_FILE:hal_only>\\n")\n')
        result = self.configure(body)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_checked(['cmake', '--build', str(self.build), '--parallel', '2',
                          '--target', 'hal_only'])
        self.run_checked([(self.build / 'hal-path.txt').read_text().strip()])

    def test_minimal_profile_enables_only_tests_for_present_components(self):
        body = (
            f'set(NEXUS_CONFIG_FILE "{(ROOT / "configs/native_minimal_defconfig").as_posix()}")\n'
            'set(NEXUS_BUILD_TESTS ON)\n'
            'set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL TRUE)\n'
            + self.add_nexus() +
            'foreach(excluded config_tests shell_tests log_tests init_tests '
            'integration_tests crypto_tests storage_tests update_tests nexus_industrial_tests)\n'
            '  if(TARGET ${excluded})\n'
            '    message(FATAL_ERROR "Tests forced a disabled component: ${excluded}")\n'
            '  endif()\nendforeach()\n'
            'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/smoke-path.txt" CONTENT '
            '"$<TARGET_FILE:runtime_native_smoke>\\n")\n')
        result = self.configure(body)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_checked(['cmake', '--build', str(self.build), '--parallel', '2',
                          '--target', 'runtime_native_smoke'])
        self.run_checked([(self.build / 'smoke-path.txt').read_text().strip()])

    def test_external_board_package_builds_cpp_consumer_with_isolated_identity(self):
        board = self.source / 'outside SDK board'
        shutil.copytree(ROOT / 'tests/fixtures/external_board', board)
        (self.source / 'application.cpp').write_text(
            '#include "runtime/nx_runtime.h"\n'
            '#include "runtime/nx_platform_info.h"\n'
            '#include <cstring>\n'
            'int main() {\n'
            '  if (std::strcmp(nx_platform_get_info()->board, "external-native-fixture")) return 1;\n'
            '  if (nx_runtime_bootstrap(nullptr) != NX_OK) return 2;\n'
            '  return nx_runtime_shutdown(nullptr) != NX_OK;\n}\n')
        body = (
            'enable_language(CXX)\n'
            'set(NEXUS_BOARD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/outside SDK board")\n'
            f'set(NEXUS_CONFIG_FILE "{(ROOT / "configs/native_minimal_defconfig").as_posix()}")\n'
            + self.add_nexus() +
            'nexus_add_application(TARGET cpp_board SOURCES application.cpp VERSION fixture-1)\n'
            'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/external-path.txt" CONTENT '
            '"$<TARGET_FILE:cpp_board>\\n")\n')
        result = self.configure(body)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.run_checked(['cmake', '--build', str(self.build), '--parallel', '2', '--target', 'cpp_board'])
        self.run_checked([(self.build / 'external-path.txt').read_text().strip()])
        import json
        identity = json.loads((self.build / 'nexus-owned/generated/board-identity.json').read_text())
        self.assertEqual(identity['id'], 'external-native-fixture')
        self.assertIn('nexus_board.h', identity['inputs_sha256'])
        effective = (self.build / 'nexus-owned/generated/effective.config').read_text()
        self.assertIn('CONFIG_BOARD_NAME="external-native-fixture"', effective)
        self.assertIn('CONFIG_BOARD_EXTERNAL=y', effective)

    def test_external_board_missing_or_wrong_manifest_fails_without_builtin_fallback(self):
        for directory in ('missing-board', 'wrong-board'):
            board = self.source / directory
            if directory == 'wrong-board':
                shutil.copytree(ROOT / 'tests/fixtures/external_board', board)
                import json
                data = json.loads((board / 'board.json').read_text())
                data['soc'] = 'gd32f303'
                (board / 'board.json').write_text(json.dumps(data))
            result = self.configure(
                f'set(NEXUS_BOARD_DIR "{board.as_posix()}")\n' + self.add_nexus())
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((self.build / 'nexus-owned/generated/board-identity.json').exists())

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
