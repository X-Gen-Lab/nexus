"""Candidate scope gates recheck actual ELF budgets and reject summary-only proof."""

import copy
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from tools.evidence.common import EvidenceError, atomic_json, file_identity, stream_digest, verify_file_identity
from tools.evidence.proof import (validate_resources, validate_scope_proof, sdk_file_identity,
    verify_sdk_file)
from tools.measurement.resources import measure
from tests.contracts.test_evidence_support import link_fixture


class ScopeProofTests(unittest.TestCase):
    def test_chunk_hash_does_not_depend_on_python_311_file_digest(self):
        data = bytes(range(256)) * 8200
        self.assertEqual(stream_digest(io.BytesIO(data)), hashlib.sha256(data).hexdigest())

    def test_budget_proof_is_recomputed_from_actual_elf(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf, resolved_path, budget = link_fixture(root)
            resource = measure(elf, resolved_path, budget)
            path = root / "resource.json"
            atomic_json(path, resource)
            report = {"scope": "resource_budget", "evidence": [file_identity(path)]}
            manifest = {"configuration": file_identity(resolved_path),
                        "artifacts": {"elf": file_identity(elf)}}
            resolved = json.loads(resolved_path.read_text())
            extra, sdk = validate_scope_proof(report, manifest, resolved)
            self.assertIsNone(sdk)
            self.assertIn(resource["budget"], extra)
            changed = copy.deepcopy(resource)
            changed["metrics"]["ram"]["sram"]["reserved_bytes"] -= 1
            atomic_json(path, changed)
            report["evidence"] = [file_identity(path)]
            with self.assertRaisesRegex(EvidenceError, "actual ELF/budget"):
                validate_scope_proof(report, manifest, resolved)

    def test_changed_declared_input_is_rejected_even_with_passed_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf, resolved, budget = link_fixture(root)
            resource = measure(elf, resolved, budget)
            path = root / "resource.json"
            atomic_json(path, resource)
            (root / "image.c").write_text("input changed after budget measurement")
            with self.assertRaisesRegex(EvidenceError, "identity changed"):
                validate_resources({"evidence": [file_identity(path)]},
                    {"configuration": file_identity(resolved), "artifacts": {"elf": file_identity(elf)}},
                    json.loads(resolved.read_text()), verify_file_identity)

    def test_sdk_empty_source_file_is_preserved_but_tamper_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "empty.c"
            path.write_bytes(b"")
            identity = sdk_file_identity(path)
            self.assertEqual(identity["size"], 0)
            self.assertEqual(verify_sdk_file(identity), path)
            path.write_bytes(b"modified")
            with self.assertRaises(EvidenceError):
                verify_sdk_file(identity)

    def test_missing_actual_reproduction_and_sdk_documents_are_rejected(self):
        for scope in ("reproducibility", "source_sdk"):
            with self.subTest(scope=scope), self.assertRaisesRegex(EvidenceError, "evidence document"):
                validate_scope_proof({"scope": scope, "evidence": []}, {}, {})


if __name__ == "__main__":
    unittest.main()
