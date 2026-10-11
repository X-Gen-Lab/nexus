"""Weekly maintenance cannot silently promote a proposed dependency."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import subprocess
import sys
from tools.maintenance import dependency_proposals as proposals

ROOT = Path(__file__).resolve().parents[2]

class DependencyProposalTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.archive = self.folder / "candidate.tar"; self.archive.write_bytes(b"candidate source archive")
        self.notice = self.folder / "LICENSE.txt"; self.notice.write_bytes(b"Candidate license notice")
        self.plan = {"schema_version": 1, "dependency": "freertos", "proposed_revision": "b"*40,
                     "source_url": "https://example.com/source.tar", "archive": self.file(self.archive),
                     "license": {"expression": "MIT", "notices": [self.file(self.notice)]}, "evidence": {}}
        self.current = {"dependencies": [{"id": "freertos", "revision": "a"*40, "kind": "git_source"}]}

    def file(self, path):
        data = path.read_bytes()
        return {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(), "size": len(data)}

    def test_actual_inventory_observes_pins_not_latest_claims(self):
        result = proposals.inventory(ROOT)
        self.assertEqual(result["owner"], "codex")
        self.assertEqual(result["mode"], "weekly_read_only")
        self.assertFalse(result["locks_changed"])
        self.assertFalse(result["upgrade_approved"])
        self.assertEqual(result["network_reads"], 0)
        self.assertGreaterEqual(len(result["dependencies"]), 8)
        for record in result["dependencies"]:
            self.assertTrue(record["revision"])
            self.assertTrue(record["identities"])

    def test_archive_and_license_identity_produce_only_pending_proposal(self):
        result = proposals.propose(self.plan, self.current)
        self.assertEqual(result["status"], "requires_review_and_execution")
        self.assertFalse(result["upgrade_approved"])
        self.assertFalse(result["locks_changed"])
        self.assertEqual(set(result["gate_matrix"]), set(proposals.GATES))
        self.assertTrue(all(v["status"] == "not_executed" for v in result["gate_matrix"].values()))

    def test_missing_or_unknown_plan_field_rejects(self):
        for key in self.plan:
            plan = copy.deepcopy(self.plan); del plan[key]
            with self.subTest(key=key), self.assertRaises(ValueError): proposals.propose(plan, self.current)
        plan = copy.deepcopy(self.plan); plan["auto_merge"] = True
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_unknown_dependency_and_same_revision_reject(self):
        for dependency, revision in (("unknown", "b"*40), ("freertos", "a"*40)):
            plan = copy.deepcopy(self.plan); plan.update(dependency=dependency, proposed_revision=revision)
            with self.subTest(dependency=dependency), self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_unpinned_git_revision_rejects(self):
        for revision in ("main", "latest", "b"*39, True):
            plan = copy.deepcopy(self.plan); plan["proposed_revision"] = revision
            with self.subTest(revision=revision), self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_candidate_archive_changed_or_empty_rejects(self):
        for content in (b"changed", b""):
            self.archive.write_bytes(content)
            with self.subTest(content=content), self.assertRaises(ValueError): proposals.propose(self.plan, self.current)

    def test_candidate_archive_symlink_rejects(self):
        alias = self.folder / "alias.tar"; alias.symlink_to(self.archive)
        plan = copy.deepcopy(self.plan); plan["archive"]["path"] = str(alias)
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_missing_or_changed_license_notice_rejects(self):
        self.notice.write_bytes(b"different terms")
        with self.assertRaises(ValueError): proposals.propose(self.plan, self.current)
        self.notice.unlink()
        with self.assertRaises(ValueError): proposals.propose(self.plan, self.current)

    def test_license_is_reviewed_not_guessed(self):
        for expression in ("", "unknown", True):
            plan = copy.deepcopy(self.plan); plan["license"]["expression"] = expression
            with self.subTest(expression=expression), self.assertRaises(ValueError): proposals.propose(plan, self.current)
        plan = copy.deepcopy(self.plan); plan["license"]["notices"] = []
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_bare_passed_marker_never_approves_upgrade(self):
        report = self.folder / "fake.json"; report.write_text('{"status":"passed"}')
        plan = copy.deepcopy(self.plan); plan["evidence"] = {gate: [self.file(report)] for gate in proposals.GATES}
        result = proposals.propose(plan, self.current)
        self.assertFalse(result["upgrade_approved"])
        self.assertTrue(all(v["status"] == "referenced_unreviewed" for v in result["gate_matrix"].values()))
        self.assertFalse(result["software_qualified"])

    def test_unknown_evidence_gate_and_empty_reference_rejects(self):
        for evidence in ({"fake_gate": [self.file(self.archive)]}, {"board_arm": []}):
            plan = copy.deepcopy(self.plan); plan["evidence"] = evidence
            with self.subTest(evidence=evidence), self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_stale_evidence_reference_rejects(self):
        plan = copy.deepcopy(self.plan); plan["evidence"] = {"configuration": [self.file(self.notice)]}
        self.notice.write_bytes(b"new report")
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_sources_with_credentials_or_non_https_reject(self):
        for url in ("http://example.com/src", "https://user:secret@example.com/src", "file:///tmp/source", "https://example.com/src?token=private"):
            plan = copy.deepcopy(self.plan); plan["source_url"] = url
            with self.subTest(url=url), self.assertRaises(ValueError): proposals.propose(plan, self.current)

    def test_proposal_does_not_modify_inputs_or_locks(self):
        before = {p: p.read_bytes() for p in (self.archive, self.notice, ROOT/'dependencies/toolchains.lock.json', ROOT/'dependencies/googletest.lock.json')}
        proposals.propose(self.plan, self.current)
        self.assertEqual(before, {p: p.read_bytes() for p in before})

    def test_boolean_schema_is_rejected(self):
        plan = copy.deepcopy(self.plan); plan["schema_version"] = True
        with self.assertRaises(ValueError): proposals.propose(plan, self.current)


    def upstream_current(self):
        return {"dependencies": [{"id": "freertos", "revision": "a"*40,
                                  "kind": "git_source",
                                  "upstream_url": "https://github.com/FreeRTOS/FreeRTOS-Kernel.git"}]}

    def capture(self, step, substitutions, output):
        response = output / (step["name"] + ".stdout.bin")
        error = output / (step["name"] + ".stderr.bin")
        response.write_bytes(b"b"*40 + b"\trefs/tags/V11.3.0\n")
        error.write_bytes(b"")
        return {"status": "captured", "process_started": True,
                "exit_code": 0, "argv": step["arguments"],
                "stdout": self.file(response),
                "stderr": {"path": str(error), "sha256": hashlib.sha256(b"").hexdigest(), "size": 0}}

    def test_explicit_upstream_records_raw_refs_without_upgrading(self):
        output = self.folder / "upstream"
        result = proposals.observe_upstream(self.upstream_current(), output, runner=self.capture)
        self.assertFalse(result["locks_changed"])
        self.assertFalse(result["upgrade_approved"])
        self.assertEqual(result["observations"][0]["status"], "observed")
        self.assertEqual(result["observations"][0]["refs"][0]["revision"], "b"*40)
        self.assertTrue((output / "observations.json").is_file())

    def test_failed_network_stays_not_observed_with_actual_exit(self):
        def failure(step, substitutions, output):
            result = self.capture(step, substitutions, output)
            result.update(status="failed", exit_code=128)
            return result
        result = proposals.observe_upstream(self.upstream_current(), self.folder / "failed", runner=failure)
        self.assertEqual(result["observations"][0]["status"], "not_observed")
        self.assertEqual(result["observations"][0]["request"]["exit_code"], 128)

    def test_upstream_origin_without_public_allowed_host_rejects_before_request(self):
        current = self.upstream_current(); current["dependencies"][0]["upstream_url"] = "https://user:password@evil.example/source.git"
        with self.assertRaises(ValueError):
            proposals.observe_upstream(current, self.folder / "bad", runner=lambda *_: self.fail("unexpected request"))

    def test_existing_or_symlink_output_cannot_replace_evidence(self):
        output = self.folder / "retained"; output.mkdir(); (output / "raw.bin").write_bytes(b"retained")
        with self.assertRaises(ValueError): proposals.observe_upstream(self.upstream_current(), output, runner=self.capture)
        alias = self.folder / "alias"; alias.symlink_to(output, target_is_directory=True)
        with self.assertRaises(ValueError): proposals.observe_upstream(self.upstream_current(), alias / "new", runner=self.capture)
        self.assertEqual((output / "raw.bin").read_bytes(), b"retained")

    def test_malformed_remote_refs_do_not_become_observed(self):
        def malformed(step, substitutions, output):
            result = self.capture(step, substitutions, output)
            Path(result["stdout"]["path"]).write_bytes(b"not a ref response\n")
            result["stdout"] = self.file(Path(result["stdout"]["path"]))
            return result
        result = proposals.observe_upstream(self.upstream_current(), self.folder / "malformed", runner=malformed)
        self.assertEqual(result["observations"][0]["status"], "not_observed")

    def test_upstream_invocation_is_no_shell_noninteractive_and_bounded(self):
        def checked(step, substitutions, output):
            self.assertLessEqual(step["timeout_s"], 10)
            self.assertLessEqual(step["maximum_output_bytes"], 1024*1024)
            arguments = step["arguments"]
            self.assertIn("GIT_CONFIG_GLOBAL=/dev/null", arguments)
            self.assertIn("GIT_TERMINAL_PROMPT=0", arguments)
            self.assertIn("ls-remote", arguments)
            self.assertIn("--refs", arguments)
            return self.capture(step, substitutions, output)
        proposals.observe_upstream(self.upstream_current(), self.folder / "bounded", runner=checked)


    def test_upstream_git_does_not_inherit_capture_repository_local_configuration(self):
        def checked(step, substitutions, output):
            arguments = step["arguments"]
            self.assertIn("-C", arguments)
            directory = Path(arguments[arguments.index("-C") + 1])
            self.assertTrue(directory.is_dir())
            self.assertFalse((directory / ".git").exists())
            self.assertIn("GIT_CEILING_DIRECTORIES=" + str(directory.parent), arguments)
            return self.capture(step, substitutions, output)
        proposals.observe_upstream(self.upstream_current(), self.folder / "isolated", runner=checked)


    def test_parent_traversal_cannot_write_from_build_into_source(self):
        path = ROOT / "build" / ".." / "dependencies" / "proposal-output-must-not-exist"
        with self.assertRaises(ValueError):
            proposals.observe_upstream(self.upstream_current(), path, runner=lambda *_: self.fail("unexpected request"))
        self.assertFalse((ROOT / "dependencies/proposal-output-must-not-exist").exists())

if __name__ == "__main__": unittest.main()
