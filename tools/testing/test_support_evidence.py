"""Exact support queries associate verified bytes without widening their scope."""

import copy
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure
from tools.evidence import candidate, identity
from tools.evidence.common import EvidenceError
from tools.maintenance import capabilities


ROOT = Path(__file__).resolve().parents[2]
ASSEMBLY = ROOT / "tools/configure/assemblies/qiming-baremetal-uart-dma.toml"


class SelectedEvidenceTests(unittest.TestCase):
    """Git and candidate I/O are ports; resolution and association remain real."""

    def setUp(self):
        self.resolved = configure.resolve_ir(ASSEMBLY, ROOT).to_dict()
        self.source = {"root": str(ROOT), "commit": "1" * 40,
                       "tree": "2" * 40, "dirty": False, "submodules": []}
        # This model projection isolates the association boundary. It is never
        # written as a candidate or used as software qualification evidence.
        self.reference = {
            "schema_version": 1,
            "kind": "verified_software_candidate_reference",
            "candidate": {"path": "/model/candidate.json",
                          "sha256": "3" * 64, "size": 123},
            "candidate_id": "association-model",
            "source": copy.deepcopy(self.source),
            "configuration_sha256": self.resolved["configuration_sha256"],
            "resolved": copy.deepcopy(self.resolved),
            "artifacts": {role: {"sha256": str(index) * 64, "size": 32}
                          for index, role in enumerate(("elf", "bin", "map"), 4)},
            "qualified_scopes": sorted(candidate.REQUIRED_SCOPES),
            "physical_status": "not_executed",
            "product_promotion": "not_performed",
            "signature_status": "unsigned"}

    def query(self, reference=None, observations=None):
        with patch.object(candidate, "verified_projection", create=True,
                          return_value=reference or self.reference), \
                patch.object(identity, "observe_git_source",
                             side_effect=observations or [self.source, self.source]):
            return capabilities.selection(ASSEMBLY, ROOT,
                                          candidate_path=Path("/model/candidate.json"))

    def test_same_source_and_configuration_reference_exact_software_scopes(self):
        result = self.query()
        self.assertEqual(result["current_source"], self.source)
        evidence = result["evidence"]
        self.assertEqual(evidence["relation"], "exact_source_and_configuration")
        self.assertTrue(evidence["source_match"])
        self.assertTrue(evidence["configuration_match"])
        self.assertEqual(evidence["reference"]["source"], self.source)
        self.assertNotIn("resolved", evidence["reference"])
        self.assertEqual(evidence["reference"]["artifacts"],
                         self.reference["artifacts"])
        for scope in ("host_models", "arm_image", "installed_sdk",
                      "software_candidate"):
            self.assertEqual(result["validation"][scope], "passed")
        self.assertEqual(result["validation"]["physical_hil"], "not_assessed")
        self.assertFalse(result["physical_qualified"])

    def test_relocated_checkout_does_not_change_git_source_identity(self):
        reference = copy.deepcopy(self.reference)
        reference["source"]["root"] = "/original/source"
        result = self.query(reference)
        self.assertEqual(result["evidence"]["relation"],
                         "exact_source_and_configuration")

    def test_historical_source_is_navigation_without_current_qualification(self):
        reference = copy.deepcopy(self.reference)
        reference["source"]["commit"] = "7" * 40
        result = self.query(reference)
        self.assertEqual(result["evidence"]["relation"], "historical_source")
        self.assertTrue(result["evidence"]["configuration_match"])
        self.assertFalse(result["evidence"]["source_match"])
        self.assertEqual(result["validation"]["software_candidate"], "not_assessed")

    def test_dirty_or_different_dependency_source_cannot_inherit_qualification(self):
        original = copy.deepcopy(self.source)
        for field, value in (("dirty", True),
                             ("tree", "8" * 40),
                             ("submodules", ["-" + "9" * 40 + " ext/model"])):
            with self.subTest(field=field):
                self.source = {**original, field: value}
                result = self.query()
                self.assertFalse(result["evidence"]["source_match"])
                self.assertEqual(result["validation"]["host_models"],
                                 "not_assessed")

    def test_configuration_hash_mismatch_is_explicit_and_never_qualifies(self):
        reference = copy.deepcopy(self.reference)
        reference["configuration_sha256"] = "a" * 64
        reference["resolved"]["configuration_sha256"] = "a" * 64
        result = self.query(reference)
        self.assertEqual(result["evidence"]["relation"], "configuration_mismatch")
        self.assertFalse(result["evidence"]["configuration_match"])
        self.assertEqual(result["validation"]["arm_image"], "not_assessed")

    def test_display_fields_cannot_substitute_for_exact_resolved_bytes(self):
        # Even an accidentally reused digest from the boundary must not make a
        # changed route, backend or controller mode look like an exact match.
        for path, value in ((["backend"], "freertos"),
                            (["board"], "another-board"),
                            (["controllers", 0, "mode"], "dma-duplex"),
                            (["controllers", 0, "binding"], "another-route")):
            with self.subTest(path=path):
                reference = copy.deepcopy(self.reference)
                node = reference["resolved"]
                for key in path[:-1]:
                    node = node[key]
                node[path[-1]] = value
                result = self.query(reference)
                self.assertFalse(result["evidence"]["configuration_match"])
                self.assertEqual(result["validation"]["software_candidate"],
                                 "not_assessed")

    def test_invalid_candidate_is_not_reported_as_historical_success(self):
        with patch.object(candidate, "verified_projection", create=True,
                          side_effect=EvidenceError("changed raw receipt")), \
                patch.object(identity, "observe_git_source", return_value=self.source):
            with self.assertRaisesRegex(EvidenceError, "changed raw receipt"):
                capabilities.selection(ASSEMBLY, ROOT,
                                       candidate_path=Path("/model/candidate.json"))

    def test_source_change_during_query_rejects_the_report(self):
        changed = {**self.source, "tree": "9" * 40}
        with self.assertRaisesRegex(EvidenceError, "changed during"):
            self.query(observations=[self.source, changed])

    def test_no_evidence_does_not_observe_git_or_change_default_status(self):
        with patch.object(identity, "observe_git_source",
                          side_effect=AssertionError("unrequested Git observation")):
            result = capabilities.selection(ASSEMBLY, ROOT)
        self.assertNotIn("evidence", result)
        self.assertNotIn("current_source", result)
        self.assertEqual(result["validation"]["software_candidate"], "not_assessed")

    def test_cli_accepts_candidate_only_with_an_exact_assembly(self):
        for arguments in (["--candidate", "/model/candidate.json"],
                          ["--write", "--candidate", "/model/candidate.json"]):
            with self.subTest(arguments=arguments), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as raised:
                    capabilities.main(arguments)
                self.assertEqual(raised.exception.code, 2)
        stream = io.StringIO()
        with patch.object(candidate, "verified_projection", create=True,
                          return_value=self.reference), \
                patch.object(identity, "observe_git_source", return_value=self.source), \
                redirect_stdout(stream):
            result = capabilities.main(["--assembly", str(ASSEMBLY),
                                        "--candidate", "/model/candidate.json"])
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(stream.getvalue())["evidence"]["relation"],
                         "exact_source_and_configuration")

    def test_missing_candidate_fails_without_printing_a_success_report(self):
        with tempfile.TemporaryDirectory() as directory:
            stream = io.StringIO()
            with patch.object(identity, "observe_git_source", return_value=self.source), \
                    redirect_stdout(stream), redirect_stderr(io.StringIO()):
                result = capabilities.main(["--assembly", str(ASSEMBLY),
                    "--candidate", str(Path(directory) / "missing.json")])
            self.assertEqual(result, 1)
            self.assertEqual(stream.getvalue(), "")


if __name__ == "__main__":
    unittest.main()
