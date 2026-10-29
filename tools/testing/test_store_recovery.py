"""Artifact recovery resumes byte verification, never cached test success."""

from copy import deepcopy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence import store
from tools.evidence.common import EvidenceError, file_identity, load_json


class StoreRecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        source = self.root / "source"
        source.mkdir()
        (source / "empty").mkdir()
        (source / "result.txt").write_text("original executed output\n")
        (source / "zero.txt").touch()
        self.package = store.export_tree(source, self.root / "store")
        self.envelope = self.root / "package.json"
        self.envelope.write_text(json.dumps(self.package))
        self.plan = {"schema_version": 1, "kind": "evidence_recovery_plan",
                     "reserve_bytes": 8192, "packages": [{
                         "name": "platform", "package": file_identity(self.envelope),
                         "archive": self.package["archive"]}]}
        self.output = self.root / "recovery"
        self.source = {"commit": "fixture-source", "dirty": False}

    def recover(self, *, plan=None, resume=False):
        with patch("tools.evidence.identity.observe_git_source",
                   return_value=self.source):
            return store.recover_packages(
                self.plan if plan is None else plan, self.output,
                resume=resume)

    def test_restores_original_bytes_and_preserves_source_inputs(self):
        before = self.envelope.read_bytes()
        result = self.recover()
        self.assertEqual(result["status"], "recovered_verified")
        self.assertIs(result["software_qualified"], False)
        self.assertEqual(result["physical_status"], "not_executed")
        self.assertEqual((self.output / "payload/platform/result.txt").read_text(),
                         "original executed output\n")
        self.assertTrue((self.output / "payload/platform/empty").is_dir())
        self.assertTrue((self.output / "payload/platform/zero.txt").is_file())
        self.assertEqual(self.envelope.read_bytes(), before)
        self.assertEqual(result["packages"][0]["operation"], "restored")
        self.assertEqual(load_json(Path(result["packages"][0]["receipt"]["path"]))[
            "exit_code"], 0)

    def test_resume_rechecks_existing_members_with_a_fresh_command(self):
        first = self.recover()
        original = Path(first["packages"][0]["receipt"]["path"])
        receipt = original.read_bytes()
        payload = self.output / "payload/platform/result.txt"
        inode = payload.stat().st_ino
        second = self.recover(resume=True)
        self.assertEqual(second["packages"][0]["operation"],
                         "existing_members_reverified")
        self.assertNotEqual(first["attempt"], second["attempt"])
        self.assertEqual(original.read_bytes(), receipt)
        self.assertEqual(payload.stat().st_ino, inode)
        self.assertEqual(load_json(Path(second["previous"]["path"])), first)

    def test_resume_rejects_changed_source_or_plan_before_new_attempt(self):
        self.recover()
        before = sorted(self.output.rglob("*"))
        self.source["commit"] = "another-source"
        with self.assertRaisesRegex(EvidenceError, "source|fingerprint"):
            self.recover(resume=True)
        self.source["commit"] = "fixture-source"
        changed = deepcopy(self.plan)
        changed["reserve_bytes"] += 1
        with self.assertRaisesRegex(EvidenceError, "plan|fingerprint"):
            self.recover(plan=changed, resume=True)
        self.assertEqual(sorted(self.output.rglob("*")), before)

    def test_no_implicit_resume_and_no_output_tree_as_input(self):
        self.recover()
        with self.assertRaisesRegex(EvidenceError, "resume"):
            self.recover()
        altered = deepcopy(self.plan)
        altered["packages"][0]["archive"]["path"] = str(
            self.output / "payload/platform/result.txt")
        with self.assertRaises(EvidenceError):
            self.recover(plan=altered, resume=True)

    def test_capacity_rejects_before_any_recovery_output(self):
        with patch("tools.evidence.store.shutil.disk_usage",
                   return_value=type("Usage", (), {"free": 1})()):
            with self.assertRaisesRegex(EvidenceError, "capacity"):
                self.recover()
        self.assertFalse(self.output.exists())

    def test_archive_and_package_changes_fail_before_output_creation(self):
        self.envelope.write_text("{}")
        with self.assertRaises(EvidenceError):
            self.recover()
        self.assertFalse(self.output.exists())

    def test_resume_does_not_accept_changed_existing_payload(self):
        self.recover()
        payload = self.output / "payload/platform/result.txt"
        payload.write_text("substituted\n")
        with self.assertRaisesRegex(EvidenceError, "member|manifest"):
            self.recover(resume=True)
        self.assertEqual(payload.read_text(), "substituted\n")

    def test_plan_rejects_duplicate_names_and_unknown_fields(self):
        invalid = []
        changed = deepcopy(self.plan)
        changed["packages"].append(deepcopy(changed["packages"][0]))
        invalid.append(changed)
        changed = deepcopy(self.plan)
        changed["reserve_bytes"] = True
        invalid.append(changed)
        changed = deepcopy(self.plan)
        changed["packages"][0]["name"] = "../escape"
        invalid.append(changed)
        changed = deepcopy(self.plan)
        changed["run_validator"] = ["untrusted"]
        invalid.append(changed)
        for plan in invalid:
            with self.subTest(plan=plan), self.assertRaises(EvidenceError):
                self.recover(plan=plan)
        self.assertFalse(self.output.exists())

    def test_resume_keeps_unknown_previous_operation_as_unknown(self):
        first = self.recover()
        first["status"] = "running"
        first["packages"] = []
        state = self.output / "recovery.json"
        state.write_text(json.dumps(first))
        second = self.recover(resume=True)
        previous = load_json(Path(second["previous"]["path"]))
        self.assertEqual(previous["status"], "running")
        self.assertNotIn("exit_code", previous)
        self.assertEqual(second["packages"][0]["operation"],
                         "existing_members_reverified")

    def test_existing_recovery_lock_blocks_concurrent_mutation(self):
        import fcntl

        self.output.mkdir()
        with (self.output / ".recovery.lock").open("w") as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            # A completed previous view still cannot authorize a concurrent
            # second writer; the kernel releases the lock after actual exit.
            self.recover_state = {"kind": "evidence_recovery",
                                  "source": self.source,
                                  "destination": str(self.output)}
            from tools.evidence.identity import canonical_digest
            self.recover_state["plan_sha256"] = canonical_digest(self.plan)
            (self.output / "recovery.json").write_text(json.dumps(self.recover_state))
            with self.assertRaisesRegex(EvidenceError, "active|lock"):
                self.recover(resume=True)
        self.assertFalse((self.output / "payload").exists())

    def test_cli_rejects_malformed_plan_without_replacing_it(self):
        path = self.root / "malformed.json"
        path.write_text('{"packages":[{}]}')
        original = path.read_bytes()
        result = store.main(["recover", "--plan", str(path),
                             "--destination", str(self.output),
                             "--report", str(self.root / "result.json")])
        self.assertEqual(result, 1)
        self.assertEqual(path.read_bytes(), original)
        self.assertFalse(self.output.exists())

    def test_resume_rejects_unselected_payload_targets_before_new_attempt(self):
        self.recover()
        extra = self.output / "payload/unselected"
        extra.mkdir()
        before = sorted(self.output.rglob("*"))
        with self.assertRaisesRegex(EvidenceError, "unselected|target"):
            self.recover(resume=True)
        self.assertEqual(sorted(self.output.rglob("*")), before)

    def test_failed_owned_command_remains_explicitly_navigable(self):
        from tools.dev import process

        original = process.run

        def failed(argv, **kwargs):
            # The external runner really records a failing child. This is an
            # isolated launch fixture, not a successful archive verification.
            import sys
            return original([sys.executable, "-c", "raise SystemExit(7)"], **kwargs)

        with patch.object(process, "run", side_effect=failed):
            with self.assertRaisesRegex(EvidenceError, "nonzero"):
                self.recover()
        state = load_json(self.output / "recovery.json")
        self.assertEqual(state["status"], "failed")
        self.assertEqual(state["active"]["name"], "platform")
        receipt = load_json(Path(state["active"]["command_view"]))
        self.assertEqual(receipt["exit_code"], 7)
        self.assertEqual(state["packages"], [])

    def test_unknown_active_command_blocks_resume_without_rewriting_it(self):
        state = self.recover()
        state.update(status="running", active={"name": "platform",
                     "argv": ["unknown"], "command_view": str(
                         self.output / "attempts" / state["attempt"] /
                         "unknown.command.json")})
        path = self.output / "recovery.json"
        path.write_text(json.dumps(state))
        before = path.read_bytes()
        with self.assertRaisesRegex(EvidenceError, "unfinished|withdrawal|unknown"):
            self.recover(resume=True)
        self.assertEqual(path.read_bytes(), before)

    def test_restore_capacity_accounts_for_zero_files_and_directory_blocks(self):
        measured = store.restore_capacity(self.package, reserve_bytes=4096)
        self.assertEqual(measured["expanded_bytes"], self.package["bytes"])
        self.assertEqual(measured["files"], 2)
        self.assertEqual(measured["directories"], 1)
        self.assertGreater(measured["required_bytes"],
                           measured["expanded_bytes"] + 4096)
        self.assertGreater(measured["metadata_reserve_bytes"], 0)


if __name__ == "__main__":
    unittest.main()
