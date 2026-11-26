"""Retained supplier inventory does not silently enter an installed SDK."""
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "nexus_sdk_import_contract", ROOT / "cmake/package/package_source_sdk.py")
sdk = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdk)


class SourceSDKImportSetTests(unittest.TestCase):
    def test_retained_vendor_inventory_is_not_a_runtime_import_set(self):
        self.assertEqual(sdk.reviewed_import_directories(), ())
        for directory in ("vendors/gigadevice/gd32f30x",
                          "vendors/gigadevice/gd32f4xx",
                          "vendors/st/cmsis_device_h7"):
            with self.subTest(directory=directory):
                self.assertFalse(sdk.selected(directory + "/source.lock.json"))
                self.assertFalse(sdk.selected(directory + "/Firmware/driver.h"))

    def test_optional_component_suppliers_stay_out_of_default_payload(self):
        from tools.maintenance.component_lock import IDENTIFIERS
        from tools.maintenance.source_catalogue import dependency
        for identifier in sorted(IDENTIFIERS):
            item = dependency(identifier)
            with self.subTest(dependency=identifier):
                self.assertFalse(sdk.selected(item.path + "/" + item.notice))
                lock = json.loads((ROOT / item.source_lock).read_text())
                entries = sdk.dependency_entries(ROOT, item.path)
                self.assertEqual(set(entries), set(lock["files"]))

    def test_import_records_reject_malformed_or_unselected_sources(self):
        identity = {"source_tree": "a" * 40, "reviewed_imports": []}
        self.assertEqual(sdk.validate_import_records(identity), [])
        foreign = {"path": "vendors/gigadevice/gd32f4xx",
                   "source_tree": "a" * 40}
        for value in (None, {}, [foreign], [foreign, foreign],
                      [{**foreign, "source_tree": "b" * 40}]):
            with self.subTest(value=value), self.assertRaises(sdk.SDKError):
                sdk.validate_import_records({**identity,
                                             "reviewed_imports": value})


if __name__ == "__main__":
    unittest.main()
