"""The source SDK exports only the reviewed optional lwIP source closure."""

import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class LwIPSDKTests(unittest.TestCase):
    def test_dependency_payload_is_the_reviewed_source_and_license_set(self):
        specification = importlib.util.spec_from_file_location(
            "lwip_sdk_package", ROOT / "cmake/package/package_source_sdk.py")
        module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(module)
        lock = json.loads((ROOT / "dependencies/lwip.lock.json").read_text())
        entries = module.dependency_entries(ROOT, "ext/lwip")
        self.assertEqual(set(entries), set(lock["files"]))
        self.assertIn("COPYING", entries)
        self.assertNotIn("Filelists.cmake", entries)
        self.assertTrue(all(kind == "blob" and mode in ("100644", "100755")
                            for mode, kind, _ in entries.values()))


if __name__ == "__main__":
    unittest.main()
