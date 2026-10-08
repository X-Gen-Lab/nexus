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

    def test_unknown_symbol_fails_without_artifacts(self):
        self.fragment.write_text('CONFIG_OBSOLETE_DRIVER=y\n')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unknown configuration symbol', result.stderr)
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
                               '-DNEXUS_BUILD_TESTS=OFF', '-DNEXUS_BUILD_EXAMPLES=OFF', *settings],
                              text=True, capture_output=True)

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


if __name__ == '__main__':
    unittest.main()
