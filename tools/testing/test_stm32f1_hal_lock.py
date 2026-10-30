"""Official LL source is closed, paired and privately consumed without HAL tick."""
from __future__ import annotations
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch as mock_patch

from tools.maintenance import stm32f1_hal_lock as official
from tools.maintenance import source_catalogue

ROOT = Path(__file__).resolve().parents[2]


class STM32F1OfficialSourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="official F1 LL ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "vendors/st/stm32f1_hal"
        self.source.mkdir(parents=True)
        self.cmsis = self.root / "vendors/st/cmsis_device_f1"
        self.cmsis.mkdir()
        self.lock_path = self.root / "dependencies/stm32f1-hal.lock.json"
        self.lock_path.parent.mkdir()
        self.lock = json.loads((ROOT / "dependencies/stm32f1-hal.lock.json")
                               .read_text())
        self.write_lock(self.lock)
        for name in self.lock["files"]:
            file = self.source / name
            file.parent.mkdir(exist_ok=True)
            shutil.copyfile(ROOT / "vendors/st/stm32f1_hal" / name, file)
        for source in (self.source, self.cmsis):
            (source / ".git").mkdir()
        self.states = {
            self.source: {"HEAD": official.COMMIT, "origin": source_catalogue.dependency(
                official.DIRECTORY).fetch_url,
                          "status": ""},
            self.cmsis: {"HEAD": official.CMSIS_COMMIT,
                         "origin": source_catalogue.dependency(
                             official.CMSIS_DIRECTORY).fetch_url, "status": ""},
        }
        self.git_patch = mock_patch.object(official.cmsis_device_lock, "git",
                                          side_effect=self.git)
        self.git_patch.start()
        self.addCleanup(self.git_patch.stop)

    def write_lock(self, lock):
        self.lock_path.write_text(json.dumps(lock) + "\n")

    def git(self, source, *arguments):
        state = self.states[source]
        if arguments == ("rev-parse", "--show-toplevel"):
            return str(source)
        if arguments == ("rev-parse", "HEAD"):
            return state["HEAD"]
        if arguments == ("config", "--get-all", "remote.origin.url"):
            return state["origin"]
        if arguments == ("status", "--porcelain", "--untracked-files=all"):
            return state["status"]
        self.fail("Unexpected Git identity query: " + repr(arguments))

    def verify(self, **options):
        return official.verify(self.source, self.lock_path, self.cmsis,
                               **options)

    def assert_reviewed_file_count(self, result):
        reviewed = official.REVIEWED_FILES
        self.assertIn("Inc/stm32f1xx_ll_i2c.h", reviewed)
        self.assertEqual(set(self.lock["files"]), reviewed)
        self.assertEqual(result["verified_files"], len(reviewed))

    def test_paired_clean_official_source_verifies_without_qualification_claim(self):
        result = self.verify()
        self.assertEqual(result["commit"], official.COMMIT)
        self.assertEqual(result["cmsis_commit"], official.CMSIS_COMMIT)
        self.assert_reviewed_file_count(result)
        self.assertEqual(result["license"], "BSD-3-Clause")
        self.assertEqual(result["source_kind"], "git_checkout")
        self.assertFalse(result["software_qualified"])

    def test_real_official_checkout_and_cli_are_read_only(self):
        self.git_patch.stop()
        source = ROOT / "vendors/st/stm32f1_hal"
        cmsis = ROOT / "vendors/st/cmsis_device_f1"
        lock = ROOT / "dependencies/stm32f1-hal.lock.json"
        result = official.verify(source, lock, cmsis)
        self.assert_reviewed_file_count(result)
        executed = subprocess.run([
            sys.executable, "-B", str(ROOT / "tools/maintenance/stm32f1_hal_lock.py"),
            "--source", str(source), "--lock", str(lock),
            "--cmsis-source", str(cmsis)], capture_output=True, text=True,
            timeout=30)
        self.assertEqual(executed.returncode, 0, executed.stderr)
        cli_result = json.loads(executed.stdout)
        self.assert_reviewed_file_count(cli_result)
        self.assertEqual(cli_result["source_kind"], "git_checkout")

    def test_unknown_missing_or_reauthored_pairing_and_release_reject(self):
        candidates = [{**self.lock, "fallback": True}, {}, []]
        for name in self.lock:
            changed = copy.deepcopy(self.lock)
            del changed[name]
            candidates.append(changed)
        for field in ("repository", "commit", "release", "selected_api"):
            changed = copy.deepcopy(self.lock)
            changed[field] = "unreviewed"
            candidates.append(changed)
        for field in self.lock["pairing"]:
            changed = copy.deepcopy(self.lock)
            changed["pairing"][field] = "unreviewed"
            candidates.append(changed)
        for changed in candidates:
            with self.subTest(changed=changed):
                self.write_lock(changed)
                with self.assertRaises(ValueError):
                    self.verify()

    def test_duplicate_boolean_schema_nonfinite_and_missing_file_reject(self):
        raw = json.dumps(self.lock)
        for changed in (raw.replace('"schema_version": 1',
                                    '"schema_version": true'),
                        raw.replace('"schema_version": 1',
                                    '"schema_version": 1, "schema_version": 1'),
                        raw.replace('"bytes": ', '"bytes": NaN, "extra": ')):
            self.lock_path.write_text(changed)
            with self.subTest(raw=changed[:80]), self.assertRaises(ValueError):
                self.verify()
        for name in self.lock["files"]:
            changed = copy.deepcopy(self.lock)
            del changed["files"][name]
            self.write_lock(changed)
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()

    def test_extra_unsafe_or_unreviewed_file_and_license_reject(self):
        for name in ("../escape", "/LICENSE.md", "Inc\\unsafe.h",
                     "Src/stm32f1xx_hal.c"):
            changed = copy.deepcopy(self.lock)
            changed["files"][name] = {"bytes": 1, "sha256": "a" * 64}
            self.write_lock(changed)
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()
        for license_value in ({"path": "LICENSE.md", "expression": "MIT"},
                              {"path": "LICENSE", "expression": "BSD-3-Clause"}):
            self.write_lock({**self.lock, "license": license_value})
            with self.assertRaises(ValueError):
                self.verify()

    def test_changed_file_notice_symlink_and_same_bytes_rewrite_reject(self):
        for name in self.lock["files"]:
            file = self.source / name
            original = file.read_bytes()
            file.write_bytes(original + b"\n")
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()
            file.write_bytes(original)
        file = self.source / "LICENSE.md"
        real = self.root / "real notice"
        file.rename(real)
        file.symlink_to(real)
        with self.assertRaises(ValueError):
            self.verify()
        file.unlink()
        real.rename(file)
        real_snapshot = official.cmsis_device_lock.stable_snapshot
        changed = False

        def rewrite(path):
            nonlocal changed
            result = real_snapshot(path)
            if not changed and Path(path) == file:
                changed = True
                file.write_bytes(file.read_bytes())
            return result

        with mock_patch.object(official.cmsis_device_lock, "stable_snapshot",
                               side_effect=rewrite), self.assertRaises(ValueError):
            self.verify()

    def test_each_dependency_wrong_commit_origin_dirty_and_missing_git_reject(self):
        for source, state in self.states.items():
            for field in state:
                old = state[field]
                state[field] = "unexpected"
                with self.subTest(source=source, field=field):
                    with self.assertRaises(ValueError):
                        self.verify()
                state[field] = old
            shutil.rmtree(source / ".git")
            with self.subTest(source=source), self.assertRaises(ValueError):
                self.verify()
            (source / ".git").mkdir()

    def test_packaged_requires_both_exact_commits_and_no_git(self):
        for source in self.states:
            shutil.rmtree(source / ".git")
        with self.assertRaises(ValueError):
            self.verify(packaged_commit=official.COMMIT)
        with self.assertRaises(ValueError):
            self.verify(packaged_commit="a" * 40,
                        packaged_cmsis_commit=official.CMSIS_COMMIT)
        with self.assertRaises(ValueError):
            self.verify(packaged_commit=official.COMMIT,
                        packaged_cmsis_commit="a" * 40)
        result = self.verify(packaged_commit=official.COMMIT,
                             packaged_cmsis_commit=official.CMSIS_COMMIT)
        self.assertEqual(result["source_kind"], "installed_source_sdk")
        (self.cmsis / ".git").mkdir()
        with self.assertRaises(ValueError):
            self.verify(packaged_commit=official.COMMIT,
                        packaged_cmsis_commit=official.CMSIS_COMMIT)


if __name__ == "__main__":
    unittest.main()
