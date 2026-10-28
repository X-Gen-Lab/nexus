"""Local plan verification uses actual small commands, never a platform CI pass."""

import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.dev import qualification
from tools.evidence.common import EvidenceError, atomic_json, file_identity


class LocalPlanFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nexus-local-plan-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.root = self.directory / "source"
        self.root.mkdir()
        checker = self.root / "tools/testing/run_tool_tests.py"
        checker.parent.mkdir(parents=True)
        checker.write_text("import sys\nprint('small verifier model', sys.argv[1:])\n")
        self.plan = {"schema_version": 1, "formal": True, "stages": [
            {"name": "first", "checker": "tool_tests", "args": ["first"]},
            {"name": "second", "checker": "tool_tests", "args": ["second"]}]}
        plan_path = self.root / "tools/dev/plans/software.json"
        plan_path.parent.mkdir(parents=True)
        atomic_json(plan_path, self.plan)
        self.git("init", "-q")
        self.git("add", ".")
        self.git("-c", "user.name=Local CI Test", "-c",
                 "user.email=local-ci@example.invalid", "commit", "-qm", "fixture")
        self.output = self.directory / "execution"
        self.report = qualification.execute(self.plan, self.root, self.output)
        self.path = self.output / "report.json"
        self.assertEqual(self.report["status"], "executed_passed")

    def git(self, *args):
        subprocess.run(["git", "-C", str(self.root), *args],
                       check=True, capture_output=True)

    def verify(self):
        return qualification.verify_execution(self.plan, self.path, self.root)

    def write_report(self, value):
        atomic_json(self.path, value)


class LocalPlanTests(LocalPlanFixture):
    def test_actual_complete_plan_binds_source_and_preserves_report_bytes(self):
        before = self.path.read_bytes()
        result = self.verify()
        self.assertEqual(result["status"], "verified")
        self.assertEqual(result["source"], self.report["source_before"])
        self.assertEqual(result["stage_count"], 2)
        self.assertEqual(result["report"], file_identity(self.path))
        self.assertEqual(result["plan_sha256"], self.report["plan_sha256"])
        self.assertFalse(result["software_qualified"])
        self.assertEqual(self.path.read_bytes(), before)

    def test_partial_reordered_skipped_and_forged_success_are_rejected(self):
        for mutation in ("missing-stage", "reordered", "duplicate", "plan",
                         "running", "failed", "dirty", "source-after",
                         "formal", "skipped", "exit", "boolean-exit"):
            value = copy.deepcopy(self.report)
            if mutation == "missing-stage":
                value["stages"].pop()
            elif mutation == "reordered":
                value["stages"].reverse()
            elif mutation == "duplicate":
                value["stages"][1] = value["stages"][0]
            elif mutation == "plan":
                value["plan_sha256"] = "0" * 64
            elif mutation in ("running", "failed"):
                value["status"] = mutation
            elif mutation == "dirty":
                value["source_before"]["dirty"] = True
            elif mutation == "source-after":
                value["source_after"]["tree"] = "0" * 40
            elif mutation == "formal":
                value["formal"] = False
            elif mutation == "skipped":
                value["stages"][0]["status"] = "skipped"
            else:
                value["stages"][0]["exit_code"] = False if mutation == "boolean-exit" else 7
            self.write_report(value)
            with self.subTest(mutation=mutation), self.assertRaises(EvidenceError):
                self.verify()

    def test_changed_checker_or_current_source_cannot_inherit_a_pass(self):
        checker = self.root / "tools/testing/run_tool_tests.py"
        checker.write_text("raise SystemExit(0)\n")
        with self.assertRaises(EvidenceError):
            self.verify()

    def test_raw_receipt_and_unique_attempt_must_all_survive(self):
        stage = self.report["stages"][0]
        alias = Path(stage["command_receipt"]["path"])
        receipt = json.loads(alias.read_text())
        paths = [Path(stage["raw"]["path"]), alias,
                 Path(receipt["attempt_receipt"]), Path(receipt["stdout"]["path"]),
                 Path(receipt["stderr"]["path"])]
        for path in paths:
            original = path.read_bytes()
            path.write_bytes(original + b"changed")
            with self.subTest(path=path.name), self.assertRaises(EvidenceError):
                self.verify()
            path.write_bytes(original)

    def test_mutated_command_semantics_cannot_be_rehashed_into_success(self):
        original = copy.deepcopy(self.report)
        identity = original["stages"][0]["command_receipt"]
        alias = Path(identity["path"])
        alias_bytes = alias.read_bytes()
        actual = json.loads(alias_bytes)
        attempt = Path(actual["attempt_receipt"])
        attempt_bytes = attempt.read_bytes()
        for field, value in (("outcome", "timed_out"), ("child_returncode", 7),
                             ("cwd", str(self.directory)), ("timeout_seconds", 1),
                             ("withdrawal_complete", False),
                             ("command", ["true"])):
            changed = {**actual, field: value}
            atomic_json(alias, changed)
            atomic_json(attempt, {key: value for key, value in changed.items()
                                  if key != "attempt_receipt"})
            report = copy.deepcopy(original)
            report["stages"][0]["command_receipt"] = file_identity(alias)
            self.write_report(report)
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                self.verify()
            alias.write_bytes(alias_bytes)
            attempt.write_bytes(attempt_bytes)

    def test_empty_actual_output_is_valid_with_completed_command_receipts(self):
        checker = self.root / "tools/testing/run_tool_tests.py"
        checker.write_text("pass\n")
        self.git("add", ".")
        self.git("-c", "user.name=Local CI Test", "-c",
                 "user.email=local-ci@example.invalid", "commit", "-qm", "silent")
        self.output = self.directory / "silent-execution"
        self.report = qualification.execute(self.plan, self.root, self.output)
        self.path = self.output / "report.json"
        self.assertEqual(self.report["stages"][0]["raw"]["size"], 0)
        self.assertEqual(self.verify()["status"], "verified")

    def test_checker_argv_and_source_receipt_must_match_the_owned_plan(self):
        for field, value in (("argv", ["true"]),
                             ("checker_source", file_identity(self.path))):
            report = copy.deepcopy(self.report)
            report["stages"][0][field] = value
            self.write_report(report)
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                self.verify()

    def test_rehashed_log_cannot_disagree_with_its_actual_stdout_and_stderr(self):
        report = copy.deepcopy(self.report)
        raw = Path(report["stages"][0]["raw"]["path"])
        raw.write_text("replacement summary without actual output\n")
        report["stages"][0]["raw"] = file_identity(raw)
        self.write_report(report)
        with self.assertRaisesRegex(EvidenceError, "output"):
            self.verify()


class StatusPublicationTests(LocalPlanFixture):
    """The remote API is a model port; no status is sent to GitHub by tests."""

    def api(self, *, changed=None):
        commit = self.report["source_before"]["commit"]
        snapshot = {"number": 3, "state": "open", "merged": False,
                    "head": {"sha": commit,
                             "repo": {"full_name": "model/platform"}},
                    "base": {"sha": "a" * 40, "ref": "main",
                             "repo": {"full_name": "model/platform"}}}

        class Api:
            repository = "model/platform"
            base = "https://api.github.com/repos/model/platform"

            def __init__(self):
                self.gets = []
                self.posts = []

            def get(self, path):
                self.gets.append(path)
                return copy.deepcopy(changed if changed and len(self.gets) > 1
                                     else snapshot)

            def request(self, method, url, payload=None):
                self.posts.append((method, url, payload))
                return {"id": 17, "state": payload["state"],
                        "context": payload["context"],
                        "url": url, "description": payload["description"]}

        return Api(), snapshot

    def test_publisher_uses_exact_sha_fixed_context_and_two_pr_observations(self):
        from tools.maintenance import local_ci
        api, snapshot = self.api()
        output = self.directory / "publication.json"
        result = local_ci.publish(self.path, self.root, api, 3, output)
        self.assertEqual(api.gets, ["/pulls/3", "/pulls/3"])
        self.assertEqual(len(api.posts), 1)
        method, url, payload = api.posts[0]
        self.assertEqual((method, url), (
            "POST", api.base + "/statuses/" + snapshot["head"]["sha"]))
        self.assertEqual(payload["context"], "nexus/local-software-plan")
        self.assertEqual(payload["state"], "success")
        self.assertEqual(result["publication"], "confirmed")
        self.assertEqual(result, json.loads(output.read_text()))
        self.assertFalse(result["software_qualified"])

    def test_pr_head_or_base_change_prevents_any_success_post(self):
        from tools.maintenance import local_ci
        _, initial = self.api()
        for key in ("head", "base"):
            changed = copy.deepcopy(initial)
            changed[key]["sha"] = "b" * 40
            api, _ = self.api(changed=changed)
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                local_ci.publish(self.path, self.root, api, 3,
                                 self.directory / (key + ".json"))
            self.assertEqual(api.posts, [])

    def test_failed_plan_prevents_remote_publication(self):
        from tools.maintenance import local_ci
        self.write_report({**self.report, "status": "failed"})
        api, _ = self.api()
        with self.assertRaises(EvidenceError):
            local_ci.publish(self.path, self.root, api, 3,
                             self.directory / "failed.json")
        self.assertEqual(api.posts, [])

    def test_offline_cli_uses_only_the_maintained_plan(self):
        from tools.maintenance import local_ci
        stream = io.StringIO()
        with patch.object(local_ci, "GitHub",
                          side_effect=AssertionError("unexpected network")), \
                redirect_stdout(stream):
            result = local_ci.main(["--root", str(self.root),
                                    "--report", str(self.path)])
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(stream.getvalue())["status"], "verified")

    def test_remote_rejection_retains_request_without_claiming_publication(self):
        from tools.maintenance import local_ci
        api, _ = self.api()
        output = self.directory / "denied.json"
        with patch.object(api, "request", side_effect=ValueError("GitHub 403")):
            with self.assertRaisesRegex(ValueError, "403"):
                local_ci.publish(self.path, self.root, api, 3, output)
        record = json.loads(output.read_text())
        self.assertEqual(record["publication"], "unconfirmed")
        self.assertEqual(record["request"]["payload"]["context"],
                         "nexus/local-software-plan")
        self.assertEqual(record["failure"], "GitHub 403")
        self.assertNotIn("response", record)


if __name__ == "__main__":
    unittest.main()
