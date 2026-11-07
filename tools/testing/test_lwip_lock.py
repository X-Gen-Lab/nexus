"""Exact optional stack admission retains the full reviewed source closure."""
from __future__ import annotations

import copy
import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.maintenance import lwip_lock


class LwIPSourceLockTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            prefix="lwip exact source ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "ext/lwip"
        self.source.mkdir(parents=True)
        (self.source / ".git").mkdir()
        self.lock = json.loads(lwip_lock.LOCK.read_text())
        self.lock_path = self.root / "dependencies/lwip.lock.json"
        self.lock_path.parent.mkdir()
        self.write_lock()
        (self.root / ".gitmodules").write_text(
            '[submodule "ext/lwip"]\npath = ext/lwip\nurl = ' +
            lwip_lock.FETCH + "\n")
        # Model external file reads, never a smaller reviewed source policy.
        self.identities = {
            self.source / name: {"path": str(self.source / name),
                                  "sha256": digest, "size": 1}
            for name, digest in self.lock["files"].items()
        }
        self.status = ""
        self.commit = lwip_lock.COMMIT
        self.origin = lwip_lock.FETCH
        self.gitlink = "160000 " + lwip_lock.COMMIT + " 0\text/lwip"
        self.git_patch = patch.object(lwip_lock, "git", side_effect=self.git)
        self.git_patch.start()
        self.addCleanup(self.git_patch.stop)
        self.file_patch = patch.object(lwip_lock, "source_identity",
                                       side_effect=self.read_source)
        self.file_patch.start()
        self.addCleanup(self.file_patch.stop)

    def write_lock(self):
        self.lock_path.write_text(json.dumps(self.lock) + "\n")

    def read_source(self, path):
        return dict(self.identities[Path(path)])

    def git(self, source, *arguments, parent_index=False):
        if source == self.root:
            if arguments == ("rev-parse", "--show-toplevel"):
                return str(self.root)
            if arguments == ("ls-files", "--stage", "--", "ext/lwip"):
                self.assertTrue(parent_index)
                return self.gitlink
        self.assertEqual(source, self.source)
        if arguments == ("rev-parse", "--show-toplevel"):
            return str(self.source)
        if arguments == ("rev-parse", "HEAD"):
            return self.commit
        if arguments == ("rev-parse", "HEAD^{tree}"):
            return lwip_lock.TREE
        if arguments == ("config", "--get-all", "remote.origin.url"):
            return self.origin
        if arguments == ("status", "--porcelain", "--untracked-files=all"):
            return self.status
        self.fail("Unexpected Git observation: " + repr(arguments))

    def verify(self, *, development=False, source_sdk=False):
        return lwip_lock.verify(self.source, self.lock_path,
                                development=development, source_sdk=source_sdk)

    def test_org_gitlink_admission_is_not_software_qualification(self):
        result = self.verify()
        self.assertEqual(lwip_lock.COMMIT, result["commit"])
        self.assertEqual(311, result["verified_files"])
        self.assertTrue(result["production_admitted"])
        self.assertFalse(result["software_qualified"])

    def test_development_upstream_is_explicitly_unqualified(self):
        self.origin = lwip_lock.UPSTREAM
        result = self.verify(development=True)
        self.assertFalse(result["production_admitted"])
        self.assertFalse(result["software_qualified"])

    def test_production_rejects_upstream_in_place_of_org_fork(self):
        self.origin = lwip_lock.UPSTREAM
        with self.assertRaisesRegex(ValueError, "organization fork"):
            self.verify()

    def test_dirty_wrong_commit_wrong_origin_and_index_reject(self):
        for attribute, value in (("status", "?? generated.txt"),
                                 ("commit", "0" * 40),
                                 ("origin", "https://example.invalid/lwip"),
                                 ("gitlink", "160000 " + "0" * 40 +
                                  " 0\text/lwip")):
            with self.subTest(attribute=attribute):
                previous = getattr(self, attribute)
                setattr(self, attribute, value)
                with self.assertRaises(ValueError):
                    self.verify()
                setattr(self, attribute, previous)

    def test_changed_license_and_selected_source_reject(self):
        for name in ("COPYING", "NEXUS_CURATED_SOURCE.md", "src/core/init.c"):
            with self.subTest(name=name):
                identity = self.identities[self.source / name]
                before = identity["sha256"]
                identity["sha256"] = "0" * 64
                with self.assertRaisesRegex(ValueError, "bytes"):
                    self.verify()
                identity["sha256"] = before

    def test_symlink_source_or_member_rejects(self):
        link = self.root / "alias"
        link.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "regular"):
            lwip_lock.verify(link, self.lock_path, development=True)
        other = self.root / "license"
        other.write_text("fixture\n")
        (self.source / "COPYING").symlink_to(other)
        with self.assertRaisesRegex(ValueError, "regular"):
            self.verify()

    def test_unknown_fields_or_unsafe_paths_reject_before_git(self):
        for mutation in (lambda lock: lock.update({"fallback": True}),
                         lambda lock: lock["files"].update(
                             {"../escape": "0" * 64})):
            with self.subTest(mutation=mutation):
                old = copy.deepcopy(self.lock)
                mutation(self.lock)
                self.write_lock()
                with self.assertRaises(ValueError):
                    self.verify()
                self.lock = old
                self.write_lock()

    def test_truncated_added_or_reauthored_source_closure_rejects(self):
        mutations = (
            lambda files: files.pop("src/core/init.c"),
            lambda files: files.pop("NEXUS_CURATED_SOURCE.md"),
            lambda files: files.update({"src/additional.c": "0" * 64}),
            lambda files: files.update({"src/core/init.c": "0" * 64}),
        )
        for mutation in mutations:
            old = copy.deepcopy(self.lock)
            mutation(self.lock["files"])
            self.write_lock()
            with self.assertRaisesRegex(ValueError, "closure"):
                self.verify()
            self.lock = old
            self.write_lock()

    def test_source_change_during_observation_rejects(self):
        selected = self.source / "src/core/init.c"
        reads = 0

        def changing_identity(path):
            nonlocal reads
            identity = self.read_source(path)
            if Path(path) == selected:
                reads += 1
                if reads == 2:
                    identity["sha256"] = "0" * 64
            return identity

        with patch.object(lwip_lock, "source_identity",
                          side_effect=changing_identity):
            with self.assertRaisesRegex(ValueError, "changed"):
                self.verify()

    def test_source_sdk_checks_bytes_without_inventing_git_state(self):
        with patch.object(lwip_lock, "git",
                          side_effect=AssertionError("SDK has no Git state")):
            result = self.verify(source_sdk=True)
        self.assertEqual("source_sdk_payload", result["scope"])
        self.assertFalse(result["software_qualified"])

    def test_cmake_preserves_symbolic_selector_for_verifier_rejection(self):
        alias = self.root / "source-alias"
        alias.symlink_to(self.source, target_is_directory=True)
        verifier = self.root / "tools/maintenance/lwip_lock.py"
        verifier.parent.mkdir(parents=True)
        verifier.write_text(
            "import pathlib,sys\n"
            "p=pathlib.Path(sys.argv[sys.argv.index('--source')+1])\n"
            "if p.is_symlink():\n"
            " print('symbolic selector rejected',file=sys.stderr)\n"
            " raise SystemExit(1)\n")
        script = self.root / "CMakeLists.txt"
        script.write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(symbolic_source_selector NONE)\n'
            'add_library(fixture_config INTERFACE)\n'
            'add_library(Nexus::Config ALIAS fixture_config)\n'
            'set_property(TARGET fixture_config PROPERTY NEXUS_SOURCE_DIR "'
            + self.root.as_posix() + '")\n' +
            'set(NEXUS_LWIP_DEVELOPMENT ON)\n' +
            'set(NEXUS_LWIP_SOURCE "' + alias.as_posix() + '")\n' +
            'set(NEXUS_SOURCE_DIR "' + self.root.as_posix() + '")\n' +
            'set(Python3_EXECUTABLE "' + Path(sys.executable).as_posix() +
            '")\ninclude("' +
            (lwip_lock.ROOT / "components/network/CMakeLists.txt").as_posix() +
            '")\nnexus_add_lwip()\n')
        tool = shutil.which("cmake") or str(lwip_lock.ROOT / ".venv/bin/cmake")
        executed = subprocess.run([tool, "-S", str(self.root), "-B",
                                   str(self.root / "selector build")],
                                  capture_output=True, text=True, timeout=30,
                                  check=False)
        self.assertNotEqual(0, executed.returncode)
        self.assertIn("symbolic selector rejected", executed.stderr)

    def sdk_cli(self):
        with contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()):
            return lwip_lock.main(["--source", str(self.source), "--lock",
                                   str(self.lock_path), "--source-sdk"])

    def test_sdk_cli_rejects_bare_directory_and_invalid_manifest(self):
        self.assertEqual(1, self.sdk_cli())
        (self.root / ".nexus-source-sdk.json").write_text("{}\n")
        self.assertEqual(1, self.sdk_cli())

    def test_sdk_cli_delegates_full_proof_before_payload_admission(self):
        (self.root / ".nexus-source-sdk.json").write_text("{}\n")
        manifest = {"dependencies": [{"path": "ext/lwip",
                                      "commit": lwip_lock.COMMIT,
                                      "tree": lwip_lock.TREE}]}
        with patch.object(lwip_lock, "verify_sdk", return_value=manifest,
                          create=True) as verified:
            self.assertEqual(0, self.sdk_cli())
            verified.assert_called_once_with(self.root)

    def test_sdk_cli_rejects_manifest_change_during_full_proof(self):
        path = self.root / ".nexus-source-sdk.json"
        path.write_text("{}\n")

        def changed(root):
            self.assertEqual(root, self.root)
            path.write_text('{"changed":true}\n')
            return {"dependencies": [{"path": "ext/lwip",
                                      "commit": lwip_lock.COMMIT,
                                      "tree": lwip_lock.TREE}]}

        with patch.object(lwip_lock, "verify_sdk", side_effect=changed,
                          create=True):
            self.assertEqual(1, self.sdk_cli())


if __name__ == "__main__":
    unittest.main()
