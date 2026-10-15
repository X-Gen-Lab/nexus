"""Behavior checks for SDK source identity, including untrusted lock paths."""

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from verify_vendor_source import VendorSourceError, verify


class VendorSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "sdk"
        self.source = self.root / "Firmware" / "driver.c"
        self.source.parent.mkdir(parents=True)
        self.source.write_bytes(b"/* Original vendor notice */\nint driver(void) { return 42; }\n")
        data = self.source.read_bytes()
        self.lock = {"vendor": "Example", "package": "Test SDK", "version": "1",
                     "files": [{"path": "Firmware/driver.c", "bytes": len(data),
                                "sha256": hashlib.sha256(data).hexdigest(),
                                "license": "BSD-3-Clause"}]}
        self.save()

    def save(self):
        (self.root / "source.lock.json").write_text(json.dumps(self.lock))

    def test_exact_import_passes(self):
        self.assertEqual(verify(self.root)["files"], 1)

    def test_changed_source_fails(self):
        self.source.write_bytes(self.source.read_bytes().replace(b"42", b"41"))
        with self.assertRaisesRegex(VendorSourceError, "changed"):
            verify(self.root)

    def test_missing_source_fails(self):
        self.source.unlink()
        with self.assertRaisesRegex(VendorSourceError, "missing"):
            verify(self.root)

    def test_unreviewed_source_fails(self):
        (self.source.parent / "extra.c").write_text("int injected;")
        with self.assertRaisesRegex(VendorSourceError, "outside"):
            verify(self.root)

    def test_traversal_and_absolute_paths_fail(self):
        for path in ["../driver.c", "/Firmware/driver.c", "Firmware/../outside.c",
                     "Firmware\\driver.c", "Firmware//driver.c"]:
            with self.subTest(path=path):
                self.lock["files"][0]["path"] = path
                self.save()
                with self.assertRaises(VendorSourceError):
                    verify(self.root)

    def test_symlink_source_fails(self):
        outside = Path(self.temp.name) / "outside.c"
        self.source.rename(outside)
        self.source.symlink_to(outside)
        with self.assertRaisesRegex(VendorSourceError, "symlinks"):
            verify(self.root)

    def test_duplicate_or_empty_inventory_fails(self):
        self.lock["files"].append(dict(self.lock["files"][0]))
        self.save()
        with self.assertRaisesRegex(VendorSourceError, "Duplicate"):
            verify(self.root)
        self.lock["files"] = []
        self.save()
        with self.assertRaisesRegex(VendorSourceError, "contain"):
            verify(self.root)

    def test_missing_license_fails(self):
        self.lock["files"][0].pop("license")
        self.save()
        with self.assertRaisesRegex(VendorSourceError, "license"):
            verify(self.root)


if __name__ == "__main__":
    unittest.main()
