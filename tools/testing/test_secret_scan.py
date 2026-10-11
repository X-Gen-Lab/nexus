"""Precise reviewed unknowns cannot exempt other secrets or scanner failures."""
import ast
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from urllib.parse import urlsplit, urlunsplit

from scripts.ci import secret_scan

ROOT = Path(__file__).resolve().parents[2]
SOURCE_PATH = "tools/testing/test_dependency_proposals.py"


class SecretScanTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.policy = json.loads((ROOT / ".github/security/trufflehog.json").read_text())
        # Tests work in shallow CI checkouts while still committing the actual
        # reviewed source into a real isolated Git repository.
        self.source = (ROOT / SOURCE_PATH).read_bytes()
        destination = self.root / SOURCE_PATH
        destination.parent.mkdir(parents=True)
        destination.write_bytes(self.source)
        self.git("init", "-q")
        self.git("config", "user.name", "Secret scan test")
        self.git("config", "user.email", "scan@local.invalid")
        self.git("add", SOURCE_PATH)
        self.git("commit", "-qm", "test: commit actual negative fixtures")
        self.commit = self.git("rev-parse", "HEAD").strip()

    def git(self, *arguments):
        return subprocess.check_output(["git", *arguments], cwd=self.root,
                                       text=True, stderr=subprocess.PIPE)

    def candidate(self, index=1):
        reviewed = self.policy["reviewed_unknowns"][index]
        node = next(n for n in ast.walk(ast.parse(self.source))
                    if isinstance(n, ast.FunctionDef)
                    and n.name == reviewed["function"])
        literal = next(n.value for n in ast.walk(node)
                       if isinstance(n, ast.Constant) and isinstance(n.value, str)
                       and n.value.startswith("https:") and "@" in n.value)
        url = urlsplit(literal)
        raw = urlunsplit((url.scheme, url.netloc, "", "", ""))
        self.assertEqual(hashlib.sha256(raw.encode()).hexdigest(),
                         reviewed["raw_sha256"])
        return {"DetectorName": "URI", "DetectorType": 17, "SourceType": 16,
                "SourceName": "trufflehog - git", "DecoderName": "PLAIN",
                "Verified": False, "VerificationError": "test network failure",
                "Raw": raw, "SourceMetadata": {"Data": {"Git": {
                    "commit": self.commit, "file": SOURCE_PATH,
                    "line": node.lineno + 1, "repository": self.root.as_uri()}}}}

    def evaluate(self, candidates, status=183):
        payload = "\n".join(json.dumps(item) for item in candidates).encode()
        return secret_scan.evaluate(payload, b"diagnostic", status, self.root,
                                    self.policy)

    def test_two_reviewed_contexts_are_accepted_without_printing_raw(self):
        report = self.evaluate([self.candidate(0), self.candidate(1)])
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["reviewed_unknown_count"], 2)
        self.assertEqual(report["blocking_count"], 0)
        text = json.dumps(report)
        for index in (0, 1):
            self.assertNotIn(self.candidate(index)["Raw"], text)
        self.assertNotIn("VerificationError", text)

    def test_verified_candidate_is_always_blocking(self):
        candidate = self.candidate()
        candidate["Verified"] = True
        candidate["VerificationError"] = None
        self.assertEqual(self.evaluate([candidate])["blocking_count"], 1)

    def test_unknown_same_file_other_hash_is_blocking(self):
        candidate = self.candidate()
        candidate["Raw"] += "changed"
        self.assertEqual(self.evaluate([candidate])["blocking_count"], 1)

    def test_other_path_detector_decoder_or_repository_is_blocking(self):
        for field, value in (("DetectorName", "Other"), ("DetectorType", 18),
                             ("SourceType", 15), ("DecoderName", "BASE64")):
            candidate = self.candidate()
            candidate[field] = value
            with self.subTest(field=field):
                self.assertEqual(self.evaluate([candidate])["blocking_count"], 1)
        for field, value in (("file", "other.py"),
                             ("repository", "file:///other"), ("line", 1)):
            candidate = self.candidate()
            candidate["SourceMetadata"]["Data"]["Git"][field] = value
            with self.subTest(field=field):
                self.assertEqual(self.evaluate([candidate])["blocking_count"], 1)

    def test_actual_commit_context_drift_is_blocking(self):
        changed = self.source.replace(b"with self.assertRaises(ValueError)",
                                     b"with self.assertRaises(TypeError)")
        (self.root / SOURCE_PATH).write_bytes(changed)
        self.git("add", SOURCE_PATH)
        self.git("commit", "-qm", "test: change actual rejection context")
        self.commit = self.git("rev-parse", "HEAD").strip()
        self.assertEqual(self.evaluate([self.candidate()])["blocking_count"], 1)

    def test_unknown_requires_actual_error_and_boolean_verified(self):
        for verified, error in ((False, None), (0, "network"),
                                (None, "network"), (False, "")):
            candidate = self.candidate()
            candidate.update(Verified=verified, VerificationError=error)
            with self.subTest(verified=verified, error=error):
                self.assertEqual(self.evaluate([candidate])["blocking_count"], 1)

    def test_malformed_numeric_enum_types_cannot_match_reviewed_policy(self):
        for field, value in (("DetectorType", 17.0), ("SourceType", 16.0)):
            candidate = self.candidate()
            candidate[field] = value
            with self.subTest(field=field):
                self.assertEqual(self.evaluate([candidate])["blocking_count"], 1)

    def test_scan_error_exit_cannot_be_accepted_as_reviewed(self):
        for status in (1, 2, 66, 124, -9):
            with self.subTest(status=status):
                with self.assertRaises(ValueError):
                    self.evaluate([self.candidate()], status)

    def test_fail_flag_exit_must_agree_with_actual_candidates(self):
        with self.assertRaises(ValueError):
            self.evaluate([], 183)
        with self.assertRaises(ValueError):
            self.evaluate([self.candidate()], 0)
        self.assertEqual(self.evaluate([], 0)["status"], "passed")

    def test_malformed_output_and_duplicate_json_keys_reject(self):
        for payload in (b"invalid", b"{}", b'{"Raw":"a","Raw":"b"}'):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                secret_scan.evaluate(payload, b"", 183, self.root, self.policy)

    def test_nonfinite_json_extension_is_rejected_before_admission(self):
        for value in (float("nan"), float("inf"), float("-inf")):
            candidate = self.candidate()
            candidate["ExtraData"] = value
            payload = json.dumps(candidate).encode()
            with self.subTest(value=value), self.assertRaises(ValueError):
                secret_scan.evaluate(payload, b"", 183, self.root, self.policy)

    def test_missing_commit_is_blocking_without_git_failure_leak(self):
        candidate = self.candidate()
        candidate["SourceMetadata"]["Data"]["Git"]["commit"] = "0" * 40
        report = self.evaluate([candidate])
        self.assertEqual(report["blocking_count"], 1)
        self.assertNotIn("fatal", json.dumps(report))

    def test_closed_policy_rejects_unknown_fields_duplicates_or_wildcards(self):
        for change in ("field", "duplicate", "wildcard", "verified"):
            policy = copy.deepcopy(self.policy)
            if change == "field":
                policy["ignore_unknown"] = True
            elif change == "duplicate":
                policy["reviewed_unknowns"].append(policy["reviewed_unknowns"][0])
            elif change == "wildcard":
                policy["reviewed_unknowns"][0]["path"] = "**/*.py"
            else:
                policy["reviewed_unknowns"][0]["verified"] = True
            with self.subTest(change=change), self.assertRaises(ValueError):
                secret_scan.validate_policy(policy)


if __name__ == "__main__":
    unittest.main()
