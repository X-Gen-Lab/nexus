"""Precise reviewed unknowns cannot exempt other secrets or scanner failures."""
import ast
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.ci import secret_scan

ROOT = Path(__file__).resolve().parents[3]
SOURCE_PATH = "tools/testing/dependencies/test_secret_scan_fixture.py"


class SecretScanTests(unittest.TestCase):
    def test_empty_exception_policy_keeps_all_candidates_blocking(self):
        policy = self.policy | {"reviewed_unknowns": []}
        self.assertEqual(secret_scan.evaluate(
            b"", b"", 0, self.root, policy)["status"], "passed")
        candidate = json.dumps(self.candidate()).encode()
        self.assertEqual(secret_scan.evaluate(
            candidate, b"", 183, self.root, policy)["blocking_count"], 1)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.policy = json.loads((ROOT / ".github/security/trufflehog.json").read_text())
        # A small local source fixture exercises exact context admission without
        # retaining retired tools or credential-like strings in this repository.
        self.source = (
            "class Fixture:\n"
            "    def test_first_reviewed_context(self):\n"
            "        with self.assertRaises(ValueError):\n"
            "            reject_first()\n"
            "\n"
            "    def test_second_reviewed_context(self):\n"
            "        with self.assertRaises(ValueError):\n"
            "            reject_second()\n"
        ).encode()
        lines = self.source.decode().splitlines(keepends=True)
        self.policy["reviewed_unknowns"] = []
        for node in ast.walk(ast.parse(self.source)):
            if not isinstance(node, ast.FunctionDef):
                continue
            context = "".join(lines[node.lineno - 1:node.end_lineno]).encode()
            raw = "synthetic-scanner-fixture-" + node.name
            self.policy["reviewed_unknowns"].append({
                "detector": "URI", "detector_type": 17, "source_type": 16,
                "path": SOURCE_PATH, "function": node.name,
                "raw_sha256": hashlib.sha256(raw.encode()).hexdigest(),
                "context_sha256": hashlib.sha256(context).hexdigest(),
                "reason": "Local synthetic context-admission fixture",
            })
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
        raw = "synthetic-scanner-fixture-" + node.name
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

    def test_repository_policy_keeps_exact_reviewed_history_paths(self):
        policy = json.loads((ROOT / ".github/security/trufflehog.json").read_text())
        self.assertIs(secret_scan.validate_policy(policy), policy)
        items = policy["reviewed_unknowns"]
        self.assertEqual({item["path"] for item in items}, {
            "tools/testing/test_dependency_preparation.py",
            "tools/testing/dependencies/test_dependency_preparation.py"})
        identity = dict(items[0])
        del identity["path"]
        for item in items:
            self.assertEqual(item, dict(identity, path=item["path"]))
            item["verified"] = True
            with self.assertRaises(ValueError):
                secret_scan.validate_policy(policy)
            del item["verified"]

    def test_each_reviewed_history_path_requires_raw_and_context_identity(self):
        policy = json.loads((ROOT / ".github/security/trufflehog.json").read_text())
        paths = [item["path"] for item in policy["reviewed_unknowns"]]
        for path in paths:
            destination = self.root / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(self.source)
        self.git("add", *paths)
        self.git("commit", "-qm", "test: retain exact historical fixture paths")
        self.commit = self.git("rev-parse", "HEAD").strip()
        for path in paths:
            with self.subTest(path=path):
                for item in self.policy["reviewed_unknowns"]:
                    item["path"] = path
                candidate = self.candidate()
                candidate["SourceMetadata"]["Data"]["Git"]["file"] = path
                self.assertEqual(self.evaluate([candidate])["status"], "passed")
                for key in ("raw_sha256", "context_sha256"):
                    item = self.policy["reviewed_unknowns"][1]
                    original = item[key]
                    item[key] = "0" * 64
                    self.assertEqual(self.evaluate([candidate])["blocking_count"], 1)
                    item[key] = original
                candidate["Verified"] = True
                self.assertEqual(self.evaluate([candidate])["blocking_count"], 1)

    def test_policy_rejects_paths_outside_owned_python_tests(self):
        paths = ("/tools/testing/test_fixture.py", "tools/testing/../test_fixture.py",
                 "tools/testing/parts/../../test_fixture.py",
                 "tools/testing/parts/*.py", "tools/testing/parts/test_?.py",
                 "tools/testing_other/test_fixture.py", "tools/test_fixture.py",
                 "tools/testing/parts/fixture.py", "tools/testing/test_fixture.pyc",
                 "tools/testing//test_fixture.py", "tools/testing/./test_fixture.py")
        for path in paths:
            policy = copy.deepcopy(self.policy)
            policy["reviewed_unknowns"][0]["path"] = path
            with self.subTest(path=path), self.assertRaises(ValueError):
                secret_scan.validate_policy(policy)

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
