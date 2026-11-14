"""Selected official sources stay closed, paired and private without a tick."""
from __future__ import annotations
import copy
import json
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch as mock_patch

from tools.maintenance import stm32f4_hal_lock as official
from tools.maintenance import source_catalogue

ROOT = Path(__file__).resolve().parents[2]


class STM32F4OfficialSourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="official F4 LL ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "vendors/st/stm32f4_hal"
        self.source.mkdir(parents=True)
        self.cmsis = self.root / "vendors/st/cmsis_device_f4"
        self.cmsis.mkdir()
        self.lock_path = self.root / "dependencies/stm32f4-hal.lock.json"
        self.lock_path.parent.mkdir()
        self.lock = json.loads((ROOT / "dependencies/stm32f4-hal.lock.json")
                               .read_text())
        self.write_lock(self.lock)
        for name in self.lock["files"]:
            file = self.source / name
            file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "vendors/st/stm32f4_hal" / name, file)
        for source in (self.source, self.cmsis):
            (source / ".git").mkdir()
            for name in ("HEAD", "index", "config", "commondir"):
                (source / ".git" / name).write_text("fixture " + name)
        (self.cmsis / "device.h").write_text("/* paired device input */\n")
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
        if len(arguments) == 3 and arguments[:2] == ("rev-parse", "--git-path"):
            return str(source / ".git" / arguments[2])
        if arguments == ("rev-parse", "--absolute-git-dir"):
            return str(source / ".git")
        self.fail("Unexpected Git identity query: " + repr(arguments))

    def verify(self, **options):
        return official.verify(self.source, self.lock_path, self.cmsis,
                               **options)

    def test_paired_clean_official_source_verifies_without_qualification_claim(self):
        result = self.verify()
        self.assertEqual(result["commit"], official.COMMIT)
        self.assertEqual(result["cmsis_commit"], official.CMSIS_COMMIT)
        self.assertEqual(result["verified_files"], len(self.lock["files"]))
        self.assertEqual(result["license"], "BSD-3-Clause")
        self.assertEqual(result["source_kind"], "git_checkout")
        self.assertFalse(result["software_qualified"])

    def test_selected_board_units_close_the_exact_vendor_byte_graph(self):
        required = {
            "Inc/stm32f4xx_ll_fsmc.h", "Src/stm32f4xx_ll_fsmc.c",
            "Inc/stm32f4xx_ll_sdmmc.h", "Src/stm32f4xx_ll_sdmmc.c",
            "Inc/stm32f4xx_ll_rtc.h", "Inc/stm32f4xx_ll_pwr.h"}
        self.assertTrue(required.issubset(self.lock["files"]),
                        "Selected FSMC, SDIO and RTC compile inputs must be sealed")
        for name in sorted(required):
            with self.subTest(name=name):
                changed = json.loads(json.dumps(self.lock))
                changed["files"].pop(name)
                with self.assertRaisesRegex(ValueError, "reviewed graph"):
                    official.contract(changed)

    def test_new_ip_official_sources_survive_the_actual_sdk_selection(self):
        specification = importlib.util.spec_from_file_location(
            "new_ip_locked_sdk_sources", ROOT / "cmake/package/package_source_sdk.py")
        sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(sdk)
        selected = sdk.dependency_entries(ROOT, official.DIRECTORY)
        required = {"Src/stm32f4xx_hal_qspi.c", "Inc/stm32f4xx_hal_qspi.h",
                    "Src/stm32f4xx_ll_fmc.c", "Inc/stm32f4xx_ll_fmc.h",
                    "Src/stm32f4xx_hal_cryp.c", "Src/stm32f4xx_hal_sai.c",
                    "Src/stm32f4xx_hal_dma2d.c"}
        self.assertLessEqual(required, set(selected))
        self.assertTrue(all((ROOT / official.DIRECTORY / name).is_file()
                            for name in required))

    def test_real_official_checkout_and_cli_are_read_only(self):
        self.git_patch.stop()
        source = ROOT / "vendors/st/stm32f4_hal"
        cmsis = ROOT / "vendors/st/cmsis_device_f4"
        lock = ROOT / "dependencies/stm32f4-hal.lock.json"
        result = official.verify(source, lock, cmsis)
        self.assertEqual(result["verified_files"], len(self.lock["files"]))
        executed = subprocess.run([
            sys.executable, "-B", str(ROOT / "tools/maintenance/stm32f4_hal_lock.py"),
            "--source", str(source), "--lock", str(lock),
            "--cmsis-source", str(cmsis)], capture_output=True, text=True,
            timeout=30)
        self.assertEqual(executed.returncode, 0, executed.stderr)
        self.assertEqual(json.loads(executed.stdout)["source_kind"],
                         "git_checkout")

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
                     "Src/stm32f4xx_hal.c"):
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

    def begin_session(self, path=None):
        pending = path or self.root / "build/session.pending.json"
        pending.parent.mkdir(exist_ok=True)
        result = official.begin_configure(self.source, self.lock_path,
                                          self.cmsis, pending)
        return pending, result["configure_snapshot_sha256"]

    def end_session(self, pending, digest):
        return official.end_configure(self.source, self.lock_path, self.cmsis,
                                      pending, digest)

    def test_configure_snapshot_is_pending_inputs_consumed_by_final_proof(self):
        pending, digest = self.begin_session()
        value = json.loads(pending.read_text())
        self.assertEqual(value["kind"], "nexus-pending-stm32f4-configure-inputs")
        self.assertFalse(value["software_qualified"])
        result = self.end_session(pending, digest)
        self.assertFalse(result["software_qualified"])
        self.assertFalse(pending.exists())
        with self.assertRaises((ValueError, OSError)):
            self.end_session(pending, digest)

    def test_configure_final_proof_rejects_rewrites_and_replacements(self):
        for path in (self.source / "LICENSE.md", self.lock_path,
                     self.cmsis / "device.h", self.cmsis / ".git/HEAD",
                     self.cmsis / ".git/commondir"):
            for replacement in (False, True):
                with self.subTest(path=path, replacement=replacement):
                    pending, digest = self.begin_session()
                    data = path.read_bytes()
                    if replacement:
                        path.unlink()
                    path.write_bytes(data)
                    with self.assertRaisesRegex(ValueError, "changed"):
                        self.end_session(pending, digest)
                    self.assertFalse(pending.exists())

    def test_configure_final_proof_rejects_source_change_even_when_restored(self):
        pending, digest = self.begin_session()
        path = self.source / "LICENSE.md"
        data = path.read_bytes()
        path.write_bytes(data + b"changed")
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "changed"):
            self.end_session(pending, digest)
        self.assertFalse(pending.exists())

    def test_configure_final_proof_rejects_paired_git_identity_changes(self):
        for state in self.states.values():
            for name in ("HEAD", "origin", "status"):
                with self.subTest(name=name):
                    pending, digest = self.begin_session()
                    before = state[name]
                    state[name] = "changed"
                    with self.assertRaises(ValueError):
                        self.end_session(pending, digest)
                    state[name] = before
                    self.assertFalse(pending.exists())

    def test_configure_snapshot_cannot_be_substituted_or_clobbered(self):
        pending, digest = self.begin_session()
        before = pending.read_bytes()
        with self.assertRaises((ValueError, OSError)):
            self.begin_session(pending)
        self.assertEqual(pending.read_bytes(), before)
        pending.write_bytes(before.replace(b'"software_qualified": false',
                                           b'"software_qualified": true'))
        with self.assertRaisesRegex(ValueError, "digest"):
            self.end_session(pending, digest)

    def test_packaged_configure_binds_whole_manifest_and_exported_inputs(self):
        for source in self.states:
            shutil.rmtree(source / ".git")
        prefix = self.root / "prefix"
        sdk = prefix / "share/nexus/src"
        (sdk / "vendors/st").mkdir(parents=True)
        for attribute, name in (("source", official.DIRECTORY),
                                ("cmsis", official.CMSIS_DIRECTORY)):
            shutil.move(str(getattr(self, attribute)), str(sdk / name))
            setattr(self, attribute, sdk / name)
        (sdk / "dependencies").mkdir()
        shutil.move(str(self.lock_path), str(sdk / official.LOCK_NAME))
        self.lock_path = sdk / official.LOCK_NAME
        manifest_path = sdk / ".nexus-source-sdk.json"
        manifest_path.write_text("{}")
        extra = sdk / "other-selected-source.c"
        extra.write_text("/* selected SDK input */\n")
        exported = prefix / "lib/cmake/Nexus/NexusConfig.cmake"
        exported.parent.mkdir(parents=True)
        exported.write_text("# verified SDK export\n")
        manifest = {"dependencies": [
            {"path": official.DIRECTORY, "commit": official.COMMIT},
            {"path": official.CMSIS_DIRECTORY, "commit": official.CMSIS_COMMIT}],
            "files_sha256": {"other-selected-source.c": "verified by SDK port"},
            "package_files_sha256": {
                "lib/cmake/Nexus/NexusConfig.cmake": "verified by SDK port"}}
        manifest_path.write_text(json.dumps(manifest))
        outside = tempfile.TemporaryDirectory()
        self.addCleanup(outside.cleanup)
        pending_path = Path(outside.name) / "session.pending.json"
        for path in (extra, manifest_path, exported):
            with self.subTest(path=path), mock_patch.object(
                    official, "verify_sdk", return_value=manifest):
                pending, digest = self.begin_session(pending_path)
                path.write_bytes(path.read_bytes())
                with self.assertRaisesRegex(ValueError, "changed"):
                    self.end_session(pending, digest)
                self.assertFalse(pending.exists())

    def test_final_snapshot_closes_mutation_of_an_already_hashed_input(self):
        pending, digest = self.begin_session()
        early = self.cmsis / "device.h"
        late = self.source / "Src/stm32f4xx_ll_sdmmc.c"
        snapshot = official.cmsis_device_lock.stable_snapshot
        observations = 0

        def mutate(path):
            nonlocal observations
            result = snapshot(path)
            if Path(path) == early:
                observations += 1
            if Path(path) == late and observations == 2:
                early.write_bytes(early.read_bytes())
            return result

        with mock_patch.object(official.cmsis_device_lock, "stable_snapshot",
                               side_effect=mutate):
            with self.assertRaisesRegex(ValueError, "changed"):
                self.end_session(pending, digest)
        self.assertFalse(pending.exists())

    def test_git_snapshot_observer_cannot_refresh_or_inherit_git_state(self):
        self.git_patch.stop()
        completed = subprocess.CompletedProcess([], 0, "observed", "")
        with mock_patch.dict("os.environ", {"GIT_DIR": "wrong",
                                           "GIT_OPTIONAL_LOCKS": "1"}):
            with mock_patch.object(official.cmsis_device_lock.subprocess, "run",
                                   return_value=completed) as execute:
                official.cmsis_device_lock.git(self.source, "rev-parse", "HEAD")
        environment = execute.call_args.kwargs["env"]
        self.assertNotIn("GIT_DIR", environment)
        self.assertEqual(environment.get("GIT_OPTIONAL_LOCKS"), "0")

    def test_configure_snapshot_closes_recursive_symbolic_git_refs(self):
        marker = self.cmsis / ".git"
        (marker / "refs/heads").mkdir(parents=True)
        (marker / "HEAD").write_text("ref: refs/heads/alias\n")
        (marker / "refs/heads/alias").write_text("ref: refs/heads/selected\n")
        target = marker / "refs/heads/selected"
        target.write_text(official.CMSIS_COMMIT + "\n")
        pending, digest = self.begin_session()
        target.write_bytes(target.read_bytes())
        with self.assertRaisesRegex(ValueError, "changed"):
            self.end_session(pending, digest)
        self.assertFalse(pending.exists())

    def test_failed_end_consumes_only_recognized_own_pending_input(self):
        pending, digest = self.begin_session()
        pending.write_bytes(pending.read_bytes() + b"\n")
        with self.assertRaisesRegex(ValueError, "digest"):
            self.end_session(pending, digest)
        self.assertFalse(pending.exists())
        unrelated = self.root / "unrelated.json"
        unrelated.write_text('{"not": "configure inputs"}')
        with self.assertRaises(ValueError):
            self.end_session(unrelated, "0" * 64)
        self.assertTrue(unrelated.exists())

    def test_begin_returns_the_same_exact_input_closure_for_cmake(self):
        pending = self.root / "build/session.pending.json"
        pending.parent.mkdir(exist_ok=True)
        result = official.begin_configure(self.source, self.lock_path,
                                          self.cmsis, pending)
        self.assertIn(str(self.cmsis / "device.h"), result["configure_inputs"])
        self.assertIn(str(self.cmsis / ".git/index"), result["configure_inputs"])
        self.assertNotIn(str(self.cmsis / ".git"), result["configure_inputs"])
        self.end_session(pending, result["configure_snapshot_sha256"])

    def test_configure_admission_cannot_treat_missing_authority_as_optional_git(self):
        with mock_patch.object(official.dependencies, "source_names",
                return_value=("tools/configure/missing-authority-fixture.py",)):
            with self.assertRaisesRegex(ValueError, "missing"):
                self.begin_session()


if __name__ == "__main__":
    unittest.main()
