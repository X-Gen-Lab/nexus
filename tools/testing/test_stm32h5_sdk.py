"""Fixed H563 official-source provenance and actual compiler closure."""
from copy import deepcopy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.maintenance import stm32h5_device_lock as device
from tools.maintenance import stm32h5_hal_lock as hal


ROOT = Path(__file__).resolve().parents[2]


class H5SourceContractTests(unittest.TestCase):
    def setUp(self):
        self.device_lock = json.loads((ROOT / device.LOCK_NAME).read_text())
        self.hal_lock = json.loads((ROOT / hal.LOCK_NAME).read_text())

    def test_reviewed_tuple_and_original_notices_are_exact(self):
        self.assertEqual(device.contract(self.device_lock), device.FILES)
        self.assertEqual(hal.contract(self.hal_lock), hal.PROFILE.selected_files)
        self.assertEqual(self.device_lock['selected_part'], 'STM32H563xx')
        self.assertEqual(hal.PROFILE.pairing['cmsis_commit'], device.COMMIT)

    def test_device_revision_selector_license_and_closure_reject(self):
        for change in ('commit', 'selected_part', 'license', 'files'):
            with self.subTest(change=change):
                value = deepcopy(self.device_lock)
                if change == 'files':
                    value['files'].pop('Include/stm32h563xx.h')
                elif change == 'license':
                    value['license']['expression'] = 'MIT'
                else:
                    value[change] = 'unreviewed'
                with self.assertRaises(ValueError):
                    device.contract(value)

    def test_hal_pairing_and_selected_api_cannot_be_widened(self):
        for change in ('pairing', 'selected_api', 'files'):
            with self.subTest(change=change):
                value = deepcopy(self.hal_lock)
                if change == 'pairing':
                    value['pairing']['cmsis_commit'] = '0' * 40
                elif change == 'files':
                    value['files'].pop('Inc/stm32h5xx_ll_i3c.h')
                else:
                    value[change] = 'all-h5-physical-support'
                with self.assertRaises(ValueError):
                    hal.contract(value)

    def test_empty_identity_boolean_extent_and_unknown_fields_reject(self):
        for invalid in (0, True, -1):
            value = deepcopy(self.device_lock)
            value['files']['LICENSE.md']['bytes'] = invalid
            with self.assertRaises(ValueError):
                device.contract(value)
        value = deepcopy(self.device_lock)
        value['support_qualified'] = True
        with self.assertRaises(ValueError):
            device.contract(value)

    def test_actual_clean_organization_checkouts_verify(self):
        result = device.verify(ROOT / device.DIRECTORY,
                               ROOT / device.LOCK_NAME)
        self.assertFalse(result['software_qualified'])
        self.assertEqual(result['source_kind'], 'git_checkout')
        result = hal.verify(ROOT / hal.DIRECTORY, ROOT / hal.LOCK_NAME,
                            ROOT / hal.CMSIS_DIRECTORY)
        self.assertEqual(result['commit'], hal.PROFILE.commit)
        self.assertFalse(result['software_qualified'])

    def test_packaged_device_needs_explicit_commit_and_unchanged_bytes(self):
        with tempfile.TemporaryDirectory(prefix='nexus-h5-proof-') as value:
            root = Path(value)
            source = root / 'device'
            for name in device.FILES:
                target = source / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / device.DIRECTORY / name, target)
            lock = root / 'device.lock.json'
            lock.write_text(json.dumps(self.device_lock))
            with self.assertRaises(ValueError):
                device.verify(source, lock)
            with self.assertRaises(ValueError):
                device.verify(source, lock, packaged_commit='0' * 40)
            result = device.verify(source, lock,
                                   packaged_commit=device.COMMIT)
            self.assertEqual(result['source_kind'], 'installed_source_sdk')
            header = source / 'Include/stm32h563xx.h'
            header.write_bytes(header.read_bytes() + b'\n')
            with self.assertRaises(ValueError):
                device.verify(source, lock, packaged_commit=device.COMMIT)


class H5CompilerClosureTests(unittest.TestCase):
    def test_actual_c11_cpp17_hal_i3c_and_device_objects(self):
        gcc = shutil.which('arm-none-eabi-gcc')
        gxx = shutil.which('arm-none-eabi-g++')
        self.assertIsNotNone(gcc, 'Prepared locked ARM GCC is required')
        self.assertIsNotNone(gxx, 'Prepared locked ARM G++ is required')
        self.assertEqual(subprocess.check_output(
            [gcc, '-dumpfullversion'], text=True).strip(), '14.3.1')
        fixture = ROOT / 'tests/contracts/stm32h5_official'
        with tempfile.TemporaryDirectory(prefix='nexus-h5-compile-') as value:
            output = Path(value)
            common = ['-mcpu=cortex-m33', '-mthumb', '-mfloat-abi=soft',
                      '-DSTM32H563xx', '-O2', '-ffunction-sections',
                      '-fdata-sections', '-Wall', '-Wextra', '-Werror',
                      '-I', str(fixture)]
            for include in (ROOT / device.DIRECTORY / 'Include',
                            ROOT / hal.DIRECTORY / 'Inc',
                            ROOT / 'vendors/arm/CMSIS_6/CMSIS/Core/Include'):
                common += ['-isystem', str(include)]
            commands = [
                [gcc, '-std=c11', '-Wpedantic', *common, '-c',
                 str(fixture / 'h563_probe.c'), '-o', str(output / 'c11.o')],
                [gxx, '-x', 'c++', '-std=c++17', '-Wpedantic',
                 '-fno-exceptions', '-fno-rtti', *common, '-c',
                 str(fixture / 'h563_probe.c'), '-o', str(output / 'cpp17.o')],
                [gcc, '-std=c11', *common, '-c',
                 str(ROOT / hal.DIRECTORY / 'Src/stm32h5xx_hal_i3c.c'),
                 '-o', str(output / 'hal-i3c.o')],
                [gcc, '-std=c11', *common, '-c',
                 str(ROOT / device.DIRECTORY /
                     'Source/Templates/system_stm32h5xx.c'),
                 '-o', str(output / 'system.o')],
            ]
            records = []
            for command in commands:
                with self.subTest(source=command[-3], object=command[-1]):
                    result = subprocess.run(command, capture_output=True,
                                            text=True, timeout=60)
                    self.assertEqual(result.returncode, 0,
                                     result.stdout + result.stderr)
                    artifact = Path(command[-1])
                    self.assertGreater(artifact.stat().st_size, 0)
                    records.append({
                        'argv': command, 'exit_code': result.returncode,
                        'stdout': result.stdout, 'stderr': result.stderr,
                        'object': artifact.name,
                        'bytes': artifact.stat().st_size,
                        'sha256': hashlib.sha256(
                            artifact.read_bytes()).hexdigest(),
                    })
            retained = os.environ.get('NEXUS_H5_PROBE_OUTPUT')
            if retained:
                destination = Path(retained)
                destination.mkdir(parents=True, exist_ok=False)
                for artifact in output.iterdir():
                    shutil.copyfile(artifact, destination / artifact.name)
                report = {'kind': 'h563-official-compile-probes',
                          'scope': 'Objects only; no SoC/provider/HIL proof',
                          'commands': records,
                          'core_header_sha256': hashlib.sha256((ROOT /
                              'vendors/arm/CMSIS_6/CMSIS/Core/Include/'
                              'core_cm33.h').read_bytes()).hexdigest()}
                (destination / 'probes.json').write_text(
                    json.dumps(report, indent=2, sort_keys=True) + '\n')


class H5SDKPayloadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        specification = importlib.util.spec_from_file_location(
            'h563_source_sdk_contract',
            ROOT / 'cmake/package/package_source_sdk.py')
        cls.sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(cls.sdk)

    def test_vendor_selection_is_only_the_reviewed_source_closure(self):
        for directory, expected in (
                (device.DIRECTORY, device.FILES),
                (hal.DIRECTORY, hal.PROFILE.selected_files)):
            with self.subTest(directory=directory):
                actual = self.sdk.dependency_entries(ROOT, directory)
                self.assertEqual(set(actual), expected)

    def test_required_payload_closes_vendor_sources_and_verifier_graph(self):
        tools = {device.LOCK_NAME, hal.LOCK_NAME,
                 'tools/maintenance/stm32h5_device_lock.py',
                 'tools/maintenance/stm32h5_hal_lock.py'}
        required = tools | {device.DIRECTORY + '/' + name
                            for name in device.FILES}
        required |= {hal.DIRECTORY + '/' + name
                     for name in hal.PROFILE.selected_files}
        self.assertFalse(required - set(self.sdk.REQUIRED_FILES))
        self.assertTrue(all(self.sdk.selected(name) for name in tools))

    def test_h5_hal_participates_in_official_verification(self):
        self.assertIn(hal.DIRECTORY,
                      {item.DIRECTORY for item in self.sdk.OFFICIAL_DRIVERS})


class H5RehashedOmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        specification = importlib.util.spec_from_file_location(
            'h563_actual_sdk_fixture',
            ROOT / 'tests/contracts/test_source_sdk.py')
        fixture = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(fixture)
        cls.run_process = staticmethod(fixture.SourceSDKTests.run_process)
        cls.run_ok = classmethod(fixture.SourceSDKTests.run_ok.__func__)
        cls.verify = fixture.SourceSDKTests.verify
        fixture.SourceSDKTests.setUpClass.__func__(cls)

    def test_rehashed_device_and_hal_omissions_fail_closed(self):
        baseline = self.verify()
        self.assertEqual(baseline.returncode, 0, baseline.stderr)
        identity_path = self.sdk / '.nexus-source-sdk.json'
        original_identity = identity_path.read_bytes()
        names = (device.DIRECTORY + '/Include/stm32h563xx.h',
                 device.DIRECTORY + '/LICENSE.md', device.LOCK_NAME,
                 'tools/maintenance/stm32h5_device_lock.py',
                 hal.DIRECTORY + '/Inc/stm32h5xx_ll_i3c.h',
                 hal.DIRECTORY + '/Src/stm32h5xx_hal_i3c.c',
                 hal.DIRECTORY + '/LICENSE.md', hal.LOCK_NAME,
                 'tools/maintenance/stm32h5_hal_lock.py')
        for name in names:
            with self.subTest(source=name):
                target = self.sdk / name
                original = target.read_bytes()
                try:
                    target.unlink()
                    identity = json.loads(original_identity)
                    identity['files_sha256'].pop(name)
                    identity['snapshot_sha256'] = hashlib.sha256(json.dumps(
                        identity['files_sha256'], sort_keys=True,
                        separators=(',', ':')).encode()).hexdigest()
                    identity_path.write_text(json.dumps(identity) + '\n')
                    rejected = self.verify()
                    self.assertNotEqual(rejected.returncode, 0,
                                        'Rehashed H5 omission accepted: ' + name)
                finally:
                    target.write_bytes(original)
                    identity_path.write_bytes(original_identity)


if __name__ == '__main__':
    unittest.main()
