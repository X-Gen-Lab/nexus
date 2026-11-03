"""Real optional targets preserve generic headers and zero unselected cost."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
PLATFORM = Path(os.environ.get('NEXUS_PLATFORM_TEST_ROOT', ROOT))
SUPPLIER = Path(os.environ.get('NEXUS_MCUBOOT_TEST_SOURCE',
                               ROOT / 'ext/mcuboot'))

COMPOSITION = r'''
#include "nexus/components/mcuboot_control.h"
#include "flash_map_backend/flash_map_backend.h"
#include "nexus_bindings.h"
const nx_mcuboot_flash_operation_t* nx_mcuboot_flash_operation(void) {
    return NULL;
}
void nx_mcuboot_assert_fail(void);
void nx_mcuboot_assert_fail(void) { for (;;) {} }
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) { return 1; }
    nx_mcuboot_action_t action = NX_MCUBOOT_ACTION_NONE;
    nx_result_t queried = nx_mcuboot_query(&action);
    nx_result_t rejected = nx_mcuboot_request(NX_MCUBOOT_REQUEST_TEST);
    nx_result_t stopped = nx_platform_stop();
    return queried == NX_ERROR_IO && rejected == NX_ERROR_UNSUPPORTED &&
           stopped == NX_SUCCESS ? 0 : 2;
}
'''

BOOT_COMPOSITION = r'''
#include "flash_map_backend/flash_map_backend.h"
#include "bootutil/bootutil.h"
#include "nexus_bindings.h"
const nx_mcuboot_flash_operation_t* nx_mcuboot_flash_operation(void) {
    return NULL;
}
void nx_mcuboot_assert_fail(void) { for (;;) {} }
void nx_mcuboot_watchdog_progress(void) {}
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) { return 1; }
    /* Retain the full entry graph; NULL operation forbids invoking boot. */
    fih_ret (*volatile entry)(struct boot_rsp*) = boot_go;
    nx_result_t stopped = nx_platform_stop();
    return entry != NULL && stopped == NX_SUCCESS ? 0 : 2;
}
'''


class MCUbootConsumerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='MCUboot product ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.environment = {key: value for key, value in os.environ.items()
                            if not key.startswith('GIT_')}
        self.environment.update(PYTHONDONTWRITEBYTECODE='1', NEXUS_BUILD_JOBS='1')
        self.cmake = shutil.which('cmake') or str(PLATFORM / '.venv/bin/cmake')

    def configure(self, body, *, supplier=None):
        source = self.root / 'source'
        source.mkdir()
        build = self.root / 'build'
        text = (
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(mcuboot_product LANGUAGES C ASM)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'set(NEXUS_BUILD_TESTS OFF CACHE BOOL "")\n'
            'set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "")\n'
            f'set(NEXUS_ASSEMBLY_FILE "{PLATFORM}/tools/configure/assemblies/native-empty.toml")\n'
            f'add_subdirectory("{PLATFORM}" nexus EXCLUDE_FROM_ALL)\n'
            f'include("{ROOT}/components/mcuboot/CMakeLists.txt")\n'
            'set(NEXUS_MCUBOOT_DEVELOPMENT ON)\n'
            f'set(NEXUS_MCUBOOT_SOURCE "{supplier or SUPPLIER}")\n' + body)
        (source / 'CMakeLists.txt').write_text(text)
        (source / 'generic.c').write_text(
            '#include "nexus/components/mcuboot_control.h"\n'
            '#if __has_include("bootutil/bootutil_public.h") || '
            '__has_include("mcuboot_config/mcuboot_config.h") || '
            '__has_include("flash_map_backend/flash_map_backend.h")\n'
            '#error Private integration headers leaked\n#endif\n'
            'int main(void) { return 0; }\n')
        (source / 'composition.c').write_text(COMPOSITION)
        (source / 'boot.c').write_text(BOOT_COMPOSITION)
        (source / 'empty.c').write_text('int main(void) { return 0; }\n')
        result = subprocess.run([self.cmake, '-S', str(source), '-B', str(build),
                                 '-G', 'Ninja'], env=self.environment,
            capture_output=True, text=True, check=False, timeout=90)
        return build, result

    def test_unselected_definitions_create_no_vendor_or_runtime_target(self):
        build, result = self.configure(
            'foreach(name bootutil mcuboot_crypto nexus_mcuboot_control '
            'nexus_mcuboot_public nexus_mcuboot_flash)\n'
            'if(TARGET "${name}")\n'
            'message(FATAL_ERROR "Unselected target: ${name}")\n'
            'endif()\nendforeach()\n'
            'nexus_add_firmware(empty SOURCES empty.c)\n',
            supplier=self.root / 'deliberately uninitialized source')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_control_helper_is_idempotent_and_vendor_headers_are_explicit(self):
        build, result = self.configure(
            'nexus_add_mcuboot_control()\nnexus_add_mcuboot_control()\n'
            'if(TARGET bootutil OR TARGET mcuboot_crypto)\n'
            'message(FATAL_ERROR "Control configured image crypto")\nendif()\n'
            'add_executable(generic generic.c)\n'
            'target_link_libraries(generic PRIVATE Nexus::MCUBootControl)\n'
            'nexus_add_firmware(composition SOURCES composition.c\n'
            ' LIBRARIES Nexus::MCUBootControl Nexus::MCUBootPort)\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([self.cmake, '--build', str(build), '--target',
                                 'generic', 'composition', '--parallel', '1'],
            env=self.environment, capture_output=True, text=True,
            check=False, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([str(build / 'bin/composition')],
            env=self.environment, capture_output=True, text=True,
            check=False, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_no_profile_argument_can_override_selected_contract(self):
        build, result = self.configure('nexus_add_mcuboot_control(override)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('accepts no', result.stdout + result.stderr)

    def test_full_boot_public_target_supplies_its_complete_fixed_link_profile(self):
        build, result = self.configure(
            'nexus_add_mcuboot_boot()\nnexus_add_mcuboot_boot()\n'
            'nexus_add_firmware(boot SOURCES boot.c\n'
            f' "{ROOT}/tests/contracts/mcuboot_link/public_key.c"\n'
            ' LIBRARIES Nexus::MCUBootBootStorage)\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([self.cmake, '--build', str(build), '--target',
                                 'boot', '--parallel', '1'],
            env=self.environment, capture_output=True, text=True,
            check=False, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([str(build / 'bin/boot')],
            env=self.environment, capture_output=True, text=True,
            check=False, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_source_rejects_before_vendor_target_configuration(self):
        build, result = self.configure('nexus_add_mcuboot_control()\n',
                                       supplier=self.root / 'missing')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('MCUboot source rejected', result.stdout + result.stderr)

    def test_symbolic_source_selector_rejects_without_realpath_laundering(self):
        alias = self.root / 'source alias'
        alias.symlink_to(SUPPLIER, target_is_directory=True)
        build, result = self.configure('nexus_add_mcuboot_control()\n',
                                       supplier=alias)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('MCUboot source rejected', result.stdout + result.stderr)

    def tls_body(self):
        return (f'include("{ROOT}/components/tls/CMakeLists.txt")\n'
                'nexus_add_tls()\n')

    def test_supported_same_image_boot_and_tls_targets_are_incompatible(self):
        build, result = self.configure(
            self.tls_body() + 'nexus_add_mcuboot_boot()\n'
            'add_executable(first empty.c)\n'
            'target_link_libraries(first PRIVATE Nexus::MCUBootBootStorage '
            'Nexus::TLSStorage)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('NEXUS_CRYPTO_ABI', result.stdout + result.stderr)

    def test_control_with_tls_in_the_same_image_remains_supported(self):
        build, result = self.configure(
            self.tls_body() + 'nexus_add_mcuboot_control()\n'
            'add_executable(first empty.c)\n'
            'target_link_libraries(first PRIVATE Nexus::MCUBootControl '
            'Nexus::TLS)\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_independent_boot_and_tls_images_share_a_graph_without_abi_conflict(self):
        build, result = self.configure(
            self.tls_body() + 'nexus_add_mcuboot_boot()\n'
            'add_executable(first empty.c)\n'
            'target_link_libraries(first PRIVATE Nexus::MCUBootBootStorage)\n'
            'add_executable(second empty.c)\n'
            'target_link_libraries(second PRIVATE Nexus::TLS)\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_tls_helper_does_not_require_caller_side_python_discovery(self):
        build, result = self.configure(self.tls_body())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
