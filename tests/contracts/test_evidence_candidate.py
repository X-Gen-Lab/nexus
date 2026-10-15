"""Candidate gate faults and immutable packaging, using isolated model evidence."""

import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.candidate import (REQUIRED_SCOPES, collect_evidence_closure,
    collect_identities, seal, validate_manifest, validate_qualification, verify)
from tools.evidence.common import EvidenceError, atomic_json, file_identity, load_json
from tools.evidence.identity import git_source
from tools.evidence.qualification import create_qualification
from tools.evidence.reproduce import run_logged
from tools.measurement.elf import Elf32, binary_from_elf
from tests.contracts.test_evidence_support import link_fixture


class CandidateGateTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.checkout = self.root / "source"
        self.checkout.mkdir()
        subprocess.run(["git", "init", "-q", str(self.checkout)], check=True)
        (self.checkout / "source.txt").write_text("candidate verifier unit-test fixture\n")
        subprocess.run(["git", "-C", str(self.checkout), "add", "."], check=True)
        subprocess.run(["git", "-C", str(self.checkout), "-c", "user.name=Test Fixture",
            "-c", "user.email=fixture@example.invalid", "commit", "-qm", "test fixture"], check=True)
        self.source = git_source(self.checkout)
        raw = run_logged([sys.executable, "-c", "print('actual verifier unit-test command')"],
                         self.root / "raw.log")
        self.checks = [{"name": "unit_model_command", **raw}]

    def prepared(self):
        elf, resolved, _ = link_fixture(self.root)
        binary = self.root / "image.bin"
        binary.write_bytes(binary_from_elf(Elf32(elf.read_bytes()), 0x08000000, 0x08100000))
        lock = self.root / "dependencies.json"
        atomic_json(lock, {"kind": "test_fixture_dependency_lock"})
        environment = self.root / "environment.json"
        atomic_json(environment, {"kind": "environment_validation_mock_only"})
        qualifications = []
        for scope in sorted(REQUIRED_SCOPES):
            path = self.root / (scope + ".json")
            atomic_json(path, create_qualification(scope, self.source, resolved, elf, self.checks))
            qualifications.append(file_identity(path))
        manifest = {"schema_version": 1, "candidate_id": "verifier-test-model",
            "source": self.source, "dependencies": [file_identity(lock)],
            "environment": file_identity(environment), "configuration": file_identity(resolved),
            "artifacts": {"elf": file_identity(elf), "bin": file_identity(binary),
                          "map": file_identity(self.root / "image.map")},
            "qualifications": qualifications}
        return manifest, elf, resolved

    def test_empty_skipped_failed_and_stale_qualification_rejected(self):
        manifest, elf, resolved = self.prepared()
        config = json.loads(resolved.read_text())
        original = create_qualification("host_contracts", self.source, resolved, elf, self.checks)
        variations = [{**original, "executed_checks": []}, {**original, "status": "skipped"},
                      {**original, "elf_sha256": "0" * 64}]
        value = copy.deepcopy(original)
        value["executed_checks"][0]["exit_code"] = 1
        variations.append(value)
        for value in variations:
            with self.subTest(value=value), self.assertRaises(EvidenceError):
                validate_qualification(value, manifest, config)

    def test_failed_actual_command_cannot_generate_qualification(self):
        _, elf, resolved = self.prepared()
        execution = run_logged([sys.executable, "-c", "raise SystemExit(17)"], self.root / "failed.log")
        with self.assertRaises(EvidenceError):
            create_qualification("host_contracts", self.source, resolved, elf,
                                 [{"name": "failed_actual_command", **execution}])

    def test_unbound_fixture_slots_preserve_bytes_and_actual_references(self):
        artifact = self.root / "probe.cfg"
        artifact.write_text("actual fixture configuration\n")
        fixture = self.root / "station.template.json"
        atomic_json(fixture, {"kind": "hil_station", "station_id": None, "tool": {
            "path": None, "sha256": None, "size": None},
            "bound_configuration": file_identity(artifact)})
        closure = collect_evidence_closure([file_identity(fixture)])
        self.assertEqual({item["sha256"] for item in closure},
                         {file_identity(fixture)["sha256"], file_identity(artifact)["sha256"]})
        with self.assertRaises(EvidenceError):
            collect_identities({"path": None, "sha256": None, "size": None})
        with self.assertRaises(EvidenceError):
            collect_identities({"path": str(artifact), "sha256": None, "size": None})
        original = load_json(fixture)
        for changed in [{**original, "kind": "execution_context"},
                        {**original, "station_id": "declared-station"},
                        {**original, "tool": {"path": str(artifact), "sha256": None,
                                               "size": None}}]:
            atomic_json(fixture, changed)
            with self.assertRaises(EvidenceError):
                collect_evidence_closure([file_identity(fixture)])
        atomic_json(fixture, original)
        manifest, elf, resolved = self.prepared()
        with self.assertRaises(EvidenceError):
            create_qualification("hil_tooling", self.source, resolved, elf, [{
                "name": "unbound_raw", "exit_code": 0,
                "raw": {"path": None, "sha256": None, "size": None}}])
        with self.assertRaises(EvidenceError):
            create_qualification("hil_tooling", self.source, resolved, elf, [{
                **self.checks[0], "artifacts": [{"path": None, "sha256": None, "size": None}]}])
        identity = next(item for item in manifest["qualifications"]
                        if "hil_tooling" in item["path"])
        report_path = Path(identity["path"])
        report = load_json(report_path)
        report["evidence"] = [file_identity(fixture)]
        atomic_json(report_path, report)
        manifest["qualifications"] = [file_identity(report_path) if item == identity else item
                                      for item in manifest["qualifications"]]
        path = self.root / "manifest.json"
        atomic_json(path, manifest)
        # Scope trust is tested separately; this model tests fixture byte preservation.
        with patch("tools.evidence.candidate.verify_environment", return_value={}), \
                patch("tools.evidence.candidate.validate_scope_proof", return_value=([], None)):
            sealed = seal(path, self.root / "sealed")
        self.assertEqual(sealed["physical_status"], "not_executed")
        self.assertIn(file_identity(fixture)["sha256"],
                      {item["sha256"] for item in sealed["payloads"]})
        with patch("tools.evidence.candidate.validate_scope_proof", return_value=([], None)):
            verify(self.root / "sealed/candidate.json")

    def test_missing_qualification_blocks_seal(self):
        manifest, _, _ = self.prepared()
        manifest["qualifications"].pop()
        with patch("tools.evidence.candidate.verify_environment", return_value={}):
            with self.assertRaisesRegex(EvidenceError, "incomplete required"):
                validate_manifest(manifest)

    def test_generic_success_without_scope_proof_is_rejected(self):
        manifest, _, _ = self.prepared()
        with patch("tools.evidence.candidate.verify_environment", return_value={}):
            with self.assertRaisesRegex(EvidenceError, "evidence document"):
                validate_manifest(manifest)

    def test_dirty_source_is_rejected(self):
        manifest, _, _ = self.prepared()
        (self.checkout / "source.txt").write_text("changed source after checks")
        with self.assertRaisesRegex(EvidenceError, "stale"):
            validate_manifest(manifest)

    def test_binary_different_from_elf_is_rejected(self):
        manifest, _, _ = self.prepared()
        path = Path(manifest["artifacts"]["bin"]["path"])
        path.write_bytes(b"wrong binary")
        manifest["artifacts"]["bin"] = file_identity(path)
        with patch("tools.evidence.candidate.verify_environment", return_value={}):
            with self.assertRaisesRegex(EvidenceError, "BIN differs"):
                validate_manifest(manifest)

    def test_seal_preserves_same_hash_and_tamper_is_rejected(self):
        manifest, _, _ = self.prepared()
        path = self.root / "manifest.json"
        atomic_json(path, manifest)
        destination = self.root / "sealed"
        # Docker trust is tested separately; this test isolates byte-preserving packing.
        with patch("tools.evidence.candidate.verify_environment", return_value={}), \
                patch("tools.evidence.candidate.validate_scope_proof", return_value=([], None)):
            sealed = seal(path, destination)
        self.assertEqual(sealed["artifacts"]["elf"]["sha256"], manifest["artifacts"]["elf"]["sha256"])
        self.assertEqual(sealed["physical_status"], "not_executed")
        with patch("tools.evidence.candidate.validate_scope_proof", return_value=([], None)):
            verify(destination / "candidate.json")
        packed = destination / sealed["artifacts"]["elf"]["path"]
        packed.write_bytes(packed.read_bytes() + b"tampered")
        with self.assertRaises(EvidenceError):
            verify(destination / "candidate.json")

    def test_nested_execution_artifact_is_sealed_and_required_on_verify(self):
        manifest, _, _ = self.prepared()
        extra = self.root / "execution-artifact.txt"
        extra.write_text("actual nested artifact bytes\n")
        context = self.root / "execution-context.json"
        atomic_json(context, {"artifact": file_identity(extra)})
        identity = next(item for item in manifest["qualifications"]
                        if "host_contracts" in item["path"])
        report_path = Path(identity["path"])
        report = json.loads(report_path.read_text())
        report["evidence"] = [file_identity(context)]
        atomic_json(report_path, report)
        manifest["qualifications"] = [file_identity(report_path) if item == identity else item
                                      for item in manifest["qualifications"]]
        path = self.root / "manifest.json"
        atomic_json(path, manifest)
        destination = self.root / "sealed"
        with patch("tools.evidence.candidate.verify_environment", return_value={}), \
                patch("tools.evidence.candidate.validate_scope_proof", return_value=([], None)):
            seal(path, destination)
        with patch("tools.evidence.candidate.validate_scope_proof", return_value=([], None)):
            verify(destination / "candidate.json")
        extra_sha = file_identity(extra)["sha256"]
        (destination / "payloads" / extra_sha).unlink()
        candidate_path = destination / "candidate.json"
        packed = json.loads(candidate_path.read_text())
        packed["payloads"] = [item for item in packed["payloads"] if item["sha256"] != extra_sha]
        atomic_json(candidate_path, packed)
        with self.assertRaises(EvidenceError):
            verify(destination / "candidate.json")

    def test_missing_raw_payload_rejected_after_seal(self):
        manifest, _, _ = self.prepared()
        path = self.root / "manifest.json"
        atomic_json(path, manifest)
        destination = self.root / "sealed"
        with patch("tools.evidence.candidate.verify_environment", return_value={}), \
                patch("tools.evidence.candidate.validate_scope_proof", return_value=([], None)):
            seal(path, destination)
        (destination / "payloads" / self.checks[0]["raw"]["sha256"]).unlink()
        with self.assertRaises(EvidenceError):
            verify(destination / "candidate.json")


if __name__ == "__main__":
    unittest.main()
