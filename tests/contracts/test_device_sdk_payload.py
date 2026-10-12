"""SDK device components retain exactly their reviewed upstream closure."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake/package"))
import package_source_sdk as sdk
from tools.maintenance import cmsis_device_lock


class DeviceSDKPayloadTests(unittest.TestCase):
    def test_full_upstream_checkout_exports_only_the_reviewed_five_files(self):
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                entries = sdk.dependency_entries(ROOT, review["directory"])
                self.assertEqual(len(entries), 5)
                self.assertEqual(set(entries), review["files"])
                self.assertTrue(all(mode in {"100644", "100755"}
                                    and kind == "blob"
                                    for mode, kind, _ in entries.values()))

    def test_original_notice_lock_and_verifier_cannot_be_omitted_from_payload(self):
        required = set(sdk.REQUIRED_FILES)
        self.assertTrue("tools/maintenance/cmsis_device_lock.py" in required,
                        "Missing generic device source verifier payload")
        for part, review in cmsis_device_lock.REVIEWED_DEVICES.items():
            with self.subTest(part=part):
                self.assertTrue(review["lock_name"] in required,
                                "Missing exact device source lock payload")
                for name in review["files"]:
                    self.assertTrue(review["directory"] + "/" + name in required,
                                    "Missing original reviewed device source: " + name)


if __name__ == "__main__":
    unittest.main()
