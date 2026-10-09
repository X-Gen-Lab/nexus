"""Behavioral regressions for resolved configuration and actual CMake consumers."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / 'scripts/kconfig/generate_config.py'
sys.path.insert(0, str(ROOT / 'scripts/ci'))
from package_release import (RELEASE_PROFILES, ReleaseError, cmake_cache,
                             validate_configuration_bundle, validate_effective_build)


class EffectiveConfigTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)
        self.fragment = self.work / 'input.config'
        self.fragment.write_text('CONFIG_PLATFORM_NATIVE=y\nCONFIG_OSAL_NATIVE=y\n')

    def generate(self, *args):
        return subprocess.run([sys.executable, str(GENERATOR), '--config', str(self.fragment),
                               '--output', str(self.work / 'nexus_config.h'),
                               '--effective-config', str(self.work / 'effective.config'),
                               '--cmake-output', str(self.work / 'config.cmake'), *args],
                              cwd=ROOT, text=True, capture_output=True)

    def test_effective_header_and_cmake_agree_and_are_reproducible(self):
        result = self.generate('--set', 'BUILD_TYPE_RELEASE=y')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('CONFIG_BUILD_TYPE="Release"', (self.work / 'effective.config').read_text())
        self.assertIn('#define NX_CONFIG_BUILD_TYPE "Release"', (self.work / 'nexus_config.h').read_text())
        self.assertIn('set(CONFIG_BUILD_TYPE [=[Release]=])', (self.work / 'config.cmake').read_text())
        first = (self.work / 'nexus_config.h').read_bytes()
        self.assertEqual(self.generate('--set', 'BUILD_TYPE_RELEASE=y').returncode, 0)
        self.assertEqual(first, (self.work / 'nexus_config.h').read_bytes())

    def test_disabled_build_controls_remain_explicit_in_the_real_generated_bundle(self):
        result = self.generate('--set', 'BUILD_TESTS=n',
                               '--set', 'ENABLE_COVERAGE=n', '--set', 'ENABLE_SANITIZERS=n')
        self.assertEqual(result.returncode, 0, result.stderr)
        contents = {name: (self.work / name).read_text() for name in
                    ('effective.config', 'nexus_config.h', 'config.cmake')}
        config = validate_configuration_bundle(contents)
        for name in ('BUILD_TESTS', 'ENABLE_COVERAGE', 'ENABLE_SANITIZERS'):
            self.assertIs(config['CONFIG_' + name], False)
        result = self.generate('--set', 'BUILD_TESTS=n',
                               '--set', 'ENABLE_COVERAGE=n', '--set', 'ENABLE_SANITIZERS=n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(contents, {name: (self.work / name).read_text() for name in contents})

    def test_unknown_symbol_fails_without_artifacts(self):
        self.fragment.write_text('CONFIG_OBSOLETE_DRIVER=y\n')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unknown configuration symbol', result.stderr)
        self.assertFalse((self.work / 'nexus_config.h').exists())

    def test_unmaintained_platforms_and_silicon_cannot_select_a_fallback(self):
        for symbol in ('PLATFORM_ESP32', 'PLATFORM_NRF52', 'PLATFORM_GD32',
                       'STM32H7', 'STM32L4', 'STM32F429', 'STM32_I2C_ENABLE',
                       'STM32_ADC_ENABLE', 'STM32_TIMER_ENABLE'):
            with self.subTest(symbol=symbol):
                self.fragment.write_text('CONFIG_PLATFORM_STM32=y\nCONFIG_' + symbol + '=y\n')
                result = self.generate('--set', 'TOOLCHAIN_ARM_GCC=y')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Unknown configuration symbol CONFIG_' + symbol, result.stderr)
                self.assertFalse((self.work / 'nexus_config.h').exists())

    def test_choice_conflict_is_not_silently_coerced(self):
        self.fragment.write_text('CONFIG_PLATFORM_NATIVE=y\nCONFIG_PLATFORM_STM32=y\n')
        self.assertIn('cannot be honored', self.generate().stderr)

    def test_range_and_dependency_conflicts_fail(self):
        self.fragment.write_text('CONFIG_PLATFORM_NATIVE=y\nCONFIG_OSAL_TICK_RATE_HZ=1\n')
        self.assertNotEqual(self.generate().returncode, 0)
        self.fragment.write_text('CONFIG_PLATFORM_NATIVE=y\nCONFIG_STM32_SPI_ENABLE=y\n')
        self.assertNotEqual(self.generate().returncode, 0)

    def test_duplicate_and_malformed_input_fail(self):
        for body in ('CONFIG_PLATFORM_NATIVE=y\nCONFIG_PLATFORM_NATIVE=y\n', 'CONFIG_PLATFORM_NATIVE=yes\n', 'bogus input\n'):
            self.fragment.write_text(body)
            self.assertNotEqual(self.generate().returncode, 0)

    def test_missing_input_does_not_fall_back(self):
        self.fragment.unlink()
        self.assertNotEqual(self.generate().returncode, 0)

    def test_update_policy_can_use_external_ports_without_storage_or_crypto(self):
        self.fragment.write_text('CONFIG_PLATFORM_NATIVE=y\nCONFIG_SERVICE_STORAGE=n\n'
                                 'CONFIG_SERVICE_SECURITY=n\nCONFIG_FRAMEWORK_CONFIG=n\n'
                                 'CONFIG_SERVICE_UPDATE=y\n')
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('#define NX_CONFIG_SERVICE_UPDATE 1',
                      (self.work / 'nexus_config.h').read_text())

    def test_obsolete_inactive_firmware_budget_fails_with_named_diagnostic(self):
        self.fragment.write_text('CONFIG_PLATFORM_STM32=y\nCONFIG_STM32_STACK_SIZE=0x2000\n')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unknown configuration symbol CONFIG_STM32_STACK_SIZE', result.stderr)

    def test_reference_led_exposes_the_class_opened_by_product_application(self):
        self.fragment.write_text((ROOT / 'configs/stm32f407_baremetal_defconfig').read_text())
        result = self.generate('--set', 'TOOLCHAIN_ARM_GCC=y')
        self.assertEqual(result.returncode, 0, result.stderr)

        source = self.work / 'led_identity.c'
        source.write_text('#include "nexus_config.h"\n'
                          '_Static_assert(NX_CONFIG_GPIO_D12_RW_MODE == 2, "typed GPIO class");\n'
                          '_Static_assert(NX_CONFIG_GPIO_D12_MODE == 1, "push-pull output");\n'
                          '_Static_assert(NX_CONFIG_GPIO_D12_INIT_VALUE == 0, "inactive LD4");\n')
        result = subprocess.run([shutil.which('cc'), '-std=c11', '-Werror', '-I', str(self.work),
                                 '-fsyntax-only', str(source)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unimplemented_uart_dma_fails_before_emitting_firmware_configuration(self):
        self.fragment.write_text((ROOT / 'configs/stm32f407_baremetal_defconfig').read_text()
                                 .replace('CONFIG_STM32_UART_USE_DMA=n',
                                          'CONFIG_STM32_UART_USE_DMA=y'))
        result = self.generate('--set', 'TOOLCHAIN_ARM_GCC=y')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('CONFIG_STM32_UART_USE_DMA=y cannot be honored', result.stderr)
        self.assertFalse((self.work / 'nexus_config.h').exists())

    def test_gd32_physical_identity_and_product_are_resolved_from_one_profile(self):
        self.fragment.write_text((ROOT / 'configs/gd32f470_baremetal_defconfig').read_text())
        result = self.generate('--set', 'TOOLCHAIN_ARM_GCC=y')
        self.assertEqual(result.returncode, 0, result.stderr)
        config = (self.work / 'effective.config').read_text()
        for setting in ('CONFIG_PLATFORM_NAME="gd32f470"',
                        'CONFIG_BOARD_NAME="gd32f470zg-liangshan"'):
            self.assertIn(setting, config)
        for name, expected in (('LINKER_RAM_SIZE', 0x30000),
                               ('LINKER_FLASH_SIZE', 0x100000)):
            value = next(line.split('=', 1)[1] for line in config.splitlines()
                         if line.startswith('CONFIG_' + name + '='))
            self.assertEqual(int(value, 16), expected)
        self.fragment.write_text((ROOT / 'configs/gd32f470_baremetal_defconfig').read_text()
                                 + 'CONFIG_LINKER_RAM_SIZE=0x20000\n')
        self.assertNotEqual(self.generate('--set', 'TOOLCHAIN_ARM_GCC=y').returncode, 0)

    def test_hsi_clock_keeps_a_valid_board_crystal_constant_for_vendor_decoder(self):
        self.fragment.write_text((ROOT / 'configs/stm32f407_baremetal_defconfig').read_text()
                                 + 'CONFIG_STM32_HSE_ENABLE=n\n')
        result = self.generate('--set', 'TOOLCHAIN_ARM_GCC=y')
        self.assertEqual(result.returncode, 0, result.stderr)

        source = self.work / 'clock_identity.c'
        source.write_text('#include "nexus_config.h"\n'
                          '#ifdef NX_CONFIG_STM32_HSE_ENABLE\n#error HSE requested disabled\n#endif\n'
                          '_Static_assert(NX_CONFIG_STM32_HSE_VALUE == 8000000, "board crystal");\n'
                          '_Static_assert(NX_CONFIG_STM32_HSI_VALUE == 16000000, "silicon HSI");\n')
        result = subprocess.run([shutil.which('cc'), '-std=c11', '-Werror', '-I', str(self.work),
                                 '-fsyntax-only', str(source)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_customer_board_profiles_bind_density_and_typed_led_safely(self):
        for stem, part, flash, pin, inactive in (
            ('stm32f407zg_qiming_v31', 'STM32F407ZGT6', 0x100000, 'E3', 1),
            ('stm32f407ve_sky_qingchun', 'STM32F407VET6', 0x80000, 'B2', 0),
        ):
            for backend in ('baremetal', 'freertos'):
                with self.subTest(board=stem, backend=backend):
                    self.fragment.write_text((ROOT / 'configs' / (stem + '_' + backend + '_defconfig')).read_text())
                    result = self.generate('--set', 'TOOLCHAIN_ARM_GCC=y')
                    self.assertEqual(result.returncode, 0, result.stderr)
                    sys.path.insert(0, str(ROOT / 'scripts/ci'))
                    from package_release import effective_config
                    config = effective_config((self.work / 'effective.config').read_text())
                    self.assertEqual(config['CONFIG_STM32_PART_NAME'], part)
                    self.assertEqual(config['CONFIG_STM32_FLASH_SIZE'], flash)
                    self.assertEqual(config['CONFIG_LINKER_FLASH_SIZE'], flash)
                    self.assertEqual(config['CONFIG_LINKER_RAM_SIZE'], 0x20000)
                    self.assertEqual(config['CONFIG_OSAL_BACKEND_NAME'], backend)
                    self.assertEqual(config['CONFIG_GPIO_' + pin + '_RW_MODE'], 2)
                    self.assertEqual(config['CONFIG_GPIO_' + pin + '_MODE'], 1)
                    self.assertEqual(config['CONFIG_GPIO_' + pin + '_INIT_VALUE'], inactive)
                    self.assertFalse(config['CONFIG_STM32_SPI_ENABLE'])
                    self.assertTrue(config['CONFIG_STM32_UART_ENABLE'])
                    self.assertTrue(config['CONFIG_INSTANCE_STM32_UART_0'])

    def test_build_mode_conflict_fails(self):
        self.fragment.write_text('CONFIG_PLATFORM_NATIVE=y\nCONFIG_BUILD_TYPE_RELEASE=y\n')
        self.assertNotEqual(self.generate('--set', 'BUILD_TYPE_DEBUG=y').returncode, 0)

    def test_required_backend_features_and_physical_layout_cannot_be_overridden(self):
        baseline = (ROOT / 'configs/stm32f407_freertos_defconfig').read_text()
        for setting in ('CONFIG_FREERTOS_USE_PREEMPTION=n',
                        'CONFIG_FREERTOS_USE_TIMERS=n',
                        'CONFIG_LINKER_RAM_SIZE=0x80000'):
            self.fragment.write_text(baseline + setting + '\n')
            self.assertNotEqual(self.generate().returncode, 0, setting)
            self.assertFalse((self.work / 'nexus_config.h').exists())

    def test_timer_priority_rejects_the_priority_count_as_an_index(self):
        baseline = (ROOT / 'configs/stm32f407_freertos_defconfig').read_text()
        self.fragment.write_text(baseline + 'CONFIG_OSAL_MAX_PRIORITIES=8\n'
                                 'CONFIG_FREERTOS_TIMER_TASK_PRIORITY=8\n')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('must be smaller', result.stderr)
        self.assertFalse((self.work / 'nexus_config.h').exists())

    @unittest.skipUnless(shutil.which('cc'), 'A C compiler is required for the real header consumer')
    def test_freertos_header_consumes_the_effective_profile(self):
        baseline = (ROOT / 'configs/stm32f407_freertos_defconfig').read_text()
        self.fragment.write_text(baseline + 'CONFIG_OSAL_MAX_PRIORITIES=16\n'
                                 'CONFIG_OSAL_HEAP_SIZE=24576\n'
                                 'CONFIG_FREERTOS_TIMER_TASK_PRIORITY=5\n'
                                 'CONFIG_FREERTOS_TIMER_QUEUE_LENGTH=17\n')
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        source = self.work / 'freertos_profile.c'
        source.write_text('#include "FreeRTOSConfig.h"\n'
                          '_Static_assert(configMAX_PRIORITIES == 16, "priority count");\n'
                          '_Static_assert(configTOTAL_HEAP_SIZE == 24576, "heap budget");\n'
                          '_Static_assert(configTIMER_TASK_PRIORITY == 5, "timer priority");\n'
                          '_Static_assert(configTIMER_QUEUE_LENGTH == 17, "timer queue");\n'
                          '_Static_assert(configTICK_RATE_HZ == 1000, "shared HAL clock");\n'
                          '_Static_assert(configUSE_PREEMPTION == 1 && configUSE_TIMERS == 1, "required kernel features");\n')
        result = subprocess.run([shutil.which('cc'), '-std=c11', '-Werror', '-DNEXUS_EFFECTIVE_CONFIG=1',
                                 '-I', str(self.work), '-I', str(ROOT / 'osal/adapters/freertos'),
                                 '-fsyntax-only', str(source)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_string_escaping_preserves_literal_content(self):
        # A hidden string can only equal its derived value; use a custom schema.
        schema = self.work / 'Kconfig'
        schema.write_text('config MESSAGE\n    string "Message"\n')
        self.fragment.write_text('CONFIG_MESSAGE="quote\\\";${unsafe}\\\\path"\n')
        result = self.generate('--kconfig', str(schema))
        self.assertEqual(result.returncode, 0, result.stderr)
        header = (self.work / 'nexus_config.h').read_text()
        self.assertIn('"quote\\\";${unsafe}\\\\path"', header)


@unittest.skipUnless(shutil.which('cmake'), 'CMake is required for integration checks')
class CMakeConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)

    def configure(self, directory, *settings):
        return subprocess.run(['cmake', '-S', str(ROOT), '-B', str(directory),
                               '-DNEXUS_BUILD_TESTS=OFF', *settings],
                              text=True, capture_output=True)

    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('gcc') and
                         shutil.which('g++') and shutil.which('ninja'),
                         'The Native GCC release profile requires Linux, GCC and Ninja')
    def test_actual_cmake_release_bundle_passes_the_release_provenance_gate(self):
        directory = self.work / 'release'
        result = subprocess.run(['cmake', '--preset', 'linux-gcc-release', '-B', str(directory)],
                                cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        contents = {name: (directory / 'generated' / name).read_text() for name in
                    ('effective.config', 'nexus_config.h', 'config.cmake')}
        config = validate_configuration_bundle(contents)
        cache = cmake_cache(directory / 'CMakeCache.txt')
        validate_effective_build(cache, RELEASE_PROFILES['linux-gcc-release'], 'Release', config)
        self.assertIs(config['CONFIG_BUILD_TESTS'], True)
        self.assertIs(config['CONFIG_ENABLE_COVERAGE'], False)
        self.assertIs(config['CONFIG_ENABLE_SANITIZERS'], False)
        # A valid header and target graph cannot substitute for absent release metadata.
        del config['CONFIG_ENABLE_COVERAGE']
        with self.assertRaisesRegex(ReleaseError, 'Missing effective build option'):
            validate_effective_build(cache, RELEASE_PROFILES['linux-gcc-release'], 'Release', config)

    def test_separate_modes_and_stale_cache_cleanup(self):
        for mode in ('Debug', 'Release'):
            directory = self.work / mode
            result = self.configure(directory, f'-DCMAKE_BUILD_TYPE={mode}', '-DCONFIG_STALE_PLATFORM=ON')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            effective = (directory / 'generated/effective.config').read_text()
            header = (directory / 'generated/nexus_config.h').read_text()
            self.assertIn(f'CONFIG_BUILD_TYPE="{mode}"', effective)
            self.assertIn(f'#define NX_CONFIG_BUILD_TYPE "{mode}"', header)
            self.assertNotIn('CONFIG_STALE_PLATFORM', (directory / 'CMakeCache.txt').read_text())
        # Verify actual compiler consumers select the per-build header.
        compiler = shutil.which('cc')
        source = self.work / 'mode.c'
        source.write_text('#include "nexus_config.h"\nconst char *mode = NX_CONFIG_BUILD_TYPE;\n')
        for mode in ('Debug', 'Release'):
            result = subprocess.run([compiler, '-E', '-P', '-I', str(self.work / mode / 'generated'), str(source)], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(f'const char *mode = "{mode}";', result.stdout)

    def test_platform_mismatch_fails_closed(self):
        fragment = self.work / 'stm32.config'
        fragment.write_text('CONFIG_PLATFORM_STM32=y\n')
        result = self.configure(self.work / 'build', f'-DNEXUS_CONFIG_FILE={fragment}')
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue('Platform conflict' in result.stderr or 'cannot be honored' in result.stderr, result.stderr)

    @unittest.skipUnless(shutil.which('ninja'), 'Ninja is required for multi-config checks')
    def test_multi_config_cannot_compile_a_different_mode(self):
        directory = self.work / 'multi'
        result = self.configure(directory, '-G', 'Ninja Multi-Config', '-DCMAKE_BUILD_TYPE=Release')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run(['cmake', '--build', str(directory), '--config', 'Release', '--target', 'osal'],
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run(['cmake', '--build', str(directory), '--config', 'Debug', '--target', 'osal'],
                                text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)

    def test_generation_failure_stops_reconfigure(self):
        fragment = self.work / 'native.config'
        fragment.write_text('CONFIG_PLATFORM_NATIVE=y\nCONFIG_OSAL_NATIVE=y\n')
        directory = self.work / 'build'
        self.assertEqual(self.configure(directory, f'-DNEXUS_CONFIG_FILE={fragment}').returncode, 0)
        previous = (directory / 'generated/nexus_config.h').read_bytes()
        fragment.write_text('CONFIG_NOT_A_REAL_FEATURE=y\n')
        result = self.configure(directory, f'-DNEXUS_CONFIG_FILE={fragment}')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Kconfig generation failed', result.stderr)
        self.assertEqual(previous, (directory / 'generated/nexus_config.h').read_bytes())


@unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('cmake') and
                     shutil.which('cc') and shutil.which('nm') and shutil.which('readelf'),
                     'Linux CMake, GNU-compatible compiler and ELF tools are required')
class ApplicationTargetTests(unittest.TestCase):
    """Link real host ELF fixtures through nested platform/application scopes.

    The fixture checks target ownership and ELF entry points, not ARM execution.
    """

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)
        self.source = self.work / 'source'
        self.build = self.work / 'build'
        for directory in ('platforms/stm32', 'applications'):
            (self.source / directory).mkdir(parents=True)
        self.layout = self.source / 'platforms/stm32/layout.ld'
        self.layout.write_text(self.layout_at(0x10000))
        (self.source / 'platforms/stm32/startup.s').write_text(
            '.text\n.globl Reset_Handler\nReset_Handler:\n.byte 0\n'
            '.section .note.GNU-stack,"",@progbits\n')
        (self.source / 'platforms/stm32/dummy.c').write_text('int platform_dummy;\n')
        (self.source / 'applications/main.c').write_text('int main(void) { return 0; }\n')
        (self.source / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.21)\n'
            'project(platform_scope_probe C ASM)\n'
            'set(NEXUS_PLATFORM stm32)\n'
            'set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")\n'
            'set(CONFIG_FIRMWARE_MAIN_STACK_SIZE 0x1000)\nset(CONFIG_FIRMWARE_LIBC_HEAP_SIZE 0x2000)\n'
            'add_library(hal INTERFACE)\nadd_library(osal INTERFACE)\n'
            'add_library(Nexus::HAL ALIAS hal)\nadd_library(Nexus::OSAL ALIAS osal)\n'
            'add_library(nexus_build_options INTERFACE)\n'
            'add_library(Nexus::Config ALIAS nexus_build_options)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/config.cmake" '
            '"set(CONFIG_FIRMWARE_MAIN_STACK_SIZE 0x1000)\\nset(CONFIG_FIRMWARE_LIBC_HEAP_SIZE 0x2000)\\n")\n'
            'set_target_properties(nexus_build_options PROPERTIES '
            'NEXUS_PLATFORM stm32 NEXUS_PLATFORM_TARGET platform_stm32 '
            'NEXUS_BINARY_DIR "${CMAKE_BINARY_DIR}" '
            'NEXUS_CONFIG_CMAKE "${CMAKE_BINARY_DIR}/config.cmake")\n'
            'add_subdirectory(platforms)\n'
            f'include("{(ROOT / "cmake/modules/NexusApplications.cmake").as_posix()}")\n'
            'add_subdirectory(applications)\n')
        (self.source / 'platforms/CMakeLists.txt').write_text('add_subdirectory(stm32)\n')
        (self.source / 'applications/CMakeLists.txt').write_text(
            'nexus_add_application(TARGET link_probe SOURCES main.c)\n'
            'target_compile_options(link_probe PRIVATE -ffreestanding -fno-pie '
            '-fno-asynchronous-unwind-tables -fno-stack-protector)\n'
            'target_link_options(link_probe PRIVATE -nostdlib -no-pie -Wl,--build-id=none)\n')

    @staticmethod
    def layout_at(address):
        return ('ENTRY(Reset_Handler)\nSECTIONS { '
                f'. = 0x{address:x}; '
                '.text : { *(.text .text.*) } '
                '/DISCARD/ : { *(.note*) *(.eh_frame*) } }\n')

    def configure(self, defect=''):
        source = 'dummy.c' if defect == 'unowned_startup' else 'startup.s'
        startup = '' if defect == 'missing_startup' else '${CMAKE_CURRENT_SOURCE_DIR}/startup.s'
        script = '' if defect == 'missing_layout' else '${CMAKE_CURRENT_SOURCE_DIR}/layout.ld'
        (self.source / 'platforms/stm32/CMakeLists.txt').write_text(
            f'add_library(platform_stm32 OBJECT {source})\n'
            # SOURCES must carry the same absolute startup identity as the property.
            'get_target_property(_sources platform_stm32 SOURCES)\n'
            'list(TRANSFORM _sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")\n'
            'set_property(TARGET platform_stm32 PROPERTY SOURCES "${_sources}")\n'
            'set_target_properties(platform_stm32 PROPERTIES '
            f'NEXUS_STARTUP_SOURCE "{startup}" NEXUS_LINKER_SCRIPT "{script}")\n'
            'target_link_options(platform_stm32 INTERFACE "-T${CMAKE_CURRENT_SOURCE_DIR}/layout.ld")\n'
            'set_property(TARGET platform_stm32 PROPERTY INTERFACE_LINK_DEPENDS '
            '"${CMAKE_CURRENT_SOURCE_DIR}/layout.ld")\n')
        return subprocess.run(['cmake', '-S', str(self.source), '-B', str(self.build)],
                              text=True, capture_output=True)

    def build_probe(self):
        result = subprocess.run(['cmake', '--build', str(self.build), '--target', 'link_probe'],
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        elf = self.build / 'bin/link_probe.elf'
        self.assertTrue(elf.is_file())
        symbols = subprocess.run(['nm', '--defined-only', str(elf)],
                                 text=True, capture_output=True, check=True).stdout
        reset = next(int(line.split()[0], 16) for line in symbols.splitlines()
                     if line.split()[-1] == 'Reset_Handler')
        header = subprocess.run(['readelf', '-h', str(elf)],
                                text=True, capture_output=True, check=True).stdout
        entry = next(int(line.split(':', 1)[1].strip(), 16)
                     for line in header.splitlines() if 'Entry point address:' in line)
        self.assertEqual(entry, reset)
        self.assertTrue((self.build / 'bin/link_probe.map').is_file())
        return entry

    def test_nested_platform_owns_startup_and_layout_in_actual_elf(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.build_probe()

    def test_missing_or_uncompiled_startup_and_layout_fail_configuration(self):
        for defect in ('missing_startup', 'missing_layout', 'unowned_startup'):
            with self.subTest(defect=defect):
                result = self.configure(defect)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('platform_stm32', result.stderr)
                self.assertFalse((self.build / 'bin/link_probe.elf').exists())

    def test_platform_layout_change_relinks_elf_entry_without_source_edits(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        first = self.build_probe()
        self.layout.write_text(self.layout_at(0x20000))
        self.assertEqual(self.build_probe() - first, 0x10000)


if __name__ == '__main__':
    unittest.main()
