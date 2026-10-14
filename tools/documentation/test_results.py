"""Offline result admission preserves original authorities and scopes."""

import csv
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from tools.documentation import results


SOURCE_ROOT = Path(__file__).resolve().parents[2]


class ResultsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in (results.INDEX_PATH, results.TASK_AUTHORITY,
                     results.QUALIFIED_INDEX):
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(SOURCE_ROOT / name, destination)
        self.path = self.root / results.INDEX_PATH
        self.document = json.loads(self.path.read_text())

    def write(self):
        self.path.write_text(json.dumps(self.document, ensure_ascii=False))

    def errors(self):
        self.write()
        return "\n".join(results.validate(self.root))

    def test_current_historical_index_passes_without_git_or_archives(self):
        self.assertFalse((self.root / ".git").exists())
        self.assertEqual(results.validate(self.root), [])

    def test_missing_or_broken_index_is_rejected(self):
        self.path.unlink()
        self.assertIn("missing", "\n".join(results.validate(self.root)))
        self.path.write_text("{ broken json")
        self.assertIn("JSON", "\n".join(results.validate(self.root)))

    def test_duplicate_json_fields_are_rejected(self):
        self.path.write_text(self.path.read_text().replace(
            '"schema_version": 1,',
            '"schema_version": 1, "schema_version": 1,', 1))
        self.assertIn("duplicate", "\n".join(results.validate(self.root)))

    def test_unknown_top_level_and_nested_fields_are_rejected(self):
        self.document["unreviewed_scope"] = "accepted"
        self.assertIn("unknown", self.errors())
        del self.document["unreviewed_scope"]
        self.document["tasks"][0]["hardware_passed"] = True
        self.assertIn("unknown", self.errors())

    def test_boolean_schema_and_negative_byte_count_are_rejected(self):
        self.document["schema_version"] = True
        self.assertIn("schema_version", self.errors())
        self.document["schema_version"] = 1
        self.document["evidence"]["browser"]["bytes"] = -1
        self.assertIn("bytes", self.errors())

    def test_wrong_collection_types_do_not_escape_as_exceptions(self):
        for key in ("tasks", "evidence", "actual_scope_receipts"):
            with self.subTest(key=key):
                original = self.document[key]
                self.document[key] = "not a collection"
                self.assertIn(key, self.errors())
                self.document[key] = original

    def test_task_count_missing_duplicate_and_order_drift_are_rejected(self):
        self.document["task_count"] = 57
        self.assertIn("task_count", self.errors())
        self.document["task_count"] = 58
        last = self.document["tasks"].pop()
        self.assertIn("task", self.errors())
        self.document["tasks"].append(last)
        first = self.document["tasks"][0]
        self.document["tasks"][0] = last
        self.assertIn("task", self.errors())
        self.document["tasks"][0] = first
        self.document["tasks"].reverse()
        self.assertIn("order", self.errors())

    def test_unknown_empty_and_duplicate_evidence_keys_are_rejected(self):
        first = self.document["tasks"][0]
        original = first["evidence_keys"]
        for values in (["missing_evidence"], [], [original[0], original[0]]):
            with self.subTest(values=values):
                first["evidence_keys"] = values
                self.assertIn("evidence", self.errors())
        first["evidence_keys"] = original

    def test_unused_evidence_record_is_rejected(self):
        self.document["evidence"]["unmapped"] = dict(
            self.document["evidence"]["browser"])
        self.assertIn("unreferenced", self.errors())

    def test_result_status_and_scope_must_match_original_csv(self):
        first = self.document["tasks"][0]
        first["status"] = "development_passed"
        self.assertIn("status", self.errors())
        first["status"] = "passed"
        first["validation_scope"] = "new_firmware_qualification"
        self.assertIn("scope", self.errors())

    def test_csv_exit_condition_drift_breaks_authority_digest(self):
        path = self.root / results.TASK_AUTHORITY
        text = path.read_text()
        path.write_text(text.replace("逐页明确正文", "未经复核改写退出条件", 1))
        self.assertIn("task_authority_sha256", self.errors())

    def test_rehashed_csv_status_drift_is_still_rejected(self):
        path = self.root / results.TASK_AUTHORITY
        with path.open(newline="") as stream:
            reader = csv.DictReader(stream)
            fields, rows = reader.fieldnames, list(reader)
        rows[0]["validation_status"] = "pending_formal_evidence"
        with path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)
        self.document["task_authority_sha256"] = hashlib.sha256(
            path.read_bytes()).hexdigest()
        self.assertIn("status", self.errors())

    def test_authority_and_member_paths_must_be_normalized(self):
        self.document["task_authority"] = "docs/design/../other.csv"
        self.assertIn("task_authority", self.errors())
        self.document["task_authority"] = results.TASK_AUTHORITY
        member = self.document["evidence"]["browser"]
        original = member["member_path"]
        for value in ("../outside.json", "/absolute.json", "payload//x.json",
                      "payload\\x.json", "payload/./x.json", "payload/x#part"):
            with self.subTest(value=value):
                member["member_path"] = value
                self.assertIn("member_path", self.errors())
        member["member_path"] = original

    def test_invalid_digests_and_git_identities_are_rejected(self):
        self.document["evidence"]["browser"]["sha256"] = "abc"
        self.assertIn("sha256", self.errors())
        self.document["source"]["documentation_commit"] = "main"
        self.assertIn("documentation_commit", self.errors())

    def test_conflicting_receipts_for_same_member_are_rejected(self):
        self.document["actual_scope_receipts"]["clean_source_build"][
            "sha256"] = "0" * 64
        self.assertIn("conflicting", self.errors())

    def test_missing_qualified_code_index_is_rejected(self):
        (self.root / results.QUALIFIED_INDEX).unlink()
        self.assertIn("qualified", self.errors())

    def test_qualified_code_digest_and_source_identity_must_match(self):
        reference = self.document["qualified_code_reference"]
        reference["index_sha256"] = "0" * 64
        self.assertIn("index_sha256", self.errors())
        reference["index_sha256"] = hashlib.sha256(
            (self.root / results.QUALIFIED_INDEX).read_bytes()).hexdigest()
        reference["commit"] = "0" * 40
        reference["tree"] = "1" * 40
        self.assertIn("qualified", self.errors())

    def test_rehashed_qualified_index_with_malformed_source_is_rejected(self):
        path = self.root / results.QUALIFIED_INDEX
        document = json.loads(path.read_text())
        document["source"] = []
        path.write_text(json.dumps(document))
        self.document["qualified_code_reference"]["index_sha256"] = (
            hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertIn("qualified", self.errors())

    def test_symlink_authorities_are_rejected(self):
        path = self.root / results.TASK_AUTHORITY
        outside = self.root / "authority.csv"
        path.rename(outside)
        path.symlink_to(outside)
        self.assertIn("symlink", self.errors())

    def test_documentation_cannot_grant_physical_hil(self):
        self.document["physical_hil"]["status"] = "passed"
        self.document["physical_hil"]["changed_by_documentation"] = True
        self.assertIn("physical_hil", self.errors())

    def test_formal_evidence_required_must_match_nonaccepted_tasks(self):
        self.document["formal_evidence_required_for"] = ["DOC-01"]
        self.assertIn("formal_evidence_required_for", self.errors())

    def test_package_release_identity_is_bound_to_documentation_source(self):
        self.document["documentation_package"]["release_id"] = (
            "docs-20261014-deadbeefdead")
        self.assertIn("release_id", self.errors())

    def test_parts_total_and_member_paths_are_bound(self):
        parts = self.document["evidence_delivery"]["parts"]
        parts[0]["bytes"] += 1
        self.assertIn("parts", self.errors())
        parts[0]["bytes"] -= 1
        parts[1]["path"] = parts[0]["path"]
        self.assertIn("duplicate", self.errors())

    def test_unsafe_or_floating_delivery_urls_are_rejected(self):
        original = self.document["documentation_package"]["download_url"]
        self.document["documentation_package"]["download_url"] = (
            "http://example.invalid/archive.tar.gz")
        self.assertIn("download_url", self.errors())
        self.document["documentation_package"]["download_url"] = original
        self.document["evidence_delivery"]["inventory_url"] = (
            "https://raw.githubusercontent.com/X-Gen-Lab/nexus/main/index.json")
        self.assertIn("inventory_url", self.errors())


if __name__ == "__main__":
    unittest.main()
