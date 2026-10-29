"""Delivery selects only the current producer's exact, completed raw closure."""

import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from scripts.ci.tdd_gate import validate_google_report
from scripts.validation.junit import validate_junit
from tools.dev import process, qualification
from tools.evidence.common import EvidenceError, load_json


class NativeExecutionClosureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.tdd = self.build / "tdd"
        self.tdd.mkdir(parents=True)
        self.binary = self.build / "bin" / "contract"
        self.binary.parent.mkdir()
        self.binary.write_text(f"#!{sys.executable}\nprint('Suite.\\n  Works')\n")
        self.binary.chmod(0o755)
        self.manifest = self.build / "google-contracts.json"
        self.manifest.write_text(json.dumps({"schema": 1,
                                            "executables": [str(self.binary)]}))
        google = self.tdd / "google"
        google.mkdir()
        self.google = google / "contract.xml"
        ctest = self.build / "ctest-results.xml"
        self.ctest = self.root / "ctest"
        plan = {"tests": [{"name": "contract", "command": [str(self.binary)]}]}
        self.ctest.write_text(
            f"#!{sys.executable}\nimport json,sys\nfrom pathlib import Path\n"
            "if '--show-only=json-v1' in sys.argv:\n"
            f"    print(json.dumps({plan!r}))\nelse:\n"
            f"    Path({str(ctest)!r}).write_text('<testsuite tests=\"1\">"
            '<testcase name="contract"/></testsuite>\')\n'
            f"    Path({str(self.google)!r}).write_text('<testsuites name=\"AllTests\" tests=\"1\">"
            '<testsuite tests="1"><testcase classname="Suite" name="Works" '
            'status="run" result="completed"/></testsuite></testsuites>\')\n'
            "    print('actual fixture execution')\n")
        self.ctest.chmod(0o755)
        self.execute([str(self.ctest), "--test-dir", str(self.build),
                      "--show-only=json-v1"], "ctest-plan")
        self.execute([str(self.binary), "--gtest_filter=*", "--gtest_list_tests"],
                     "discovery-0")
        self.execute([str(self.ctest), "--preset", "native-debug",
                      "--output-on-failure", "--no-tests=error", "--parallel",
                      "1", "--output-junit", str(ctest)], "ctest-execution")
        self.summary = {"schema": 1, "kind": "development-contract-execution",
                        "preset": "native-debug", "passed": 1,
                        "suites": [{"binary": "contract",
                                    **validate_google_report(self.google)}],
                        "ctest": validate_junit(ctest), "scope": "fixture"}
        self.summary_path = self.tdd / "execution.json"
        self.save_summary()

    def execute(self, argv, stem):
        result = process.run(argv, cwd=self.root, env=os.environ.copy(),
                             log=self.tdd / (stem + ".txt"), timeout=30,
                             stage=stem)
        self.assertEqual(result.returncode, 0)

    def save_summary(self):
        self.summary_path.write_text(json.dumps(self.summary))

    def inputs(self):
        return qualification.native_execution_inputs(self.build, "native-debug")

    def test_selection_excludes_historical_attempts_and_keeps_empty_streams(self):
        old = self.tdd / "historical-attempt.raw"
        old.write_bytes(b"old evidence remains untouched")
        paths = self.inputs()
        self.assertIn(self.google, paths)
        self.assertIn(self.summary_path, paths)
        self.assertNotIn(old, paths)
        self.assertEqual(len(paths), len(set(paths)))
        empty = [path for path in paths if path.name == "stderr.txt"]
        self.assertEqual(len(empty), 3)
        self.assertTrue(all(path.stat().st_size == 0 for path in empty))

    def test_changed_xml_and_rehashed_incorrect_counts_do_not_qualify(self):
        self.google.write_text('<testsuites name="AllTests" tests="1">'
                               '<testsuite><testcase classname="Suite" name="Other" '
                               'status="run" result="completed"/></testsuite></testsuites>')
        self.summary["suites"] = [{"binary": "contract",
                                   **validate_google_report(self.google)}]
        self.save_summary()
        with self.assertRaisesRegex(EvidenceError, "discovery|identities"):
            self.inputs()

    def test_unknown_original_exit_is_not_completed_by_summary(self):
        view = self.tdd / "ctest-execution.command.json"
        receipt = load_json(view)
        attempt = Path(receipt["attempt_receipt"])
        receipt.update(outcome="running", exit_code=None, finished_ns=None,
                       duration_seconds=None, resource_usage=None)
        view.write_text(json.dumps(receipt))
        attempt.write_text(json.dumps({key: value for key, value in receipt.items()
                                       if key != "attempt_receipt"}))
        with self.assertRaisesRegex(EvidenceError, "completed"):
            self.inputs()

    def test_conventional_view_must_match_original_unique_attempt(self):
        view = self.tdd / "discovery-0.command.json"
        receipt = load_json(view)
        receipt["duration_seconds"] += 1
        view.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(EvidenceError, "original|attempt"):
            self.inputs()

    def test_extra_google_report_is_not_silently_ignored(self):
        (self.tdd / "google/old.xml").write_text("old report")
        with self.assertRaisesRegex(EvidenceError, "reports|plan"):
            self.inputs()

    def test_preset_and_passed_totals_are_bound_to_current_summary(self):
        for field, value in (("preset", "native-release"), ("passed", 2)):
            original = self.summary[field]
            self.summary[field] = value
            self.save_summary()
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                self.inputs()
            self.summary[field] = original

    def test_manifest_change_during_selection_is_rejected(self):
        def changed(*args, **kwargs):
            self.manifest.write_text('{"schema":1,"executables":[]}')
            return validate_google_report(*args, **kwargs)

        with patch("scripts.ci.tdd_gate.validate_google_report", side_effect=changed):
            with self.assertRaisesRegex(EvidenceError, "changed|identity"):
                self.inputs()


class FormalExecutionClosureTests(unittest.TestCase):
    def setUp(self):
        import subprocess

        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        script = self.root / "tools/dev/dev.py"
        script.parent.mkdir(parents=True)
        script.write_text("print('actual external-checker fixture')\n")
        (self.root / ".gitignore").write_text("/result/\n")
        for arguments in (["init", "-q"], ["add", "."],
                          ["-c", "user.name=Fixture", "-c",
                           "user.email=fixture@example.invalid", "commit", "-qm", "fixture"]):
            subprocess.run(["git", "-C", str(self.root), *arguments],
                           check=True, capture_output=True)
        self.plan = {"schema_version": 1, "formal": True, "stages": [
            {"name": "configure", "checker": "configure", "args": []}]}
        report = qualification.execute(self.plan, self.root, self.root / "result")
        self.assertEqual(report["status"], "executed_passed")
        self.report = self.root / "result/report.json"

    def test_complete_formal_raw_closure_excludes_unreferenced_outputs(self):
        old = self.root / "result/old-attempt.raw"
        old.write_text("earlier unreferenced output")
        files = qualification.execution_inputs(self.plan, self.report, self.root)
        self.assertEqual(len(files), 7)
        self.assertIn(self.report, files)
        self.assertIn(self.root / "tools/dev/dev.py", files)
        self.assertNotIn(old, files)
        self.assertEqual(len([path for path in files if path.name == "stderr.txt"]), 1)

    def test_modified_original_output_cannot_become_a_fresh_retention_proof(self):
        report = load_json(self.report)
        receipt = load_json(Path(report["stages"][0]["command_receipt"]["path"]))
        Path(receipt["stdout"]["path"]).write_text("new unexecuted output")
        with self.assertRaises(EvidenceError):
            qualification.execution_inputs(self.plan, self.report, self.root)


if __name__ == "__main__":
    unittest.main()
