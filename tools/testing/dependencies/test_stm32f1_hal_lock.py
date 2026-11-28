"""STM32F1 supplier admission verifies real locked bytes for both sources."""
import json
from pathlib import Path
import shutil
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from tools.maintenance import cmsis_device_lock as device_lock
from tools.maintenance import stm32f1_hal_lock as driver_lock

ROOT = Path(__file__).resolve().parents[3]


class Stm32F1PairTests(unittest.TestCase):
    def setUp(self):
        copied = TemporaryDirectory(prefix="nexus f1 supplier ")
        self.addCleanup(copied.cleanup)
        self.root = Path(copied.name)
        self.hal = self.root / driver_lock.DIRECTORY
        self.cmsis = self.root / driver_lock.CMSIS_DIRECTORY
        for lock, directory in ((driver_lock.LOCK_NAME, driver_lock.DIRECTORY),
                                (driver_lock.CMSIS_LOCK_NAME,
                                 driver_lock.CMSIS_DIRECTORY)):
            data = json.loads((ROOT / lock).read_bytes())
            for name in (lock, *(directory + "/" + member
                                 for member in data["files"])):
                target = self.root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, target)

    def verify_pair(self, cmsis_commit=driver_lock.CMSIS_COMMIT, **options):
        return driver_lock.verify(self.hal, self.root / driver_lock.LOCK_NAME,
                                  self.cmsis, packaged_commit=driver_lock.COMMIT,
                                  packaged_cmsis_commit=cmsis_commit, **options)

    def test_exact_pair_verifies_both_file_closures(self):
        evidence = self.verify_pair()
        expected = sum(len(json.loads((self.root / lock).read_bytes())["files"])
                       for lock in (driver_lock.LOCK_NAME,
                                    driver_lock.CMSIS_LOCK_NAME))
        self.assertEqual(evidence["verified_files"], expected)
        self.assertIn(str(self.root / driver_lock.CMSIS_LOCK_NAME),
                      evidence["configure_inputs"])

    def test_packaged_pair_rejects_wrong_device_commit(self):
        with self.assertRaisesRegex(ValueError, "both paired commits"):
            self.verify_pair("0" * 40)

    def test_family_device_lock_rejects_partial_header_startup_pair(self):
        path = self.root / driver_lock.CMSIS_LOCK_NAME
        original = json.loads(path.read_bytes())
        for member in ("Include/stm32f103xe.h",
                       "Source/Templates/gcc/startup_stm32f103xe.s"):
            with self.subTest(member=member):
                changed = dict(original, files=dict(original["files"]))
                del changed["files"][member]
                path.write_text(json.dumps(changed))
                with self.assertRaisesRegex(ValueError, "reviewed device graph"):
                    self.verify_pair()

    def test_both_selected_files_required_even_if_optional_otherwise(self):
        path = self.root / driver_lock.CMSIS_LOCK_NAME
        original = json.loads(path.read_bytes())
        selected = ("Include/stm32f103xe.h",
                    "Source/Templates/gcc/startup_stm32f103xe.s")
        self.verify_pair(selected_device_files=selected)
        for member in selected:
            del original["files"][member]
        path.write_text(json.dumps(original))
        self.verify_pair()
        with self.assertRaisesRegex(ValueError, "header and startup must be locked"):
            self.verify_pair(selected_device_files=selected)

    def test_rejects_device_bytes_changed_after_individual_device_proof(self):
        part = self.cmsis / "Include/stm32f103xb.h"
        verify_device = device_lock.verify

        def change_after_verification(*args, **kwargs):
            result = verify_device(*args, **kwargs)
            part.write_bytes(part.read_bytes() + b"\n")
            return result

        with patch.object(device_lock, "verify",
                          side_effect=change_after_verification):
            with self.assertRaisesRegex(ValueError, "changed during admission"):
                self.verify_pair()
