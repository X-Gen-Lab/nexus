"""Reviewed H7/U5 sources require stable official or complete SDK identity."""
from __future__ import annotations
from dataclasses import replace
import copy
import io
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch as mock_patch
from tools.maintenance import cmsis_device_lock, source_catalogue

ROOT = Path(__file__).resolve().parents[2]


class DeviceLockContracts:
    family = None

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            prefix="CMSIS device lock ")
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.source = self.folder / "official source"
        self.source.mkdir()
        self.original = ROOT / "vendors/st" / ("cmsis_device_" + self.family)
        name = "stm32" + self.family + ".lock.json"
        self.lock = json.loads((ROOT / "dependencies" / name).read_text())
        for name in self.lock["files"]:
            file = self.source / name
            file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(self.original / name, file)
        # Git is an external read-only identity port. Tests do not manufacture
        # commits or change the real complete upstream checkout.
        (self.source / ".git").mkdir()
        self.lock_path = self.folder / "source.lock.json"
        self.write_lock()
        self.git_state = {"HEAD": self.lock["commit"],
                          "origin": source_catalogue.upstream_dependency(
                              self.lock["repository"]).fetch_url, "status": ""}
        self.git_patch = mock_patch.object(cmsis_device_lock, "git",
                                          side_effect=self.git)
        self.git_patch.start()
        self.addCleanup(self.git_patch.stop)

    def write_lock(self, value=None):
        selected = self.lock if value is None else value
        self.lock_path.write_text(json.dumps(selected) + "\n")

    def git(self, source, *arguments):
        self.assertEqual(source, self.source)
        if arguments == ("rev-parse", "--show-toplevel"):
            return str(self.source)
        if arguments == ("rev-parse", "HEAD"):
            return self.git_state["HEAD"]
        if arguments == ("remote", "get-url", "origin"):
            return self.git_state.get("resolved_origin", self.git_state["origin"])
        if arguments == ("config", "--get-all", "remote.origin.url"):
            return self.git_state["origin"]
        if arguments == ("status", "--porcelain", "--untracked-files=all"):
            return self.git_state["status"]
        self.fail("Unexpected Git identity command: " + repr(arguments))

    def verify(self, **options):
        return cmsis_device_lock.verify(self.source, self.lock_path, **options)

    def relocate_to_sdk(self):
        sdk = self.folder / "prefix/share/nexus/src"
        destination = sdk / "vendors/st" / ("cmsis_device_" + self.family)
        destination.parent.mkdir(parents=True)
        self.source.rename(destination)
        self.source = destination
        name = "stm32" + self.family + ".lock.json"
        self.lock_path = sdk / "dependencies" / name
        self.lock_path.parent.mkdir()
        self.write_lock()
        return sdk

    def cli(self):
        return cmsis_device_lock.main(["--source", str(self.source),
                                       "--lock", str(self.lock_path)])

    def test_complete_reviewed_closure_verifies_without_hardware_claim(self):
        result = self.verify()
        self.assertEqual(result["kind"],
                         "nexus-cmsis-device-source-verification")
        self.assertEqual(result["commit"], self.lock["commit"])
        self.assertEqual(result["selected_part"], self.lock["selected_part"])
        self.assertEqual(result["verified_files"],
                         len(self.lock["files"]))
        self.assertEqual(result["license"], "Apache-2.0")
        self.assertEqual(result["source_kind"], "git_checkout")
        self.assertFalse(result["software_qualified"])

    def test_reviewed_transport_does_not_rewrite_official_device_lock(self):
        item = source_catalogue.upstream_dependency(self.lock["repository"])
        controlled = replace(item, fetch_url="https://github.com/Reviewed-Test/" +
                             item.upstream_url.rsplit("/", 1)[1])
        entries = tuple(controlled if record == item else record
                        for record in source_catalogue.ALL_DEPENDENCIES)
        self.git_state["origin"] = controlled.fetch_url
        with mock_patch.object(source_catalogue, "ALL_DEPENDENCIES", entries):
            self.assertEqual(self.verify()["commit"], self.lock["commit"])
            self.git_state["origin"] = self.lock["repository"]
            with self.assertRaises(ValueError):
                self.verify()

    def test_url_rewrite_cannot_hide_an_unapproved_declared_origin(self):
        self.git_state["resolved_origin"] = self.git_state["origin"]
        self.git_state["origin"] = "https://unreviewed.invalid/device.git"
        with self.assertRaises(ValueError):
            self.verify()

    def test_real_official_whole_checkout_and_cli_verify_read_only(self):
        self.git_patch.stop()
        name = "stm32" + self.family + ".lock.json"
        lock_path = ROOT / "dependencies" / name
        result = cmsis_device_lock.verify(self.original, lock_path)
        self.assertEqual(result["verified_files"],
                         len(self.lock["files"]))
        command = [sys.executable, "-B", str(ROOT /
                   "tools/maintenance/cmsis_device_lock.py"),
                   "--source", str(self.original), "--lock", str(lock_path)]
        executed = subprocess.run(command, capture_output=True, text=True,
                                  timeout=30)
        self.assertEqual(executed.returncode, 0,
                         executed.stdout + executed.stderr)
        self.assertEqual(json.loads(executed.stdout)["source_kind"],
                         "git_checkout")

    def test_empty_unknown_and_missing_lock_fields_reject(self):
        for value in ({}, [], {**self.lock, "fallback": True}):
            self.write_lock(value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.verify()
        for name in self.lock:
            value = copy.deepcopy(self.lock)
            del value[name]
            self.write_lock(value)
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()

    def test_duplicate_boolean_schema_and_nonfinite_json_reject(self):
        data = json.dumps(self.lock)
        for changed in (data.replace('"schema_version": 1',
                                    '"schema_version": 1, "schema_version": 1'),
                        data.replace('"schema_version": 1',
                                     '"schema_version": true'),
                        data.replace('"bytes": ', '"bytes": NaN, "other": ')):
            self.lock_path.write_text(changed)
            with self.subTest(data=changed[:64]), self.assertRaises(ValueError):
                self.verify()

    def test_reviewed_part_origin_and_fixed_commit_cannot_be_reauthored(self):
        shutil.rmtree(self.source / ".git")
        for key, value in (("selected_part", "STM32H750VBT6"),
                           ("selected_part", []),
                           ("repository", "https://example.com/device.git"),
                           ("commit", "a" * 40), ("commit", "main"),
                           ("commit", True)):
            changed = copy.deepcopy(self.lock)
            changed[key] = value
            self.write_lock(changed)
            with self.subTest(key=key, value=value):
                with self.assertRaises(ValueError):
                    self.verify(packaged_commit=changed["commit"])

    def test_reviewed_file_graph_cannot_omit_license_or_select_other_part(self):
        for name in self.lock["files"]:
            changed = copy.deepcopy(self.lock)
            del changed["files"][name]
            self.write_lock(changed)
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()
        for name in ("../escape.h", "/Include/device.h", "Include\\device.h",
                     "Include/unreviewed_device.h"):
            changed = copy.deepcopy(self.lock)
            changed["files"][name] = {"bytes": 1, "sha256": "a" * 64}
            self.write_lock(changed)
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify()

    def test_bytes_hash_and_nested_unknown_fields_reject(self):
        for record in ({"bytes": True, "sha256": "a" * 64},
                       {"bytes": 0, "sha256": "a" * 64},
                       {"bytes": 1, "sha256": "latest"},
                       {"bytes": 1, "sha256": "a" * 64, "fallback": 1}):
            changed = copy.deepcopy(self.lock)
            changed["files"]["LICENSE.md"] = record
            self.write_lock(changed)
            with self.subTest(record=record), self.assertRaises(ValueError):
                self.verify()
        for record in ({"path": "LICENSE.txt", "expression": "Apache-2.0"},
                       {"path": "LICENSE.md", "expression": "MIT"},
                       {**self.lock["license"], "fallback": "MIT"}):
            changed = copy.deepcopy(self.lock)
            changed["license"] = record
            self.write_lock(changed)
            with self.subTest(record=record), self.assertRaises(ValueError):
                self.verify()

    def test_changed_missing_and_empty_each_original_file_reject(self):
        for name in self.lock["files"]:
            file = self.source / name
            original = file.read_bytes()
            for replacement in (original + b"\n", b""):
                file.write_bytes(replacement)
                with self.subTest(name=name), self.assertRaises(ValueError):
                    self.verify()
            file.unlink()
            with self.subTest(missing=name), self.assertRaises(ValueError):
                self.verify()
            file.write_bytes(original)

    def test_files_source_parents_and_lock_symlinks_reject(self):
        alias = self.folder / "source alias"
        alias.symlink_to(self.source, target_is_directory=True)
        with self.assertRaises(ValueError):
            cmsis_device_lock.verify(alias, self.lock_path)
        alias = self.folder / "lock alias"
        alias.symlink_to(self.lock_path)
        with self.assertRaises(ValueError):
            cmsis_device_lock.verify(self.source, alias)
        file = self.source / self.lock["license"]["path"]
        actual = self.folder / "notice"
        file.rename(actual)
        file.symlink_to(actual)
        with self.assertRaises(ValueError):
            self.verify()
        file.unlink()
        actual.rename(file)
        folder = self.source / "Include"
        actual = self.folder / "real Include"
        folder.rename(actual)
        folder.symlink_to(actual, target_is_directory=True)
        with self.assertRaises(ValueError):
            self.verify()

    def test_actual_git_head_origin_or_whole_dirty_checkout_reject(self):
        for key, value in (("HEAD", "b" * 40),
                           ("origin", "https://example.com/device.git"),
                           ("status", " M unrelated_header.h"),
                           ("status", "?? unselected_untracked.txt")):
            original = self.git_state[key]
            self.git_state[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.verify()
            self.git_state[key] = original

    def test_dirty_checkout_rejects_before_selected_files_are_read(self):
        self.git_state["status"] = " M unrelated_header.h"
        with mock_patch.object(cmsis_device_lock, "snapshot",
                               wraps=cmsis_device_lock.snapshot) as snapshots:
            with self.assertRaises(ValueError):
                self.verify()
        source_reads = [c for c in snapshots.call_args_list
                        if Path(c.args[0]).is_relative_to(self.source)]
        self.assertEqual(source_reads, [], "Dirty identity checked too late")

    def test_git_identity_must_remain_exact_after_selected_files_are_read(self):
        heads = 0

        def changed(source, *arguments):
            nonlocal heads
            if arguments == ("rev-parse", "HEAD"):
                heads += 1
                if heads == 2:
                    return "c" * 40
            return self.git(source, *arguments)

        with mock_patch.object(cmsis_device_lock, "git", side_effect=changed):
            with self.assertRaises(ValueError):
                self.verify()

    def test_same_hash_source_and_lock_replacement_during_verify_reject(self):
        for file in (self.source / self.lock["license"]["path"], self.lock_path):
            heads = 0

            def rewritten(source, *arguments):
                nonlocal heads
                if arguments == ("rev-parse", "HEAD"):
                    heads += 1
                    if heads == 2:
                        replacement = file.with_name(file.name + ".replacement")
                        replacement.write_bytes(file.read_bytes())
                        replacement.replace(file)
                return self.git(source, *arguments)

            with self.subTest(file=file.name), mock_patch.object(
                    cmsis_device_lock, "git", side_effect=rewritten):
                with self.assertRaises(ValueError):
                    self.verify()

    def test_same_bytes_in_place_rewrite_and_rewrite_during_read_reject(self):
        original_snapshot = cmsis_device_lock.snapshot
        target = self.source / self.lock["license"]["path"]
        changed = False

        def rewritten(path):
            nonlocal changed
            result = original_snapshot(path)
            if Path(path) == target and not changed:
                changed = True
                target.write_bytes(result[0])
            return result

        with mock_patch.object(cmsis_device_lock, "snapshot",
                               side_effect=rewritten):
            with self.assertRaises(ValueError):
                self.verify()

    def test_non_git_requires_explicit_independently_verified_sdk_commit(self):
        shutil.rmtree(self.source / ".git")
        with self.assertRaises(ValueError):
            self.verify()
        for value in ("d" * 40, True, ""):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.verify(packaged_commit=value)
        result = self.verify(packaged_commit=self.lock["commit"])
        self.assertEqual(result["source_kind"], "installed_source_sdk")

    def test_packaged_commit_cannot_bypass_an_existing_git_checkout(self):
        with self.assertRaises(ValueError):
            self.verify(packaged_commit=self.lock["commit"])

    def test_git_marker_symlink_cannot_impersonate_a_checkout(self):
        marker = self.source / ".git"
        actual = self.folder / "foreign Git metadata"
        marker.rename(actual)
        marker.symlink_to(actual, target_is_directory=True)
        with self.assertRaises(ValueError):
            self.verify()

    def test_cli_calls_full_sdk_verify_before_component_verify(self):
        sdk = self.relocate_to_sdk()
        (sdk / ".nexus-source-sdk.json").write_text("{}\n")
        failure = ValueError("full SDK failed")
        with mock_patch.object(cmsis_device_lock, "verify_sdk",
                               side_effect=failure) as sdk_check:
            with mock_patch.object(cmsis_device_lock, "verify") as component:
                with mock_patch("sys.stderr",
                                new_callable=io.StringIO) as errors:
                    self.assertEqual(self.cli(), 1)
        sdk_check.assert_called_once_with(sdk)
        component.assert_not_called()
        self.assertIn("full SDK failed", errors.getvalue())

    def test_cli_uses_only_the_exact_verified_sdk_dependency_record(self):
        sdk = self.relocate_to_sdk()
        shutil.rmtree(self.source / ".git")
        (sdk / ".nexus-source-sdk.json").write_text("{}\n")
        record = {"path": "vendors/st/cmsis_device_" + self.family,
                  "commit": self.lock["commit"], "tree": "e" * 40}
        manifest = {"dependencies": [record]}
        with mock_patch.object(cmsis_device_lock, "verify_sdk",
                               return_value=manifest) as checked:
            with mock_patch("sys.stdout", new_callable=io.StringIO) as output:
                self.assertEqual(self.cli(), 0)
        checked.assert_called_once_with(sdk)
        self.assertEqual(json.loads(output.getvalue())["source_kind"],
                         "installed_source_sdk")
        for records in ([], [record, record], [{**record, "commit": "f" * 40}],
                        [{**record, "path": "vendors/st/other_device"}]):
            manifest = {"dependencies": records}
            with self.subTest(records=records), mock_patch.object(
                    cmsis_device_lock, "verify_sdk", return_value=manifest):
                with mock_patch("sys.stderr", new_callable=io.StringIO):
                    self.assertEqual(self.cli(), 1)

    def test_cli_sdk_path_mismatch_and_dangling_marker_reject(self):
        sdk = self.relocate_to_sdk()
        marker = sdk / ".nexus-source-sdk.json"
        marker.symlink_to(sdk / "missing.json")
        with mock_patch.object(cmsis_device_lock, "verify_sdk") as checked:
            with mock_patch("sys.stderr", new_callable=io.StringIO):
                self.assertEqual(self.cli(), 1)
        checked.assert_not_called()
        marker.unlink()
        marker.write_text("{}\n")
        self.lock_path.rename(self.folder / "outside.lock.json")
        self.lock_path = self.folder / "outside.lock.json"
        with mock_patch.object(cmsis_device_lock, "verify_sdk") as checked:
            with mock_patch("sys.stderr", new_callable=io.StringIO):
                self.assertEqual(self.cli(), 1)
        checked.assert_not_called()

    def test_cli_same_hash_manifest_replacement_during_full_check_rejects(self):
        sdk = self.relocate_to_sdk()
        shutil.rmtree(self.source / ".git")
        marker = sdk / ".nexus-source-sdk.json"
        marker.write_text("{}\n")

        def rewritten(root):
            self.assertEqual(root, sdk)
            replacement = sdk / "replacement.json"
            replacement.write_bytes(marker.read_bytes())
            replacement.replace(marker)
            path = "vendors/st/cmsis_device_" + self.family
            return {"dependencies": [{"path": path,
                                       "commit": self.lock["commit"],
                                       "tree": "e" * 40}]}

        with mock_patch.object(cmsis_device_lock, "verify_sdk",
                               side_effect=rewritten):
            with mock_patch("sys.stderr", new_callable=io.StringIO):
                with mock_patch("sys.stdout", new_callable=io.StringIO):
                    self.assertEqual(self.cli(), 1)

    def test_real_cli_rejects_non_git_and_invalid_full_sdk_manifest(self):
        sdk = self.relocate_to_sdk()
        shutil.rmtree(self.source / ".git")
        command = [sys.executable, "-B", str(ROOT /
                   "tools/maintenance/cmsis_device_lock.py"),
                   "--source", str(self.source), "--lock", str(self.lock_path)]
        for marker in (False, True):
            if marker:
                (sdk / ".nexus-source-sdk.json").write_text("{}\n")
            result = subprocess.run(command, capture_output=True, text=True,
                                    timeout=30)
            with self.subTest(marker=marker):
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("rejected", result.stderr)


class STM32F1LockTests(DeviceLockContracts, unittest.TestCase):
    family = "f1"


class STM32H7LockTests(DeviceLockContracts, unittest.TestCase):
    family = "h7"

    def test_axi_boot_reference_is_in_verified_and_packaged_closure(self):
        name = "Source/Templates/system_stm32h7xx.c"
        self.assertIn(name, self.lock["files"])
        self.assertIn(name, cmsis_device_lock.contract(self.lock))
        path = ROOT / "cmake/package/package_source_sdk.py"
        spec = importlib.util.spec_from_file_location("boot_reference_sdk", path)
        package_source_sdk = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(package_source_sdk)
        self.assertIn("vendors/st/cmsis_device_h7/" + name,
                      package_source_sdk.REQUIRED_FILES)


class STM32U5LockTests(DeviceLockContracts, unittest.TestCase):
    family = "u5"


if __name__ == "__main__":
    unittest.main()
