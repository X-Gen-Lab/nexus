"""Public evidence boundary tests, including real maintained signature verification."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import EvidenceError, atomic_json, digest, file_identity
from scripts.evidence.check_traceability import validate as validate_traceability
from scripts.evidence.release_gate import validate_policy, verify_signature
from scripts.evidence.resource_checks import parse_size, valgrind_errors


class ResourceReportTests(unittest.TestCase):
    def test_real_size_sections_are_parsed_without_invented_limits(self):
        sections = parse_size(b"app.elf :\nsection size addr\n.text 4096 134217728\n.bss 512 0x20000000\nTotal 4608\n")
        self.assertEqual(sections[0], {"name": ".text", "bytes": 4096, "address": 134217728})
        self.assertEqual(sections[1]["address"], 0x20000000)

    def test_empty_size_output_is_rejected(self):
        with self.assertRaises(EvidenceError):
            parse_size(b"app.elf :\nsection size addr\n")

    def test_duplicate_size_section_is_rejected(self):
        with self.assertRaises(EvidenceError):
            parse_size(b".text 100 0\n.text 100 0\n")

    def check_memcheck(self, body: str):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "memcheck.xml"
            path.write_text("<valgrindoutput><protocoltool>memcheck</protocoltool>" + body + "</valgrindoutput>")
            return valgrind_errors(path)

    def test_finished_memory_execution_can_count_reachable_state(self):
        result = self.check_memcheck("<status><state>FINISHED</state></status><error><kind>Leak_StillReachable</kind></error>")
        self.assertEqual(result["reachable_retained_records"], 1)

    def test_incomplete_memory_execution_is_rejected(self):
        with self.assertRaises(EvidenceError):
            self.check_memcheck("<status><state>RUNNING</state></status>")

    def test_invalid_access_is_rejected(self):
        with self.assertRaises(EvidenceError):
            self.check_memcheck("<status><state>FINISHED</state></status><error><kind>InvalidRead</kind></error>")

    def test_lost_allocation_is_rejected(self):
        with self.assertRaises(EvidenceError):
            self.check_memcheck("<status><state>FINISHED</state></status><error><kind>Leak_DefinitelyLost</kind></error>")


class TraceabilityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        repository = Path(__file__).resolve().parents[2]
        self.roles = json.loads((repository / ".github/maintainer-roles.json").read_text())
        self.requirements = json.loads((repository / "docs/requirements/industrial-reference.json").read_text())
        self.backlog = self.root / "backlog.csv"
        shutil.copyfile(repository / "docs/strategy/backlog.csv", self.backlog)
        self.roles_file = self.root / "roles.json"
        self.requirements_file = self.root / "requirements.json"

    def tearDown(self):
        self.temporary.cleanup()

    def validate(self):
        atomic_json(self.roles_file, self.roles)
        atomic_json(self.requirements_file, self.requirements)
        return validate_traceability(self.requirements_file, self.backlog, self.roles_file)

    def test_approved_roles_map_eight_unaccepted_product_requirements(self):
        report = self.validate()
        self.assertEqual(report["allocated_people"], 10)
        self.assertEqual(report["requirements"], 8)
        self.assertEqual(report["status_counts"]["planned"], 8)

    def test_unknown_owner_role_is_rejected(self):
        self.requirements["requirements"][0]["owner_role"] = "imaginary-person"
        with self.assertRaises(EvidenceError):
            self.validate()

    def test_unmapped_backlog_identity_is_rejected(self):
        self.requirements["requirements"][0]["backlog_ids"] = ["NOT-REAL"]
        with self.assertRaises(EvidenceError):
            self.validate()

    def test_unfunded_extra_role_seat_is_rejected(self):
        self.roles["roles"][0]["seats"] = 2
        with self.assertRaises(EvidenceError):
            self.validate()

    def test_verified_claim_without_evidence_is_rejected(self):
        self.requirements["requirements"][0]["status"] = "host_validated"
        with self.assertRaises(EvidenceError):
            self.validate()

    def test_model_evidence_cannot_validate_hardware(self):
        report = self.root / "model-hil.json"
        atomic_json(report, {"kind": "orchestrator_model", "status": "pass", "eligible_physical_hil": False})
        requirement = self.requirements["requirements"][1]
        requirement["status"] = "hardware_validated"
        requirement["evidence"] = [{"kind": "physical_hil", "identity": file_identity(report)}]
        with self.assertRaises(EvidenceError):
            self.validate()


class MaintainedSignatureTests(unittest.TestCase):
    """Ephemeral test keys are generated in a deleted temp directory, never fixtures."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.private = self.root / "ephemeral-private.pem"
        self.public = self.root / "public.pem"
        self.payload = self.root / "public-evidence.json"
        self.signature = self.root / "signature.bin"
        self.openssl = shutil.which("openssl")
        self.assertIsNotNone(self.openssl, "maintained OpenSSL CLI required for signature boundary tests")
        self.payload.write_text('{"kind":"ephemeral-signature-test"}\n')
        self.execute("genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(self.private))
        self.private.chmod(0o600)
        self.execute("pkey", "-in", str(self.private), "-pubout", "-out", str(self.public))
        self.execute("dgst", "-sha256", "-sign", str(self.private), "-out", str(self.signature), str(self.payload))

    def execute(self, *args):
        subprocess.run([self.openssl, *args], check=True, stdin=subprocess.DEVNULL,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)

    def tearDown(self):
        self.temporary.cleanup()

    def test_real_detached_signature_verifies_without_private_material(self):
        report = verify_signature(self.payload, self.public, self.signature, digest(self.public))
        self.assertEqual(report["status"], "pass")
        self.assertEqual(report["signed_payload_sha256"], digest(self.payload))
        self.assertNotIn(str(self.private), json.dumps(report))

    def test_tampered_evidence_is_rejected(self):
        self.payload.write_text('{"kind":"tampered"}\n')
        with self.assertRaises(EvidenceError):
            verify_signature(self.payload, self.public, self.signature, digest(self.public))

    def test_changed_trust_anchor_is_rejected(self):
        with self.assertRaises(EvidenceError):
            verify_signature(self.payload, self.public, self.signature, "0" * 64)

    def test_private_key_is_rejected_before_verification(self):
        with self.assertRaises(EvidenceError):
            verify_signature(self.payload, self.private, self.signature, digest(self.private))

    def test_missing_maintained_verifier_is_rejected(self):
        with self.assertRaises(EvidenceError):
            verify_signature(self.payload, self.public, self.signature, digest(self.public), "nexus-no-such-openssl")


class ReleasePolicyTests(unittest.TestCase):
    def policy(self):
        return {"schema_version": 1, "product_id": "industrial-reference", "board_profile": "candidate",
                "board_revision": "A", "trusted_public_key_sha256": "a" * 64,
                "required_tests": ["dma-cancel"], "budgets": {"deadline": {"maximum": 100, "unit": "us"}},
                "required_reviews": ["license"], "runtime_components": []}

    def test_nonfinite_hardware_budget_is_rejected(self):
        policy = self.policy()
        policy["budgets"]["deadline"]["maximum"] = float("nan")
        with self.assertRaises(EvidenceError):
            validate_policy(policy)

    def test_empty_hardware_requirements_are_rejected(self):
        policy = self.policy()
        policy["required_tests"] = []
        with self.assertRaises(EvidenceError):
            validate_policy(policy)


if __name__ == "__main__":
    unittest.main()
