"""Indexed observations remain navigation, never new execution qualification."""

import copy
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure
from tools.evidence import identity
from tools.evidence.common import EvidenceError, file_identity
from tools.maintenance import capabilities


ROOT = Path(__file__).resolve().parents[2]
ASSEMBLY = ROOT / "tools/configure/assemblies/qiming-baremetal-uart-dma.toml"


class SelectedIndexTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.index = Path(self.workspace.name) / "results.json"
        self.source = {"root": str(ROOT), "commit": "1" * 40,
                       "tree": "2" * 40, "dirty": False, "submodules": []}
        self.resolved = configure.resolve_ir(ASSEMBLY, ROOT).to_dict()
        self.record = {"source": copy.deepcopy(self.source), "status": "passed",
                       "configuration_sha256": self.resolved["configuration_sha256"],
                       "resolved": copy.deepcopy(self.resolved),
                       "checks": {"arm_links": 1}, "physical_hil": "not_executed"}

    def query(self, record=None, pointer="/iterations/selected", observations=None):
        self.index.write_text(json.dumps({"iterations": {
            "selected": self.record if record is None else record}}))
        with patch.object(identity, "observe_git_source",
                          side_effect=observations or [self.source, self.source]):
            return capabilities.selection(ASSEMBLY, ROOT,
                results_index=self.index, record_pointer=pointer)

    def test_exact_indexed_observation_preserves_bytes_and_never_promotes_validation(self):
        result = self.query()
        observed = result["indexed_evidence"]
        self.assertEqual(observed["index"], file_identity(self.index))
        self.assertEqual(observed["record_pointer"], "/iterations/selected")
        self.assertEqual(observed["record"], self.record)
        self.assertEqual(observed["relation"], "exact_source_and_configuration")
        self.assertTrue(observed["source_match"])
        self.assertTrue(observed["configuration_match"])
        self.assertEqual(observed["verification"], "indexed_observation_only")
        for scope in ("host_models", "arm_image", "installed_sdk",
                      "software_candidate", "physical_hil"):
            self.assertEqual(result["validation"][scope], "not_assessed")
        self.assertFalse(result["physical_qualified"])
        self.assertNotIn("evidence", result)

    def test_incomplete_source_and_missing_configuration_are_not_exact(self):
        record = {"source": {"commit": self.source["commit"],
                             "tree": self.source["tree"]},
                  "status": "30_selected_software_tasks_implemented"}
        observed = self.query(record)["indexed_evidence"]
        self.assertEqual(observed["relation"], "insufficient_identity")
        self.assertTrue(observed["code_identity_match"])
        self.assertFalse(observed["source_match"])
        self.assertFalse(observed["configuration_match"])
        self.assertEqual(observed["configuration_relation"], "not_recorded")

    def test_current_source_without_configuration_never_inherits_a_scope(self):
        record = {"source": self.source, "status": "passed"}
        observed = self.query(record)["indexed_evidence"]
        self.assertTrue(observed["source_match"])
        self.assertEqual(observed["relation"], "insufficient_identity")

    def test_historical_code_is_navigation_only(self):
        record = copy.deepcopy(self.record)
        record["source"]["commit"] = "3" * 40
        observed = self.query(record)["indexed_evidence"]
        self.assertEqual(observed["relation"], "historical_source")
        self.assertFalse(observed["code_identity_match"])

    def test_dirty_or_unavailable_dependency_identity_never_matches(self):
        for update in ({"dirty": True}, {"submodules": ["-" + "4" * 40 + " ext/model"]}):
            with self.subTest(update=update):
                record = copy.deepcopy(self.record)
                record["source"].update(update)
                observed = self.query(record)["indexed_evidence"]
                self.assertFalse(observed["source_match"])

    def test_relocation_does_not_change_exact_git_identity(self):
        record = copy.deepcopy(self.record)
        record["source"]["root"] = "/original/source"
        self.assertTrue(self.query(record)["indexed_evidence"]["source_match"])

    def test_configuration_digest_and_resolved_projection_both_must_match(self):
        for update in ({"configuration_sha256": "a" * 64},
                       {"resolved": {**self.resolved, "backend": "freertos"}}):
            with self.subTest(update=tuple(update)):
                record = {**self.record, **update}
                observed = self.query(record)["indexed_evidence"]
                self.assertEqual(observed["relation"], "configuration_mismatch")
                self.assertFalse(observed["configuration_match"])

    def test_digest_only_record_is_explicitly_distinct_from_resolved_comparison(self):
        record = {key: value for key, value in self.record.items() if key != "resolved"}
        observed = self.query(record)["indexed_evidence"]
        self.assertEqual(observed["configuration_relation"], "exact_digest")
        self.assertEqual(observed["relation"], "exact_source_and_configuration")

    def test_pointer_escapes_are_decoded_without_path_or_ancestor_scope_inference(self):
        self.index.write_text(json.dumps({"group/a": {"~mode": self.record}}))
        with patch.object(identity, "observe_git_source", return_value=self.source):
            result = capabilities.selection(ASSEMBLY, ROOT, results_index=self.index,
                                           record_pointer="/group~1a/~0mode")
        self.assertEqual(result["indexed_evidence"]["record"], self.record)
        for pointer in ("iterations/selected", "/iterations/missing", "/iterations/~2bad",
                        "/iterations/selected/status"):
            with self.subTest(pointer=pointer), self.assertRaises(EvidenceError):
                self.query(pointer=pointer)

    def test_invalid_record_identity_is_rejected(self):
        for field, value in (("source", "passed"),
                             ("configuration_sha256", "not-a-digest"),
                             ("resolved", [])):
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                self.query({**self.record, field: value})

    def test_query_rejects_source_change(self):
        with self.assertRaisesRegex(EvidenceError, "source changed"):
            self.query(observations=[self.source, {**self.source, "tree": "5" * 40}])

    def test_nested_record_never_inherits_its_parent_source_or_configuration(self):
        observed = self.query(pointer="/iterations/selected/checks")["indexed_evidence"]
        self.assertEqual(observed["record"], {"arm_links": 1})
        self.assertEqual(observed["relation"], "insufficient_identity")
        self.assertFalse(observed["source_match"])
        self.assertFalse(observed["configuration_match"])

    def test_changing_index_bytes_during_the_query_is_rejected(self):
        from tools.evidence import common
        load = common.load_json

        def change_index(path):
            value = load(path)
            self.index.write_text(self.index.read_text() + " ")
            return value

        with patch.object(common, "load_json", side_effect=change_index), \
                self.assertRaisesRegex(EvidenceError, "index changed"):
            self.query()

    def test_oversized_index_is_rejected_before_unbounded_identity_hashing(self):
        self.index.write_bytes(b" " * (4 * 1024 * 1024 + 1))
        with patch.object(identity, "observe_git_source", return_value=self.source), \
                patch("tools.evidence.common.file_identity",
                      side_effect=AssertionError("oversized input was hashed")), \
                self.assertRaisesRegex(EvidenceError, "bounded input size"):
            capabilities.selection(ASSEMBLY, ROOT, results_index=self.index,
                                   record_pointer="/iterations")

    def test_index_is_read_with_duplicate_key_and_size_guards(self):
        self.index.write_text('{"iterations": {}, "iterations": {}}')
        with patch.object(identity, "observe_git_source", return_value=self.source), \
                self.assertRaisesRegex(EvidenceError, "duplicate JSON"):
            capabilities.selection(ASSEMBLY, ROOT, results_index=self.index,
                                   record_pointer="/iterations")

    def test_cli_requires_assembly_and_pointer_with_index(self):
        for args in (["--results-index", str(self.index), "--record", "/selected"],
                     ["--assembly", str(ASSEMBLY), "--results-index", str(self.index)],
                     ["--assembly", str(ASSEMBLY), "--record", "/selected"]):
            with self.subTest(args=args), redirect_stderr(io.StringIO()), \
                    self.assertRaises(SystemExit) as raised:
                capabilities.main(args)
            self.assertEqual(raised.exception.code, 2)
        self.query()
        stream = io.StringIO()
        with patch.object(identity, "observe_git_source", return_value=self.source), \
                redirect_stdout(stream):
            code = capabilities.main(["--assembly", str(ASSEMBLY),
                "--results-index", str(self.index), "--record", "/iterations/selected"])
        self.assertEqual(code, 0)
        self.assertIn("indexed_evidence", json.loads(stream.getvalue()))


if __name__ == "__main__":
    unittest.main()
