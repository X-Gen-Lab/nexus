"""Documentation preparation rejects drift before exposing usable tools."""
from __future__ import annotations

import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

from tools.documentation import install

ROOT = Path(__file__).resolve().parents[2]


class DocumentationInstallTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="docs closure ")
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)

    def test_changed_archive_is_rejected_before_extraction(self):
        archive = self.folder / "locked.deb"
        archive.write_bytes(b"changed bytes")
        locked = {"filename": archive.name, "sha256": "0" * 64,
                  "size": archive.stat().st_size}
        with self.assertRaisesRegex(ValueError, "digest"):
            install.verify_blob(archive, locked)

    def test_offline_missing_blob_does_not_contact_network(self):
        locked = {"filename": "missing.whl", "sha256": "0" * 64,
                  "size": 3, "url": "https://example.invalid/missing.whl"}
        with mock.patch.object(install.urllib.request, "urlopen") as network:
            with self.assertRaisesRegex(ValueError, "offline"):
                install.fetch_blob(self.folder, locked, offline=True)
        network.assert_not_called()

    def test_existing_bad_blob_is_not_silently_redownloaded(self):
        archive = self.folder / "existing.whl"
        archive.write_bytes(b"bad")
        locked = {"filename": archive.name, "sha256": "0" * 64,
                  "size": 3, "url": "https://example.invalid/file"}
        with mock.patch.object(install.urllib.request, "urlopen") as network:
            with self.assertRaises(ValueError):
                install.fetch_blob(self.folder, locked, offline=False)
        network.assert_not_called()

    def test_tar_escape_and_link_are_rejected(self):
        for name, symlink in (("../escape", False), ("link", True)):
            archive = self.folder / "unsafe.tar.gz"
            with tarfile.open(archive, "w:gz") as tar:
                entry = tarfile.TarInfo(name)
                if symlink:
                    entry.type = tarfile.SYMTYPE
                    entry.linkname = "/outside"
                    tar.addfile(entry)
                else:
                    entry.size = 1
                    tar.addfile(entry, io.BytesIO(b"x"))
            with self.assertRaisesRegex(ValueError, "unsafe"):
                install.extract_tar(archive, self.folder / "output")
        self.assertFalse((self.folder.parent / "escape").exists())

    def test_lock_requires_complete_unique_closure_and_identity(self):
        locked = json.loads((ROOT / "dependencies/docs-tools.lock.json")
                            .read_text())
        install.validate_lock(locked)
        for change in ("empty", "duplicate", "digest", "tuple"):
            broken = copy.deepcopy(locked)
            if change == "empty":
                broken["wheels"] = []
            elif change == "duplicate":
                broken["wheels"].append(broken["wheels"][0])
            elif change == "digest":
                broken["native"][0]["sha256"] = "marker-only"
            else:
                broken["target"]["machine"] = "generic"
            with self.subTest(change=change), self.assertRaises(ValueError):
                install.validate_lock(broken)

    def test_requirements_reference_cannot_drift_from_authoritative_lock(self):
        locked = json.loads((ROOT / "dependencies/docs-tools.lock.json")
                            .read_text())
        reference = ROOT / "dependencies/docs-python-linux-cp312.txt"
        install.verify_requirements(reference, locked)
        changed = self.folder / "requirements.txt"
        changed.write_text(reference.read_text().replace("--hash=sha256:",
                                                       "--hash=sha512:", 1))
        with self.assertRaisesRegex(ValueError, "requirements"):
            install.verify_requirements(changed, locked)

    def test_doxygen_obeys_the_same_archive_identity_boundary(self):
        locked = json.loads((ROOT / "dependencies/docs-tools.lock.json")
                            .read_text())
        for field, value in (("filename", "../../escape.tar.gz"),
                             ("url", "file:///tmp/untrusted.tar.gz"),
                             ("size", -1), ("licenses", [])):
            broken = copy.deepcopy(locked)
            broken["doxygen"][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                install.validate_lock(broken)

    def test_installed_file_drift_invalidates_receipt(self):
        prefix = self.folder / "prefix"
        prefix.mkdir()
        payload = prefix / "bin/tool"
        payload.parent.mkdir()
        payload.write_bytes(b"original")
        receipt = {"files": {"bin/tool": hashlib.sha256(b"original")
                             .hexdigest()}}
        install.verify_installed(prefix, receipt)
        payload.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "installed"):
            install.verify_installed(prefix, receipt)

    def test_added_python_module_invalidates_complete_inventory(self):
        prefix = self.folder / "prefix"
        prefix.mkdir()
        (prefix / "locked.py").write_bytes(b"original")
        receipt = {"files": {"locked.py": hashlib.sha256(b"original")
                             .hexdigest()}}
        (prefix / "extra.py").write_bytes(b"unlocked")
        with self.assertRaisesRegex(ValueError, "inventory"):
            install.verify_installed(prefix, receipt)

    def test_retargeted_runtime_symlink_invalidates_receipt(self):
        prefix = self.folder / "prefix"
        prefix.mkdir()
        (prefix / "first").write_bytes(b"one")
        (prefix / "second").write_bytes(b"two")
        (prefix / "alias").symlink_to("first")
        receipt = {"files": install.tree_files(prefix),
                   "links": {"alias": "first"}}
        (prefix / "alias").unlink()
        (prefix / "alias").symlink_to("second")
        with self.assertRaisesRegex(ValueError, "link"):
            install.verify_installed(prefix, receipt)

    def test_native_shared_license_references_need_complete_texts(self):
        native = self.folder / "native"
        shared = native / "usr/share/common-licenses"
        shared.mkdir(parents=True)
        (shared / "LGPL-2.1").write_text("complete LGPL text")
        output = self.folder / "licenses"
        output.mkdir()
        (output / "libc6.txt").write_text(
            "Full license: /usr/share/common-licenses/LGPL-2.1.")
        install.collect_common_licenses(native, output)
        self.assertEqual((output / "common-licenses/LGPL-2.1").read_text(),
                         "complete LGPL text")
        (output / "missing.txt").write_text(
            "Full license: /usr/share/common-licenses/GPL-3.")
        with self.assertRaisesRegex(ValueError, "shared license"):
            install.collect_common_licenses(native, output)

    def test_legacy_python_wrapper_uses_one_cli_and_retains_failure(self):
        wrapper = ROOT / "scripts/tools/docs.py"
        fake = self.folder / "fake"
        fake.write_text("#!/bin/sh\nexit 7\n")
        fake.chmod(0o755)
        specification = __import__("importlib.util", fromlist=["util"])
        spec = specification.spec_from_file_location("legacy_docs", wrapper)
        module = specification.module_from_spec(spec)
        spec.loader.exec_module(module)
        with mock.patch.object(module.subprocess, "call", return_value=7) as call:
            self.assertEqual(module.main(["--target", "all"]), 7)
        arguments = call.call_args.args[0]
        self.assertEqual(arguments[1], str(ROOT / "tools/dev/dev.py"))
        self.assertEqual(arguments[2:4], ["docs", "build"])
        self.assertNotIn("doxygen", arguments)


if __name__ == "__main__":
    unittest.main()
