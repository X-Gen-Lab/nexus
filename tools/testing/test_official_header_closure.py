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

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT))
from tools.maintenance.stm32_ll_lock import PROFILES
from tools.dev.process import run

F4_HAL_HEADERS = {
    'Inc/Legacy/stm32_hal_legacy.h', 'Inc/stm32f4xx_hal_def.h',
    *(f'Inc/stm32f4xx_hal_{unit}.h' for unit in (
        'dma', 'dma_ex', 'flash', 'flash_ex', 'flash_ramfunc', 'gpio',
        'gpio_ex', 'rcc', 'rcc_ex', 'uart')),
    'Inc/stm32f4xx_hal.h'}
F4_HAL_SOURCES = {f'Src/stm32f4xx_hal_{unit}.c'
                  for unit in ('uart', 'dma', 'rcc')}
F4_REQUIRED_FILES = {
    'LICENSE.md', 'Inc/stm32f4xx_hal_eth.h', *F4_HAL_HEADERS,
    *F4_HAL_SOURCES, *(f'Inc/stm32f4xx_ll_{unit}.h' for unit in
                      ('gpio', 'usart', 'spi', 'adc', 'tim', 'iwdg', 'i2c'))}
F4_SELECTED_API = 'stm32f407-ll-inline-eth-definitions-uart-dma-hal-tx'

ADDITIONS = {
    'h7': {
        'Inc/stm32h7xx_ll_usart.h': {
            'bytes': 175243,
            'sha256': '0a739716ed496ee660c25a16b0e90a40bb83998fb83107fa778164dd6419403a'}},
    'u5': {
        'Inc/stm32u5xx_ll_rtc.h': {
            'bytes': 226081,
            'sha256': 'e40e46698164cd4c0722460fbb59c868be643ba0f7447e0bed439a88f3a63bf2'},
        'Inc/stm32u5xx_ll_dma.h': {
            'bytes': 322291,
            'sha256': '2cca143aaf883cffacd46e6a0a29a4a677badd485b7eb19f8da6199eceacead6'},
        'Inc/stm32u5xx_ll_usart.h': {
            'bytes': 177899,
            'sha256': '50a203a04e4b9688b95db3c806cb5af1a4c436b1608ea44e04d72205f354127b'}},
    'g4': {
        'Inc/stm32g4xx_hal_fdcan.h': {
            'bytes': 79977,
            'sha256': '085076ee896219babd0801c0f8c3a97b65e5ae2cb88c78f6b43124925deb1c56'},
        'Inc/stm32g4xx_hal_def.h': {
            'bytes': 6966,
            'sha256': 'adcb12d4eeb391241481a75f738f81dc7c186abe0ded173da3a1191803658eb7'},
        'Inc/Legacy/stm32_hal_legacy.h': {
            'bytes': 238453,
            'sha256': '97a63cd0d7e27f692d92f475179ea949751f790a08b7d80fb5f611792be98236'}}}
BASE_FILES = {
    'h7': {'LICENSE.md', 'Inc/stm32h7xx_ll_adc.h',
           'Inc/stm32h7xx_ll_spi.h'},
    'u5': {'LICENSE.md', *(f'Inc/stm32u5xx_ll_{unit}.h'
                          for unit in ('adc', 'spi', 'i2c', 'tim'))},
    'g4': {'LICENSE.md', 'Inc/stm32g4xx_ll_adc.h', 'Inc/stm32g4xx_ll_dmamux.h'}}


def proposed_lock(family):
    selected = json.loads((ROOT / PROFILES[family].lock_name).read_text())
    selected['files'].update(copy.deepcopy(ADDITIONS[family]))
    if family == 'g4':
        selected['selected_api'] = 'stm32g474-ll-inline-fdcan-definitions'
    return selected


class OfficialHeaderClosureTests(unittest.TestCase):
    def test_h7_exact_closed_graph_adds_only_usart_inline_header(self):
        profile = PROFILES['h7']
        expected = BASE_FILES['h7'] | ADDITIONS['h7'].keys()
        self.assertEqual(profile.selected_files, expected)
        self.assertEqual(profile.contract(proposed_lock('h7')), expected)

    def test_f4_selected_transport_closes_exact_headers_and_only_three_sources(self):
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
        self.assertEqual(f4.REVIEWED_FILES, expected)
        self.assertEqual(f4.contract(candidate), expected)
        for name in expected:
            self.assertEqual(original['files'][name], candidate['files'][name])
            reduced = copy.deepcopy(candidate)
            del reduced['files'][name]
            with self.subTest(missing=name), self.assertRaises(ValueError):
                f4.contract(reduced)
        unsafe = copy.deepcopy(candidate)
        unsafe['files']['Src/stm32f4xx_hal_eth.c'] = {
            'bytes': 1, 'sha256': '0' * 64}
        with self.assertRaises(ValueError):
            f4.contract(unsafe)
        with tempfile.TemporaryDirectory() as temporary:
            lock = Path(temporary) / 'f4.lock.json'
            lock.write_text(json.dumps(candidate))
            result = f4.verify(ROOT / f4.DIRECTORY, lock,
                               ROOT / f4.CMSIS_DIRECTORY)
            self.assertEqual(result['verified_files'], len(expected))
            self.assertFalse(result['software_qualified'])

    def test_f4_exported_headers_compile_real_uart_dma_and_rcc_sources(self):
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
            for unit in ('uart', 'dma', 'rcc'):
                source = ROOT / f4.DIRECTORY / f'Src/stm32f4xx_hal_{unit}.c'
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

    def test_u5_exact_closed_graph_adds_only_dma_usart_and_rtc(self):
        profile = PROFILES['u5']
        expected = BASE_FILES['u5'] | ADDITIONS['u5'].keys()
        self.assertEqual(profile.selected_files, expected)
        self.assertEqual(profile.contract(proposed_lock('u5')), expected)

    def test_u5_rtc_sdk_export_closes_actual_provider_and_cmsis_includes(self):
        import re
        header = 'Inc/stm32u5xx_ll_rtc.h'
        profile = PROFILES['u5']
        provider = (ROOT / 'soc/stm32/stm32u575/drivers/rtc.c').read_text()
        self.assertIn('#include "stm32u5xx_ll_rtc.h"', provider)
        spec = importlib.util.spec_from_file_location(
            'nexus_package_for_rtc_header_closure',
            ROOT / 'cmake/package/package_source_sdk.py')
        module = importlib.util.module_from_spec(spec)
        sys.path.insert(0, str(ROOT / 'cmake/package'))
        spec.loader.exec_module(module)
        selected = module.dependency_entries(ROOT, profile.directory)
        self.assertIn(header, selected)
        text = (ROOT / profile.directory / header).read_text()
        includes = set(re.findall(r'^#include "([^"]+)"', text, re.MULTILINE))
        self.assertEqual(includes, {'stm32u5xx.h'})
        device = module.dependency_entries(ROOT, profile.cmsis_directory)
        self.assertTrue({'Include/stm32u5xx.h', 'Include/stm32u575xx.h',
                         'Include/system_stm32u5xx.h'}.issubset(device))
        core = module.dependency_entries(ROOT, 'vendors/arm/CMSIS_6')
        self.assertIn('CMSIS/Core/Include/core_cm33.h', core)

    def test_g4_exact_closed_graph_adds_transitive_hal_definitions_only(self):
        profile = PROFILES['g4']
        expected = BASE_FILES['g4'] | ADDITIONS['g4'].keys()
        self.assertEqual(profile.selected_files, expected)
        self.assertEqual(profile.contract(proposed_lock('g4')), expected)

    def test_scope_is_exact_and_does_not_widen_other_families(self):
        for family, scope in (('g4', 'stm32g474-ll-inline-fdcan-definitions'),
                              ('u5', 'stm32u575-ll-inline'),
                              ('h7', 'stm32h743-ll-inline')):
            with self.subTest(family=family):
                profile = PROFILES[family]
                lock = proposed_lock(family) if family in ADDITIONS else json.loads(
                    (ROOT / profile.lock_name).read_text())
                self.assertEqual(lock['selected_api'], scope)
                profile.contract(lock)
                wrong = copy.deepcopy(lock)
                wrong['selected_api'] = 'unreviewed-headers-or-runtime'
                with self.assertRaises(ValueError):
                    profile.contract(wrong)

    def test_real_pinned_checkouts_verify_both_proposed_graphs(self):
        for family in ADDITIONS:
            with self.subTest(family=family), tempfile.TemporaryDirectory() as temp:
                profile = PROFILES[family]
                path = Path(temp) / 'proposed.lock.json'
                path.write_text(json.dumps(proposed_lock(family)))
                result = profile.verify(ROOT / profile.directory, path,
                                        ROOT / profile.cmsis_directory)
                self.assertEqual(result['verified_files'],
                                 len(BASE_FILES[family]) + len(ADDITIONS[family]))
                self.assertFalse(result['software_qualified'])

    def test_missing_transitive_header_or_extra_vendor_c_is_rejected(self):
        for family, additions in ADDITIONS.items():
            profile = PROFILES[family]
            baseline = proposed_lock(family)
            profile.contract(baseline)
            for name in additions:
                with self.subTest(family=family, missing=name):
                    candidate = copy.deepcopy(baseline)
                    del candidate['files'][name]
                    with self.assertRaises(ValueError):
                        profile.contract(candidate)
            candidate = copy.deepcopy(baseline)
            candidate['files'][f'Src/stm32{family}xx_hal.c'] = {
                'bytes': 1, 'sha256': '0' * 64}
            with self.subTest(family=family, extra='vendor-C'):
                with self.assertRaises(ValueError):
                    profile.contract(candidate)

    def test_installed_new_header_bytes_are_checked_after_exact_pair(self):
        for family, additions in ADDITIONS.items():
            profile = PROFILES[family]
            with self.subTest(family=family), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                driver, cmsis = root / 'driver', root / 'cmsis'
                driver.mkdir()
                cmsis.mkdir()
                lock = proposed_lock(family)
                path = root / 'lock.json'
                path.write_text(json.dumps(lock))
                for name in lock['files']:
                    destination = driver / name
                    destination.parent.mkdir(exist_ok=True, parents=True)
                    shutil.copyfile(ROOT / profile.directory / name, destination)
                result = profile.verify(driver, path, cmsis,
                    packaged_commit=profile.commit,
                    packaged_cmsis_commit=profile.cmsis_commit)
                self.assertEqual(result['source_kind'], 'installed_source_sdk')
                for name in additions:
                    original = (driver / name).read_bytes()
                    (driver / name).write_bytes(original + b'\nchanged\n')
                    with self.subTest(header=name), self.assertRaises(ValueError):
                        profile.verify(driver, path, cmsis,
                            packaged_commit=profile.commit,
                            packaged_cmsis_commit=profile.cmsis_commit)
                    (driver / name).write_bytes(original)

    def test_sdk_dependency_selection_requires_the_same_exact_headers(self):
        spec = importlib.util.spec_from_file_location(
            'nexus_package_for_official_header_closure',
            ROOT / 'cmake/package/package_source_sdk.py')
        self.assertIsNotNone(spec)
        module = importlib.util.module_from_spec(spec)
        sys.path.insert(0, str(ROOT / 'cmake/package'))
        spec.loader.exec_module(module)
        for family in ADDITIONS:
            with self.subTest(family=family):
                selected = module.dependency_entries(ROOT, PROFILES[family].directory)
                self.assertEqual(set(selected), BASE_FILES[family] |
                                 ADDITIONS[family].keys())


if __name__ == '__main__':
    unittest.main()
