"""Exact official ThreadX closure rejects stale or incomplete source inputs."""
from __future__ import annotations
from dataclasses import replace
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from tools.maintenance import threadx_lock, source_catalogue

ROOT = Path(__file__).resolve().parents[2]


class ThreadXLockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.seed_temporary = tempfile.TemporaryDirectory(
            prefix="threadx source seed ")
        cls.addClassCleanup(cls.seed_temporary.cleanup)
        cls.seed_source = Path(cls.seed_temporary.name) / "official source"
        cls.seed_source.mkdir()
        cls.seed_lock = json.loads(
            (ROOT / "dependencies/threadx.lock.json").read_text())
        for name in [*cls.seed_lock["files"],
                     cls.seed_lock["license"]["path"]]:
            out = cls.seed_source / name
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / "ext/threadx" / name, out)
        cls._git(cls.seed_source, "init", "--quiet")
        cls._git(cls.seed_source, "config", "user.name", "ThreadX lock test")
        cls._git(cls.seed_source, "config", "user.email",
                 "threadx-test@local.invalid")
        cls._git(cls.seed_source, "remote", "add", "origin",
                 source_catalogue.dependency("threadx").fetch_url)
        cls._git(cls.seed_source, "add", ".")
        cls._git(cls.seed_source, "commit", "--quiet", "-m",
                 "actual locked source fixture")
        cls.seed_lock["commit"] = cls._git(cls.seed_source, "rev-parse", "HEAD")
        cls._git(cls.seed_source, "tag", cls.seed_lock["tag"])

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="threadx source lock ")
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.source = self.folder / "official source"
        # Copy .git too: index, objects and config belong to this case. Never
        # share hardlinks or cache a verifier result for mutable test inputs.
        shutil.copytree(type(self).seed_source, self.source)
        self.lock = copy.deepcopy(type(self).seed_lock)
        self.lock_path = self.folder / "threadx.lock.json"
        self.write_lock()

    @staticmethod
    def _git(source, *args):
        environment = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
        result = subprocess.run(["git", "-C", str(source), *args],
                                capture_output=True, text=True, env=environment,
                                timeout=20)
        if result.returncode:
            raise AssertionError(result.stderr)
        return result.stdout.strip()

    def git(self, *args):
        return self._git(self.source, *args)

    def write_lock(self):
        self.lock_path.write_text(json.dumps(self.lock, indent=2) + "\n")

    def verify(self, port="linux"):
        return threadx_lock.verify(self.source, self.lock_path, port)

    def test_class_seed_isolates_worktree_index_and_untracked_state(self):
        seed = type(self).seed_source

        def snapshot():
            return {file.relative_to(seed).as_posix():
                    hashlib.sha256(file.read_bytes()).hexdigest()
                    for file in seed.rglob("*") if file.is_file()}

        before = snapshot()
        name = self.lock["common"][0]
        original = (self.source / name).read_bytes()
        (self.source / name).write_bytes(original + b"staged mutation\n")
        self.git("add", name)
        (self.source / name).write_bytes(original + b"unstaged mutation\n")
        (self.source / "untracked.txt").write_text("private case\n")
        self.git("remote", "set-url", "origin", "https://invalid.example/git")
        with self.assertRaises(ValueError):
            self.verify()

        sibling = type(self)("test_unknown_port_rejects")
        sibling.setUp()
        self.addCleanup(sibling.doCleanups)
        self.assertEqual((sibling.source / name).read_bytes(), original)
        self.assertEqual(sibling.git("status", "--porcelain",
                                     "--untracked-files=all"), "")
        self.assertEqual(sibling.git("diff", "--cached", "--name-only"), "")
        self.assertEqual(sibling.verify()["commit"], sibling.lock["commit"])
        (sibling.source / name).write_bytes(original + b"later mutation\n")
        with self.assertRaises(ValueError):
            sibling.verify()
        self.assertEqual(self.git("diff", "--cached", "--name-only"), name)
        for relative in before:
            first, second = self.source / relative, sibling.source / relative
            self.assertFalse(os.path.samefile(seed / relative, first))
            self.assertFalse(os.path.samefile(seed / relative, second))
            self.assertFalse(os.path.samefile(first, second))
        self.assertEqual(snapshot(), before)

    def test_real_git_source_and_both_ports_verify_without_changes(self):
        before = self.git("status", "--porcelain")
        for port in ("linux", "cortex_m4"):
            result = self.verify(port)
            self.assertEqual(result["commit"], self.lock["commit"])
            self.assertEqual(result["verified_files"], 215)
        self.assertEqual(self.git("status", "--porcelain"), before)

    def test_actual_root_official_checkout_is_hash_verified(self):
        result = threadx_lock.verify(ROOT / "ext/threadx",
                                     ROOT / "dependencies/threadx.lock.json",
                                     "cortex_m4")
        self.assertEqual(result["verified_files"], 215)

    def test_approved_transport_preserves_official_release_pair(self):
        item = source_catalogue.dependency("threadx")
        approved = replace(item,
            fetch_url="https://github.com/Reviewed-Test/threadx.git")
        entries = tuple(approved if record == item else record
                        for record in source_catalogue.ALL_DEPENDENCIES)
        self.git("remote", "set-url", "origin", approved.fetch_url)
        with patch.object(source_catalogue, "ALL_DEPENDENCIES", entries):
            self.assertEqual(self.verify()["commit"], self.lock["commit"])
            self.git("remote", "set-url", "origin", self.lock["repository"])
            with self.assertRaises(ValueError):
                self.verify()

    def test_url_rewrite_cannot_admit_a_foreign_raw_origin(self):
        fetch = source_catalogue.dependency("threadx").fetch_url
        alias = "https://unreviewed.invalid/threadx.git"
        self.git("remote", "set-url", "origin", alias)
        self.git("config", "url." + fetch + ".insteadOf", alias)
        with self.assertRaises(ValueError):
            self.verify()

    def test_unknown_port_rejects(self):
        for port in ("m4", "linux/../linux", "cortex_m7", "", True):
            with self.subTest(port=port), self.assertRaises(ValueError):
                self.verify(port)

    def test_unknown_or_missing_field_rejects(self):
        self.lock["fallback"] = True
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()
        del self.lock["fallback"]
        del self.lock["headers"]
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()

    def test_duplicate_json_key_rejects(self):
        data = self.lock_path.read_text()
        self.lock_path.write_text(data.replace('"schema_version": 1',
                                               '"schema_version": 1, "schema_version": 1'))
        with self.assertRaises(ValueError):
            self.verify()

    def test_duplicate_graph_path_rejects(self):
        self.lock["common"].append(self.lock["common"][0])
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()

    def test_path_traversal_absolute_backslash_and_wrong_family_reject(self):
        for name in ("../escape.c", "/common/src/tx_thread_delete.c",
                     "common\\src\\tx_thread_delete.c", "ports/linux/gnu/src/extra.c"):
            changed = copy.deepcopy(self.lock)
            changed["common"][0] = name
            self.lock_path.write_text(json.dumps(changed))
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()

    def test_closed_graph_omission_rejects_even_if_file_map_rehashed(self):
        name = self.lock["common"].pop()
        del self.lock["files"][name]
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()

    def test_extra_source_in_owned_directory_rejects(self):
        (self.source / "common/src/tx_invented.c").write_text("void invented(void) {}\n")
        with self.assertRaises(ValueError):
            self.verify()

    def test_extra_identity_without_graph_path_rejects(self):
        self.lock["files"]["common/src/tx_invented.c"] = {"bytes": 1, "sha256": "a" * 64}
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()

    def test_changed_source_and_header_reject(self):
        for name in (self.lock["common"][0], self.lock["headers"][0]):
            file = self.source / name
            original = file.read_bytes()
            file.write_bytes(original + b"\n")
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()
            file.write_bytes(original)

    def test_missing_source_or_wrong_size_reject(self):
        name = self.lock["common"][0]
        self.lock["files"][name]["bytes"] += 1
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()
        self.lock["files"][name]["bytes"] -= 1
        self.write_lock()
        (self.source / name).unlink()
        with self.assertRaises(ValueError):
            self.verify()

    def test_symlink_source_or_directory_reject(self):
        name = self.lock["common"][0]
        file = self.source / name
        real = self.folder / "replaced.c"
        file.rename(real)
        file.symlink_to(real)
        with self.assertRaises(ValueError):
            self.verify()
        file.unlink()
        real.rename(file)
        directory = self.source / "common/inc"
        target = self.folder / "headers"
        directory.rename(target)
        directory.symlink_to(target, target_is_directory=True)
        with self.assertRaises(ValueError):
            self.verify()

    def test_license_content_and_declared_expression_reject(self):
        notice = self.source / self.lock["license"]["path"]
        original = notice.read_bytes()
        notice.write_bytes(b"unreviewed terms")
        with self.assertRaises(ValueError):
            self.verify()
        notice.write_bytes(original)
        self.lock["license"]["expression"] = "unknown"
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()

    def test_wrong_head_tag_or_origin_reject(self):
        self.lock["commit"] = "a" * 40
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()
        self.lock["commit"] = self.git("rev-parse", "HEAD")
        self.lock["tag"] = "v0.0.0_absent"
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()
        self.lock["tag"] = "v6.5.2.202603_rel"
        self.write_lock()
        self.git("remote", "set-url", "origin", "https://example.com/threadx.git")
        with self.assertRaises(ValueError):
            self.verify()

    def test_invalid_scalar_and_identity_types_reject(self):
        for key, value in (("schema_version", True), ("commit", "main"),
                           ("repository", "https://example.com/threadx.git")):
            changed = copy.deepcopy(self.lock)
            changed[key] = value
            self.lock_path.write_text(json.dumps(changed))
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.verify()
        self.write_lock()
        self.lock["files"][self.lock["common"][0]]["bytes"] = True
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()

    def test_cli_success_and_failed_hash_have_actual_exit_codes(self):
        command = [sys.executable, str(ROOT / "tools/maintenance/threadx_lock.py"),
                   "--source", str(self.source), "--lock", str(self.lock_path),
                   "--port", "linux"]
        passed = subprocess.run(command, capture_output=True, text=True, timeout=20)
        self.assertEqual(passed.returncode, 0, passed.stderr)
        self.assertEqual(json.loads(passed.stdout)["verified_files"], 215)
        (self.source / self.lock["common"][0]).write_bytes(b"changed")
        failed = subprocess.run(command, capture_output=True, text=True, timeout=20)
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn("rejected", failed.stderr.lower())

    def test_cli_dangling_sdk_manifest_rejects_despite_valid_git(self):
        sdk = self.folder / "prefix/share/nexus/src"
        destination = sdk / "ext/threadx"
        destination.parent.mkdir(parents=True)
        self.source.rename(destination)
        self.source = destination
        self.lock_path = sdk / "dependencies/threadx.lock.json"
        self.lock_path.parent.mkdir()
        self.write_lock()
        shutil.copyfile(ROOT / "dependencies/stm32g4.lock.json",
                        sdk / "dependencies/stm32g4.lock.json")
        shutil.copyfile(ROOT / "dependencies/stm32f4-hal.lock.json",
                        sdk / "dependencies/stm32f4-hal.lock.json")
        shutil.copyfile(ROOT / "dependencies/stm32f1-hal.lock.json",
                        sdk / "dependencies/stm32f1-hal.lock.json")
        checker = sdk / "cmake/package/package_source_sdk.py"
        checker.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / "cmake/package/package_source_sdk.py", checker)
        command = [sys.executable, "-B", str(ROOT / "tools/maintenance/threadx_lock.py"),
                   "--source", str(self.source), "--lock", str(self.lock_path),
                   "--port", "linux"]
        baseline = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(baseline.returncode, 0, baseline.stdout + baseline.stderr)
        self.assertEqual(json.loads(baseline.stdout)["source_kind"], "git_checkout")
        manifest = sdk / ".nexus-source-sdk.json"
        manifest.symlink_to(sdk / "missing-manifest.json")
        self.assertTrue(manifest.is_symlink())
        self.assertFalse(manifest.exists())
        rejected = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertNotEqual(rejected.returncode, 0,
                            "Dangling SDK manifest accepted as Git checkout: " + rejected.stdout)
        self.assertIn("cannot follow symlinks", rejected.stderr)
        self.assertEqual(self.git("status", "--porcelain"), "")


if __name__ == "__main__":
    unittest.main()
