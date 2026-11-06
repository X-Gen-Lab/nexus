"""Static historical SDK closure replay never runs archived validators."""

import copy
import hashlib
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.legacy_sdk import validate_declared_sdk_closure


class LegacySDKClosureTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="legacy SDK ")
        self.addCleanup(directory.cleanup)
        self.base = Path(directory.name)
        self.sdk_root = self.base / "prefix/share/nexus/src"
        self.sdk_root.mkdir(parents=True)
        self.packed = self.base / "payloads"
        self.packed.mkdir()
        self.marker = self.base / "must-not-execute"
        source = ("from pathlib import Path\nPath(" + repr(str(self.marker)) +
                  ").write_text('executed')\n").encode()
        files = {"old/source.c": b"int old_source;\n",
                 "old/empty.c": b"", "old/verify.py": source}
        exports = {"lib/cmake/Nexus/NexusConfig.cmake": b"# old export\n"}
        self.sdk = {"schema": 1, "kind": "nexus-installed-source-sdk",
            "files_sha256": {name: hashlib.sha256(data).hexdigest()
                             for name, data in files.items()},
            "package_files_sha256": {
                name: hashlib.sha256(data).hexdigest()
                for name, data in exports.items()}}
        manifest = self.sdk_root / ".nexus-source-sdk.json"
        atomic_json(manifest, self.sdk)
        self.identity = file_identity(manifest)
        self.closure = {"manifest_sha256": self.identity["sha256"],
                        "files": {}, "package_files": {}}
        for key, root, contents in (
                ("files", self.sdk_root, files),
                ("package_files", self.sdk_root.parents[2], exports)):
            for name, data in contents.items():
                digest = hashlib.sha256(data).hexdigest()
                self.closure[key][name] = {
                    "path": str(root / name), "size": len(data),
                    "sha256": digest, "role": "sdk_file"}
                (self.packed / digest).write_bytes(data)
        (self.packed / self.identity["sha256"]).write_bytes(manifest.read_bytes())

    def resolver(self, identity):
        return self.packed / identity["sha256"]

    def validate(self, *, sdk=None, closure=None, identity=None):
        return validate_declared_sdk_closure(
            identity or self.identity, self.sdk if sdk is None else sdk,
            self.closure if closure is None else closure,
            resolver=self.resolver)

    def test_old_declared_closure_is_checked_without_current_required_sources(self):
        before = {path.name: path.read_bytes() for path in self.packed.iterdir()}
        with patch("tools.evidence.identity.verify_sdk_package",
                   side_effect=AssertionError("must not run current verifier")):
            self.assertIs(self.validate(), self.closure)
        self.assertFalse(self.marker.exists())
        self.assertEqual(before, {path.name: path.read_bytes()
                                  for path in self.packed.iterdir()})

    def test_empty_source_is_valid_but_changed_payload_is_rejected(self):
        self.validate()
        record = self.closure["files"]["old/empty.c"]
        self.resolver(record).write_bytes(b"tampered")
        with self.assertRaisesRegex(EvidenceError, "identity changed"):
            self.validate()

    def test_declared_member_maps_must_be_complete_and_exact(self):
        missing = copy.deepcopy(self.closure)
        missing["files"].pop("old/source.c")
        extra = copy.deepcopy(self.closure)
        extra["package_files"]["lib/extra.cmake"] = copy.deepcopy(
            next(iter(extra["package_files"].values())))
        for closure in (missing, extra):
            with self.subTest(closure=closure), self.assertRaisesRegex(
                    EvidenceError, "member map"):
                self.validate(closure=closure)

    def test_manifest_and_declared_digest_must_match_original_bytes(self):
        wrong_manifest = {**self.closure, "manifest_sha256": "0" * 64}
        with self.assertRaisesRegex(EvidenceError, "manifest"):
            self.validate(closure=wrong_manifest)
        changed_sdk = copy.deepcopy(self.sdk)
        changed_sdk["files_sha256"]["old/source.c"] = "0" * 64
        with self.assertRaisesRegex(EvidenceError, "manifest"):
            self.validate(sdk=changed_sdk)
        wrong_record = copy.deepcopy(self.closure)
        wrong_record["files"]["old/source.c"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(EvidenceError, "declared digest"):
            self.validate(closure=wrong_record)

    def test_noncanonical_names_and_original_paths_are_rejected(self):
        for name in ("/absolute.c", "../escape.c", "a/../b.c", "a//b.c",
                     "./a.c", "a\\b.c", "a/", "", "a\x00b.c"):
            sdk = copy.deepcopy(self.sdk)
            closure = copy.deepcopy(self.closure)
            digest = sdk["files_sha256"].pop("old/source.c")
            record = closure["files"].pop("old/source.c")
            sdk["files_sha256"][name] = digest
            closure["files"][name] = record
            manifest = self.base / "bad-manifest.json"
            atomic_json(manifest, sdk)
            identity = {**file_identity(manifest), "path": self.identity["path"]}
            self.resolver(identity).write_bytes(manifest.read_bytes())
            closure["manifest_sha256"] = identity["sha256"]
            with self.subTest(name=name), self.assertRaisesRegex(
                    EvidenceError, "member name"):
                self.validate(sdk=sdk, closure=closure, identity=identity)
        closure = copy.deepcopy(self.closure)
        closure["files"]["old/source.c"]["path"] = str(self.base / "source.c")
        with self.assertRaisesRegex(EvidenceError, "original SDK path"):
            self.validate(closure=closure)

    def test_noncanonical_manifest_path_and_member_aliases_are_rejected(self):
        bad = {**self.identity, "path": str(self.sdk_root / "sub/../.nexus-source-sdk.json")}
        with self.assertRaisesRegex(EvidenceError, "manifest path"):
            self.validate(identity=bad)
        sdk = copy.deepcopy(self.sdk)
        closure = copy.deepcopy(self.closure)
        name = "share/nexus/src/old/source.c"
        sdk["package_files_sha256"][name] = sdk["files_sha256"]["old/source.c"]
        closure["package_files"][name] = copy.deepcopy(
            closure["files"]["old/source.c"])
        manifest = self.base / "alias.json"
        atomic_json(manifest, sdk)
        identity = {**file_identity(manifest), "path": self.identity["path"]}
        self.resolver(identity).write_bytes(manifest.read_bytes())
        closure["manifest_sha256"] = identity["sha256"]
        with self.assertRaisesRegex(EvidenceError, "duplicate.*path"):
            self.validate(sdk=sdk, closure=closure, identity=identity)

    def test_dot_member_cannot_alias_an_original_directory(self):
        sdk = copy.deepcopy(self.sdk)
        closure = copy.deepcopy(self.closure)
        digest = sdk["files_sha256"].pop("old/source.c")
        record = closure["files"].pop("old/source.c")
        sdk["files_sha256"]["."] = digest
        closure["files"]["."] = {**record, "path": str(self.sdk_root)}
        manifest = self.base / "dot.json"
        atomic_json(manifest, sdk)
        identity = {**file_identity(manifest), "path": self.identity["path"]}
        self.resolver(identity).write_bytes(manifest.read_bytes())
        closure["manifest_sha256"] = identity["sha256"]
        with self.assertRaisesRegex(EvidenceError, "member name"):
            self.validate(sdk=sdk, closure=closure, identity=identity)

    def test_reading_unchanged_bytes_may_update_access_time(self):
        path = self.resolver(self.identity)
        os.utime(path, ns=(0, path.stat().st_mtime_ns))
        self.assertIs(self.validate(), self.closure)

    def test_unknown_fields_roles_and_invalid_sizes_are_rejected(self):
        variants = []
        variants.append({**self.closure, "upgrade": True})
        for change in ({"role": "execution_stream"}, {"size": -1},
                       {"size": True}, {"extra": "field"}):
            changed = copy.deepcopy(self.closure)
            changed["files"]["old/source.c"].update(change)
            variants.append(changed)
        for closure in variants:
            with self.subTest(closure=closure), self.assertRaises(EvidenceError):
                self.validate(closure=closure)


if __name__ == "__main__":
    unittest.main()
