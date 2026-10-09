"""Regression tests for the aggregate CI gate, using only the stdlib."""

import contextlib
import copy
import io
import itertools
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from check_required_jobs import check_required_jobs, main, required_jobs


def needs_for(code="false", docs="false", workflows="false"):
    return {
        "changes": {
            "result": "success",
            "outputs": {"code": code, "docs": docs, "workflows": workflows},
        },
        "build-test": {"result": "skipped"},
        "code-quality": {"result": "success"},
        "docs": {"result": "skipped"},
    }


class RequiredJobsTests(unittest.TestCase):
    def test_trigger_truth_table_and_gate_results(self):
        # Independent policy table: code affects all gates; workflow edits affect
        # build/static analysis; style checks always run inside quality.
        for event in ("push", "pull_request", "schedule", "workflow_dispatch"):
            for code, docs, workflows in itertools.product((False, True), repeat=3):
                expected = {
                    "build-test": event in ("schedule", "workflow_dispatch") or code or workflows,
                    "code-quality": True,
                    "docs": event in ("schedule", "workflow_dispatch") or code or docs,
                }
                needs = needs_for(*(str(flag).lower() for flag in (code, docs, workflows)))
                with self.subTest(event=event, code=code, docs=docs, workflows=workflows):
                    self.assertEqual(required_jobs(needs["changes"]["outputs"], event), expected)
                for statuses in itertools.product(
                    ("success", "skipped", "failure", "cancelled"), repeat=3
                ):
                    for job, status in zip(expected, statuses):
                        needs[job]["result"] = status
                    should_pass = all(
                        status == "success" or (status == "skipped" and not expected[job])
                        for job, status in zip(expected, statuses)
                    )
                    with self.subTest(event=event, flags=(code, docs, workflows), statuses=statuses):
                        self.assertEqual(check_required_jobs(needs, event).passed, should_pass)

    def test_documentation_failure_blocks_docs_only_change(self):
        needs = needs_for(docs="true")
        needs["docs"]["result"] = "failure"
        self.assertFalse(check_required_jobs(needs, "pull_request").passed)

    def test_manual_run_requires_all_gates(self):
        needs = needs_for()
        self.assertFalse(check_required_jobs(needs, "workflow_dispatch").passed)
        for job in ("build-test", "code-quality", "docs"):
            needs[job]["result"] = "success"
        self.assertTrue(check_required_jobs(needs, "workflow_dispatch").passed)

    def test_changes_must_succeed_even_on_manual_run(self):
        for status in ("failure", "cancelled", "skipped", "unknown", None):
            needs = needs_for()
            for job in ("build-test", "code-quality", "docs"):
                needs[job]["result"] = "success"
            needs["changes"]["result"] = status
            with self.subTest(status=status):
                self.assertFalse(check_required_jobs(needs, "workflow_dispatch").passed)

    def test_missing_or_malformed_job_entries_fail(self):
        for job in ("changes", "build-test", "code-quality", "docs"):
            for replacement in (None, "success", {}, {"result": None}, {"result": "unknown"}):
                needs = needs_for()
                needs[job] = replacement
                with self.subTest(job=job, replacement=replacement):
                    self.assertFalse(check_required_jobs(needs, "push").passed)
            needs = needs_for()
            del needs[job]
            with self.subTest(missing=job):
                self.assertFalse(check_required_jobs(needs, "push").passed)

    def test_invalid_change_outputs_fail(self):
        original = needs_for()
        for name in ("code", "docs", "workflows"):
            for value in (None, True, False, 0, 1, "", "TRUE", [], {}):
                needs = copy.deepcopy(original)
                needs["changes"]["outputs"][name] = value
                with self.subTest(name=name, value=value):
                    self.assertFalse(check_required_jobs(needs, "push").passed)
            needs = copy.deepcopy(original)
            del needs["changes"]["outputs"][name]
            self.assertFalse(check_required_jobs(needs, "push").passed)
        for outputs in (None, [], "false", {}):
            needs = copy.deepcopy(original)
            needs["changes"]["outputs"] = outputs
            self.assertFalse(check_required_jobs(needs, "push").passed)
        needs = copy.deepcopy(original)
        del needs["changes"]["outputs"]
        self.assertFalse(check_required_jobs(needs, "push").passed)

    def test_non_object_needs_fail(self):
        for needs in (None, [], "success", True, 1):
            with self.subTest(needs=needs):
                self.assertFalse(check_required_jobs(needs, "push").passed)

    def test_unknown_event_fails(self):
        for event in ("", "unknown", "pull_request_target"):
            self.assertFalse(check_required_jobs(needs_for(), event).passed)

    @mock.patch.dict("os.environ", {}, clear=True)
    def test_cli_fails_on_invalid_json(self):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(main(["--needs-json", "{", "--event-name", "push"]), 1)

    def test_cli_success_and_failure_exit_codes_and_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            summary_file = Path(directory) / "summary.md"
            args = [
                "--needs-json", json.dumps(needs_for()), "--event-name", "push",
                "--summary-file", str(summary_file),
            ]
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(args), 0)
                args[3] = "workflow_dispatch"
                self.assertEqual(main(args), 1)
            report = summary_file.read_text(encoding="utf-8")
            self.assertIn("conditional skips are valid", report)
            self.assertIn("The aggregate check failed", report)

    def test_workflow_trigger_expressions_match_policy(self):
        # Make divergence between workflow conditions and Python policy visible.
        workflow = Path(__file__).resolve().parents[2] / ".github/workflows/ci.yml"
        lines = workflow.read_text(encoding="utf-8").splitlines()
        expected = {
            "build-test": "needs.changes.outputs.code == 'true' || needs.changes.outputs.workflows == 'true' || github.event_name == 'schedule' || github.event_name == 'workflow_dispatch'",
            "code-quality": None,
            "docs": "needs.changes.outputs.docs == 'true' || needs.changes.outputs.code == 'true' || github.event_name == 'schedule' || github.event_name == 'workflow_dispatch'",
        }
        for job, expression in expected.items():
            start = lines.index("  {}:".format(job)) + 1
            condition = None
            for line in lines[start:]:
                if line.startswith("  ") and not line.startswith("    ") and line.strip():
                    break
                if line.startswith("    if: "):
                    condition = line.removeprefix("    if: ")
                    break
            with self.subTest(job=job):
                self.assertEqual(condition, expression)


if __name__ == "__main__":
    unittest.main()
