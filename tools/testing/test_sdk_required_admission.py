"""Reject an incomplete SDK manifest before opening its payload contents."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]


class SDKRequiredAdmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        specification = importlib.util.spec_from_file_location(
            "sdk_required_admission",
            ROOT / "cmake/package/package_source_sdk.py")
        cls.sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(cls.sdk)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="SDK admission ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "share/nexus/src"
        self.root.mkdir(parents=True)
        self.payload = self.root / "ordinary.c"
        self.payload.write_bytes(b"reviewed payload bytes\n")
        self.files = {"ordinary.c": self.sdk.digest(self.payload.read_bytes())}
        self.identity = {
            "schema": 1, "kind": "nexus-installed-source-sdk",
            "source_revision": "a" * 40, "source_tree": "b" * 40,
            "publishable": True, "source_dirty": False,
            "files_sha256": self.files, "dependencies": [],
            "reviewed_imports": [], "toolchains_lock_sha256": "c" * 64,
            "snapshot_sha256": "", "package_files_sha256": {},
        }

    def write_manifest(self):
        self.identity["snapshot_sha256"] = self.sdk.digest(json.dumps(
            self.files, sort_keys=True, separators=(",", ":")).encode())
        (self.root / self.sdk.MANIFEST).write_text(json.dumps(self.identity))

    def complete_required_map(self):
        # Portable rejection fixtures do not claim a complete installed SDK.
        # Retain the actual policy; no vendor checkout or reduced checker graph.
        for name in self.sdk.REQUIRED_FILES:
            self.files[name] = self.sdk.digest(b"fixture placeholder")

    def test_missing_required_names_reject_without_opening_payload(self):
        self.write_manifest()
        reads = []
        original_open = Path.open

        def observe_open(path, *args, **kwargs):
            if path == self.payload:
                reads.append(path)
            return original_open(path, *args, **kwargs)

        with mock.patch.object(Path, "open", autospec=True,
                               side_effect=observe_open):
            with self.assertRaisesRegex(self.sdk.SDKError,
                                        "missing required sources/licenses"):
                self.sdk.verify(self.root)
        self.assertEqual(reads, [],
                         "An unqualified manifest opened payload bytes")

    def test_complete_required_map_still_checks_payload_bytes(self):
        self.complete_required_map()
        self.write_manifest()
        self.payload.write_bytes(b"changed bytes after manifest publication\n")
        with self.assertRaisesRegex(self.sdk.SDKError,
                                    "file identity changed: ordinary.c"):
            self.sdk.verify(self.root)

    def test_complete_required_map_still_rejects_unsafe_payload_paths(self):
        self.complete_required_map()
        self.files = {"../outside.c": "d" * 64, **self.files}
        self.identity["files_sha256"] = self.files
        self.write_manifest()
        with self.assertRaisesRegex(self.sdk.SDKError,
                                    "Unsafe source SDK path"):
            self.sdk.verify(self.root)

    def test_schema_and_provenance_precede_required_name_admission(self):
        for changes, message in (
                ({"schema": 2}, "Unsupported installed source SDK identity"),
                ({"source_revision": "invalid"}, "Invalid source Git identity"),
                ({"publishable": "true"},
                 "Invalid source SDK provenance flags"),
                ({"source_dirty": True},
                 "dirty source SDK cannot be publishable")):
            with self.subTest(changes=changes):
                original = dict(self.identity)
                try:
                    self.identity.update(changes)
                    self.write_manifest()
                    with self.assertRaisesRegex(self.sdk.SDKError, message):
                        self.sdk.verify(self.root)
                finally:
                    self.identity = original


if __name__ == "__main__":
    unittest.main()
