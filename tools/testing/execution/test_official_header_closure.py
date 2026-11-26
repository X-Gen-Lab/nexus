"""Exact official header closures preserve pins, notices and SDK inputs."""

import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT))
from tools.dev.process import run

F4_HAL_HEADERS = {
    'Inc/Legacy/stm32_hal_legacy.h', 'Inc/stm32f4xx_hal_def.h',
    *(f'Inc/stm32f4xx_hal_{unit}.h' for unit in (
        'dma', 'dma_ex', 'flash', 'flash_ex', 'flash_ramfunc', 'gpio',
        'gpio_ex', 'rcc', 'rcc_ex', 'uart', 'dcmi', 'dcmi_ex', 'hcd',
        'pcd', 'pcd_ex')),
    'Inc/stm32f4xx_hal.h', 'Inc/stm32f4xx_ll_usb.h'}
F4_HAL_SOURCES = {f'Src/stm32f4xx_hal_{unit}.c'
                  for unit in ('uart', 'dma', 'rcc', 'dcmi', 'hcd')}
F4_LL_SOURCES = {f'Src/stm32f4xx_ll_{unit}.c'
                 for unit in ('fsmc', 'sdmmc', 'usb')}
F4_NEW_IP_FILES = {
    'Inc/stm32f4xx_hal_cryp.h',
    'Inc/stm32f4xx_hal_dma2d.h',
    'Inc/stm32f4xx_hal_dsi.h',
    'Inc/stm32f4xx_hal_hash.h',
    'Inc/stm32f4xx_hal_hash_ex.h',
    'Inc/stm32f4xx_hal_ltdc.h',
    'Inc/stm32f4xx_hal_ltdc_ex.h',
    'Inc/stm32f4xx_hal_pwr.h',
    'Inc/stm32f4xx_hal_pwr_ex.h',
    'Inc/stm32f4xx_hal_qspi.h',
    'Inc/stm32f4xx_hal_sai.h',
    'Inc/stm32f4xx_hal_sai_ex.h',
    'Inc/stm32f4xx_ll_fmc.h',
    'Src/stm32f4xx_hal_cryp.c',
    'Src/stm32f4xx_hal_dma2d.c',
    'Src/stm32f4xx_hal_gpio.c',
    'Src/stm32f4xx_hal_hash.c',
    'Src/stm32f4xx_hal_ltdc.c',
    'Src/stm32f4xx_hal_qspi.c',
    'Src/stm32f4xx_hal_rcc_ex.c',
    'Src/stm32f4xx_hal_sai.c',
    'Src/stm32f4xx_hal_sai_ex.c',
    'Src/stm32f4xx_ll_fmc.c',
}
F4_REQUIRED_FILES = {
    'LICENSE.md', 'Inc/stm32f4xx_hal_eth.h', *F4_HAL_HEADERS,
    *F4_HAL_SOURCES, *F4_LL_SOURCES, *F4_NEW_IP_FILES,
    *(f'Inc/stm32f4xx_ll_{unit}.h' for unit in
                      ('gpio', 'usart', 'spi', 'adc', 'dac', 'dma', 'tim',
                       'iwdg', 'wwdg', 'i2c', 'bus', 'rcc', 'rng', 'fsmc', 'sdmmc', 'rtc', 'pwr',
                       'crc', 'utils', 'system'))}
F4_SELECTED_API = 'stm32f4-reviewed-hal-ll-controllers'


class OfficialHeaderClosureTests(unittest.TestCase):
    def test_f4_closes_exact_headers_and_reviewed_controller_sources(self):
        from tools.maintenance import stm32f4_hal_lock as f4
        additions = {
            'Inc/stm32f4xx_hal_eth.h': {
                'bytes': 89154,
                'sha256': '596e6eb83351a5cb3cad97750f2c7536069c278bb58fc52ccce74a0bfc34beb5'},
            'Inc/stm32f4xx_hal_def.h': {
                'bytes': 7563,
                'sha256': 'c8eb3f7b393f8c5a0c32e16ccfa101c83ddb59e96bd42239b8a5c5d4da83e97f'},
            'Inc/Legacy/stm32_hal_legacy.h': {
                'bytes': 240833,
                'sha256': '8c2d2feb90e8ed88fe5756f3f1616a2b67aa1cd0858da8b130ed69607688c8c0'}}
        original = json.loads((ROOT / f4.LOCK_NAME).read_text())
        candidate = copy.deepcopy(original)
        candidate['files'].update(additions)
        candidate['selected_api'] = F4_SELECTED_API
        for name in F4_REQUIRED_FILES:
            data = subprocess.run(
                ['git', '-C', str(ROOT / f4.DIRECTORY), 'cat-file', 'blob',
                 f'{f4.COMMIT}:{name}'], capture_output=True, check=True).stdout
            candidate['files'][name] = {
                'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        expected = F4_REQUIRED_FILES
        self.assertEqual({name for name in expected if name.endswith('.c')},
                         F4_HAL_SOURCES | F4_LL_SOURCES |
                         {name for name in F4_NEW_IP_FILES if name.endswith('.c')})
        self.assertEqual(original['selected_api'], F4_SELECTED_API)
        self.assertEqual(f4.REVIEWED_FILES, expected)
        self.assertEqual(f4.contract(candidate), expected)
        for name in expected:
            self.assertEqual(original['files'][name], candidate['files'][name])
            reduced = copy.deepcopy(candidate)
            del reduced['files'][name]
            with self.subTest(missing=name), self.assertRaises(ValueError):
                f4.contract(reduced)
        for unit in ('eth', 'sd', 'nand', 'nor', 'sram', 'rtc', 'pwr'):
            name = f'Src/stm32f4xx_hal_{unit}.c'
            unsafe = copy.deepcopy(candidate)
            unsafe['files'][name] = {'bytes': 1, 'sha256': '0' * 64}
            with self.subTest(extra=name), self.assertRaises(ValueError):
                f4.contract(unsafe)
        with tempfile.TemporaryDirectory() as temporary:
            lock = Path(temporary) / 'f4.lock.json'
            lock.write_text(json.dumps(candidate))
            result = f4.verify(ROOT / f4.DIRECTORY, lock,
                               ROOT / f4.CMSIS_DIRECTORY)
            self.assertEqual(result['verified_files'], len(expected))
            self.assertFalse(result['software_qualified'])


    def test_f4_exported_headers_compile_vendor_and_nexus_transports(self):
        from tools.maintenance import stm32f4_hal_lock as f4
        compiler = (os.environ.get('NEXUS_ARM_GCC') or
                    shutil.which('arm-none-eabi-gcc'))
        self.assertIsNotNone(compiler, 'Prepared ARM compiler required')
        spec = importlib.util.spec_from_file_location(
            'nexus_package_for_f4_hal_source_closure',
            ROOT / 'cmake/package/package_source_sdk.py')
        module = importlib.util.module_from_spec(spec)
        sys.path.insert(0, str(ROOT / 'cmake/package'))
        spec.loader.exec_module(module)
        selected = module.dependency_entries(ROOT, f4.DIRECTORY)
        self.assertEqual(set(selected), F4_REQUIRED_FILES)
        with tempfile.TemporaryDirectory(
                prefix='nexus-f4-hal-closure-') as temporary:
            retained = os.environ.get('NEXUS_HAL_CLOSURE_TEST_ROOT')
            root = (Path(retained) / self._testMethodName if retained
                    else Path(temporary))
            root.mkdir(parents=True, exist_ok=True)
            exported = root / 'exported'
            for name in selected:
                path = exported / name
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / f4.DIRECTORY / name, path)
            private = root / 'private'
            private.mkdir()
            (private / 'stm32f4xx_hal_conf.h').write_text('''
#define HAL_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_DCMI_MODULE_ENABLED
#define HAL_HCD_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HSE_VALUE 8000000U
#define HSI_VALUE 16000000U
#define LSE_VALUE 32768U
#define EXTERNAL_CLOCK_VALUE 12288000U
#define HSE_STARTUP_TIMEOUT 100U
#define LSE_STARTUP_TIMEOUT 5000U
#define USE_HAL_UART_REGISTER_CALLBACKS 1U
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_uart.h"
#include "stm32f4xx_hal_dcmi.h"
#include "stm32f4xx_hal_hcd.h"
#include "stm32f4xx_hal_pcd.h"
#include "stm32f4xx_hal_pcd_ex.h"
#define assert_param(expr) ((void)0U)
''')
            flags = ['-std=c11', '-mcpu=cortex-m4', '-mthumb',
                     '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-Os',
                     '-DSTM32F407xx', '-DUSE_HAL_DRIVER', '-ffunction-sections',
                     '-fdata-sections', '-Werror=implicit-function-declaration']
            includes = [private, exported / 'Inc',
                        ROOT / f4.CMSIS_DIRECTORY / 'Include',
                        ROOT / 'vendors/arm/CMSIS_6/CMSIS/Core/Include']
            used = set()
            # Compile committed vendor sources against only exported HAL
            # headers, never the complete checkout's incidental include tree.
            for name in sorted(F4_HAL_SOURCES | F4_LL_SOURCES):
                source = ROOT / f4.DIRECTORY / name
                unit = source.stem
                dependencies = root / (unit + '.d')
                result = run([
                    compiler, *flags,
                    *[arg for path in includes for arg in ('-I', str(path))],
                    '-MMD', '-MF', str(dependencies), '-c', str(source),
                    '-o', str(root / (unit + '.o'))], cwd=root,
                    env=os.environ.copy(), log=root / (unit + '.log'),
                    timeout=60, stage='f4-exported-hal-source-closure')
                self.assertEqual(result.returncode, 0, result.stderr)
                dependency_text = dependencies.read_text().replace('\\\n', ' ')
                for token in shlex.split(dependency_text):
                    path = Path(token)
                    if path.is_relative_to(exported):
                        used.add(path.relative_to(exported).as_posix())
            self.assertEqual(used, F4_HAL_HEADERS)

            # Compile real Nexus transports with the sealed supplier graph.
            # The checkout's complete HAL Inc directory must never mask gaps.
            configuration = root / 'configuration'
            configured = run([
                sys.executable, '-B', str(ROOT / 'tools/configure/configure.py'),
                '--source-root', str(ROOT), '--assembly',
                str(ROOT / 'tests/integration/assemblies/dac_assembly/ve-dma.toml'),
                '--output', str(configuration)], cwd=ROOT,
                env=os.environ.copy(), log=root / 'configure.log',
                timeout=60, stage='f4-exported-nexus-configuration')
            self.assertEqual(configured.returncode, 0, configured.stderr)
            owned_includes = [private, configuration,
                ROOT / 'soc/stm32/stm32f4', ROOT / 'io/private',
                ROOT / 'io/include', ROOT / 'core/include', ROOT / 'arch/include']
            system_includes = includes[1:]
            owned_flags = [flag for flag in flags if flag != '-DUSE_HAL_DRIVER']
            owned_flags += ['-DCMSIS_DISABLE_DEPRECATED=1', '-fshort-enums',
                            '-Wall', '-Wextra', '-Werror', '-Wpedantic',
                            '-Wconversion', '-Wshadow', '-Wstrict-prototypes',
                            '-Wmissing-prototypes']
            checkout_headers = (ROOT / f4.DIRECTORY / 'Inc').resolve()
            exported_headers = exported.resolve()
            for unit, relative in (('dac_dma', 'dac/dac_dma.c'),
                                   ('crc', 'crc/crc.c'), ('chip', 'identity/chip.c')):
                source = ROOT / 'soc/stm32/stm32f4/drivers' / relative
                dependencies = root / (unit + '.d')
                result = run([
                    compiler, *owned_flags,
                    *[arg for path in owned_includes for arg in ('-I', str(path))],
                    *[arg for path in system_includes
                      for arg in ('-isystem', str(path))],
                    '-MD', '-MF', str(dependencies), '-c', str(source),
                    '-o', str(root / (unit + '.o'))], cwd=root,
                    env=os.environ.copy(), log=root / (unit + '.log'),
                    timeout=60, stage='f4-exported-nexus-source-closure')
                self.assertEqual(result.returncode, 0, result.stderr)
                dependency_text = dependencies.read_text().replace('\\\n', ' ')
                for token in shlex.split(dependency_text):
                    path = Path(token)
                    path = (path if path.is_absolute() else root / path).resolve()
                    self.assertFalse(path.is_relative_to(checkout_headers))
                    if path.is_relative_to(exported_headers):
                        used.add(path.relative_to(exported_headers).as_posix())
            self.assertLessEqual({f'Inc/stm32f4xx_ll_{unit}.h' for unit in
                                  ('dma', 'dac', 'tim', 'crc', 'system', 'utils')},
                                 used)



if __name__ == '__main__':
    unittest.main()
