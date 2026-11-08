"""Retained H563 vendor inventory preserves official-source provenance."""
from copy import deepcopy
import json
from pathlib import Path
import tempfile
import unittest

from tools.maintenance import stm32h5_device_lock as device
from tools.maintenance import stm32h5_hal_lock as hal
from tools.testing.inventory_fixtures import write_files


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

    def test_packaged_device_needs_explicit_commit_and_unchanged_bytes(self):
        with tempfile.TemporaryDirectory(prefix='nexus-h5-proof-') as value:
            root = Path(value)
            source = root / 'device'
            write_files(source, self.device_lock)
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


if __name__ == '__main__':
    unittest.main()
