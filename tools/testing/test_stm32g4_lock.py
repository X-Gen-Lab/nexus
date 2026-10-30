"""The exact G474 device-source closure rejects altered or partial inputs."""
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
from tools.maintenance import stm32g4_lock, source_catalogue

ROOT = Path(__file__).resolve().parents[2]


class STM32G4LockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.seed_temporary = tempfile.TemporaryDirectory(
            prefix="G474 device seed ")
        cls.addClassCleanup(cls.seed_temporary.cleanup)
        cls.seed_source = Path(cls.seed_temporary.name) / "official source"
        cls.seed_source.mkdir()
        cls.seed_lock = json.loads(
            (ROOT / "dependencies/stm32g4.lock.json").read_text())
        for name in cls.seed_lock["files"]:
            file = cls.seed_source / name
            file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "vendors/st/cmsis_device_g4" / name, file)
        cls._git(cls.seed_source, "init", "--quiet")
        cls._git(cls.seed_source, "config", "user.name", "Device lock test")
        cls._git(cls.seed_source, "config", "user.email",
                 "device-test@local.invalid")
        cls._git(cls.seed_source, "remote", "add", "origin",
                 source_catalogue.dependency("stm32g4_cmsis").fetch_url)
        cls._git(cls.seed_source, "add", ".")
        cls._git(cls.seed_source, "commit", "--quiet", "-m",
                 "actual selected device source")
        cls.seed_lock["commit"] = cls._git(cls.seed_source, "rev-parse", "HEAD")

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="G474 device source ")
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.source = self.folder / "official source"
        # Copy .git too: index, objects and config belong to this case. Never
        # share hardlinks or cache a verifier result for mutable test inputs.
        shutil.copytree(type(self).seed_source, self.source)
        self.lock = copy.deepcopy(type(self).seed_lock)
        self.lock_path = self.folder / "stm32g4.lock.json"
        self.write_lock()

    @staticmethod
    def _git(source, *arguments):
        environment = {k: v for k, v in os.environ.items()
                       if not k.startswith("GIT_")}
        result = subprocess.run(["git", "-C", str(source), *arguments],
                                capture_output=True, text=True, env=environment,
                                timeout=20)
        if result.returncode:
            raise AssertionError(result.stderr)
        return result.stdout.strip()

    def git(self, *arguments):
        return self._git(self.source, *arguments)

    def write_lock(self):
        self.lock_path.write_text(json.dumps(self.lock) + "\n")

    def verify(self, **options):
        return stm32g4_lock.verify(self.source, self.lock_path, **options)

    def test_class_seed_isolates_worktree_index_and_untracked_state(self):
        seed = type(self).seed_source

        def snapshot():
            return {file.relative_to(seed).as_posix():
                    hashlib.sha256(file.read_bytes()).hexdigest()
                    for file in seed.rglob("*") if file.is_file()}

        before = snapshot()
        name = "Include/stm32g474xx.h"
        original = (self.source / name).read_bytes()
        (self.source / name).write_bytes(original + b"staged mutation\n")
        self.git("add", name)
        (self.source / name).write_bytes(original + b"unstaged mutation\n")
        (self.source / "untracked.txt").write_text("private case\n")
        self.git("remote", "set-url", "origin", "https://invalid.example/git")
        with self.assertRaises(ValueError):
            self.verify()

        sibling = type(self)(
            "test_actual_official_root_checkout_is_hash_verified")
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

    def test_real_git_source_verifies_exact_part_and_does_not_change_checkout(self):
        before = self.git("status", "--porcelain")
        result = self.verify()
        self.assertEqual(result["commit"], self.lock["commit"])
        self.assertEqual(result["verified_files"], 5)
        self.assertEqual(result["selected_part"], "STM32G474RET6")
        self.assertEqual(result["license"], "Apache-2.0")
        self.assertFalse(result["software_qualified"])
        self.assertEqual(self.git("status", "--porcelain"), before)

    def test_actual_official_root_checkout_is_hash_verified(self):
        result = stm32g4_lock.verify(ROOT / "vendors/st/cmsis_device_g4",
                                    ROOT / "dependencies/stm32g4.lock.json")
        self.assertEqual(result["verified_files"], 5)

    def test_approved_transport_is_separate_from_official_repository(self):
        item = source_catalogue.dependency("stm32g4_cmsis")
        approved = replace(item,
            fetch_url="https://github.com/Reviewed-Test/cmsis-device-g4.git")
        entries = tuple(approved if record == item else record
                        for record in source_catalogue.ALL_DEPENDENCIES)
        self.git("remote", "set-url", "origin", approved.fetch_url)
        with patch.object(source_catalogue, "ALL_DEPENDENCIES", entries):
            self.assertEqual(self.verify()["commit"], self.lock["commit"])
            self.git("remote", "set-url", "origin", self.lock["repository"])
            with self.assertRaises(ValueError):
                self.verify()

    def test_url_rewrite_cannot_admit_a_foreign_raw_origin(self):
        fetch = source_catalogue.dependency("stm32g4_cmsis").fetch_url
        alias = "https://unreviewed.invalid/device.git"
        self.git("remote", "set-url", "origin", alias)
        self.git("config", "url." + fetch + ".insteadOf", alias)
        with self.assertRaises(ValueError):
            self.verify()

    def test_unknown_or_missing_schema_fields_reject(self):
        for name in self.lock:
            value = copy.deepcopy(self.lock)
            del value[name]
            self.lock_path.write_text(json.dumps(value))
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()
        self.lock["fallback"] = True
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()

    def test_duplicate_json_key_boolean_schema_and_nonfinite_identity_reject(self):
        data = json.dumps(self.lock)
        for changed in (data.replace('"schema_version": 1',
                                     '"schema_version": 1, "schema_version": 1'),
                        data.replace('"schema_version": 1',
                                     '"schema_version": true'),
                        data.replace('"bytes": 8920', '"bytes": NaN')):
            self.lock_path.write_text(changed)
            with self.subTest(data=changed[:64]), self.assertRaises(ValueError):
                self.verify()

    def test_unknown_part_origin_commit_and_license_reject(self):
        for key, value in (("selected_part", "STM32G491RET6"),
                           ("repository", "https://example.com/device.git"),
                           ("commit", "main"), ("commit", True)):
            changed = copy.deepcopy(self.lock)
            changed[key] = value
            self.lock_path.write_text(json.dumps(changed))
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.verify()
        for key, value in (("path", "LICENSE.txt"), ("expression", "MIT")):
            changed = copy.deepcopy(self.lock)
            changed["license"][key] = value
            self.lock_path.write_text(json.dumps(changed))
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.verify()

    def test_closed_file_map_omission_extra_and_unsafe_paths_reject(self):
        for name in self.lock["files"]:
            changed = copy.deepcopy(self.lock)
            del changed["files"][name]
            self.lock_path.write_text(json.dumps(changed))
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()
        for name in ("../escape.h", "/Include/stm32g474xx.h",
                     "Include\\stm32g474xx.h", "Include/stm32g491xx.h"):
            changed = copy.deepcopy(self.lock)
            changed["files"][name] = {"bytes": 1, "sha256": "a" * 64}
            self.lock_path.write_text(json.dumps(changed))
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()

    def test_invalid_bytes_hash_and_unknown_identity_field_reject(self):
        for identity in ({"bytes": True, "sha256": "a" * 64},
                         {"bytes": 0, "sha256": "a" * 64},
                         {"bytes": 1, "sha256": "latest"},
                         {"bytes": 1, "sha256": "a" * 64, "fallback": 1}):
            changed = copy.deepcopy(self.lock)
            changed["files"]["LICENSE.md"] = identity
            self.lock_path.write_text(json.dumps(changed))
            with self.subTest(identity=identity), self.assertRaises(ValueError):
                self.verify()

    def test_changed_or_missing_each_selected_file_and_notice_reject(self):
        for name in self.lock["files"]:
            file = self.source / name
            original = file.read_bytes()
            file.write_bytes(original + b"\n")
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()
            file.unlink()
            with self.subTest(missing=name), self.assertRaises(ValueError):
                self.verify()
            file.write_bytes(original)

    def test_source_and_parent_directory_symlinks_reject(self):
        file = self.source / "Include/stm32g474xx.h"
        real = self.folder / "real.h"
        file.rename(real)
        file.symlink_to(real)
        with self.assertRaises(ValueError):
            self.verify()
        file.unlink()
        real.rename(file)
        folder = self.source / "Include"
        actual = self.folder / "real Include"
        folder.rename(actual)
        folder.symlink_to(actual, target_is_directory=True)
        with self.assertRaises(ValueError):
            self.verify()

    def test_source_alias_and_lock_symlink_reject(self):
        alias = self.folder / "alias"
        alias.symlink_to(self.source, target_is_directory=True)
        with self.assertRaises(ValueError):
            stm32g4_lock.verify(alias, self.lock_path)
        link = self.folder / "alias.lock"
        link.symlink_to(self.lock_path)
        with self.assertRaises(ValueError):
            stm32g4_lock.verify(self.source, link)

    def test_wrong_actual_head_origin_and_untracked_checkout_reject(self):
        self.lock["commit"] = "a" * 40
        self.write_lock()
        with self.assertRaises(ValueError):
            self.verify()
        self.lock["commit"] = self.git("rev-parse", "HEAD")
        self.write_lock()
        self.git("remote", "set-url", "origin", "https://example.com/device.git")
        with self.assertRaises(ValueError):
            self.verify()
        self.git("remote", "set-url", "origin",
                 source_catalogue.dependency("stm32g4_cmsis").fetch_url)
        (self.source / "untracked.txt").write_text("not locked\n")
        with self.assertRaises(ValueError):
            self.verify()

    def test_packaged_commit_requires_exact_identity_without_git_fallback(self):
        shutil.rmtree(self.source / ".git")
        with self.assertRaises(ValueError):
            self.verify()
        with self.assertRaises(ValueError):
            self.verify(packaged_commit="b" * 40)
        result = self.verify(packaged_commit=self.lock["commit"])
        self.assertEqual(result["source_kind"], "installed_source_sdk")

    def test_cli_checks_real_source_and_rejects_missing_git_without_manifest(self):
        command = [sys.executable, "-B", str(ROOT / "tools/maintenance/stm32g4_lock.py"),
                   "--source", str(self.source), "--lock", str(self.lock_path)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        shutil.rmtree(self.source / ".git")
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)

    def test_cli_dangling_sdk_manifest_rejects_despite_valid_git(self):
        sdk = self.folder / "prefix/share/nexus/src"
        destination = sdk / "vendors/st/cmsis_device_g4"
        destination.parent.mkdir(parents=True)
        self.source.rename(destination)
        self.source = destination
        self.lock_path = sdk / "dependencies/stm32g4.lock.json"
        self.lock_path.parent.mkdir()
        self.write_lock()
        shutil.copyfile(ROOT / "dependencies/threadx.lock.json",
                        sdk / "dependencies/threadx.lock.json")
        shutil.copyfile(ROOT / "dependencies/stm32f4-hal.lock.json",
                        sdk / "dependencies/stm32f4-hal.lock.json")
        shutil.copyfile(ROOT / "dependencies/stm32f1-hal.lock.json",
                        sdk / "dependencies/stm32f1-hal.lock.json")
        checker = sdk / "cmake/package/package_source_sdk.py"
        checker.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / "cmake/package/package_source_sdk.py", checker)
        command = [sys.executable, "-B", str(ROOT / "tools/maintenance/stm32g4_lock.py"),
                   "--source", str(self.source), "--lock", str(self.lock_path)]
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
