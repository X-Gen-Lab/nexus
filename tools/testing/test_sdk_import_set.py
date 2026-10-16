"""Every maintained vendor import remains explicit in Source SDK provenance."""
from types import SimpleNamespace
import importlib.util
from pathlib import Path
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "nexus_sdk_import_contract", ROOT / "cmake/package/package_source_sdk.py")
sdk = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdk)


class SourceSDKImportSetTests(unittest.TestCase):
    def families(self):
        return (
            SimpleNamespace(import_lock=None),
            SimpleNamespace(import_lock="vendors/gigadevice/gd32f4xx/source.lock.json"),
            SimpleNamespace(import_lock="vendors/gigadevice/gd32f30x/source.lock.json"),
        )

    def identity(self):
        return {"source_tree": "a" * 40, "reviewed_imports": [
            {"path": "vendors/gigadevice/gd32f30x", "source_tree": "a" * 40},
            {"path": "vendors/gigadevice/gd32f4xx", "source_tree": "a" * 40},
        ]}

    def test_import_set_comes_from_maintained_descriptors(self):
        with mock.patch.object(sdk.provider_descriptors, "families",
                               return_value=self.families()):
            self.assertEqual(sdk.reviewed_import_directories(), (
                "vendors/gigadevice/gd32f30x", "vendors/gigadevice/gd32f4xx"))

    def test_all_reviewed_imports_are_packaged_without_adjacent_vendor_trees(self):
        with mock.patch.object(sdk.provider_descriptors, "families",
                               return_value=self.families()):
            for directory in sdk.reviewed_import_directories():
                self.assertTrue(sdk.selected(directory + "/source.lock.json"))
                self.assertTrue(sdk.selected(directory + "/Firmware/driver.h"))
                self.assertFalse(sdk.selected(directory + "-unreviewed/file.h"))
            self.assertFalse(sdk.selected("vendors/gigadevice/other/driver.h"))

    def test_complete_unordered_records_bind_the_same_owner_tree(self):
        with mock.patch.object(sdk.provider_descriptors, "families",
                               return_value=self.families()):
            identity = self.identity()
            self.assertEqual(sdk.validate_import_records(identity),
                             identity["reviewed_imports"])

    def test_missing_duplicate_foreign_and_changed_owner_records_reject(self):
        with mock.patch.object(sdk.provider_descriptors, "families",
                               return_value=self.families()):
            records = self.identity()["reviewed_imports"]
            invalid = [None, {}, [], records[:1], [records[0], records[0]],
                       records + [records[0]],
                       [{**records[0], "path": "vendors/unreviewed"}, records[1]],
                       [{**records[0], "source_tree": "b" * 40}, records[1]]]
            for value in invalid:
                with self.subTest(value=value), self.assertRaises(sdk.SDKError):
                    sdk.validate_import_records({**self.identity(),
                                                 "reviewed_imports": value})


if __name__ == "__main__":
    unittest.main()
