"""Behavioral model tests. These results are never physical HIL evidence."""

from __future__ import annotations

import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import (EvidenceError, atomic_json, command, digest, file_identity,
                                     parse_json, reject_secret_fields, run_adapter)
from scripts.evidence.build_inventory import generate, link_evidence, test_summary, verify
from scripts.evidence.release_gate import assemble, promote, verify_signature
from scripts.hil.run_hil import BoardLease, EquipmentLease, execute as hil_execute
from scripts.manufacturing.provision_device import execute as manufacture_execute


class AdapterFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.board = {"id": "board-1", "profile": "stm32-candidate", "revision": "A", "probe_serial": "probe-1"}
        self.image = self.root / "image.bin"
        self.image.write_bytes(b"firmware model fixture")
        self.firmware_sha = digest(self.image)
        self.config_sha = "a" * 64
        self.adapter = self.root / "model_adapter.py"
        self.adapter.write_text(
            "import json, sys, time\n"
            "data = json.load(open(sys.argv[1]))[sys.argv[2]]\n"
            "if isinstance(data, dict) and 'sleep' in data: time.sleep(data['sleep'])\n"
            "if isinstance(data, dict) and 'exit' in data: sys.exit(data['exit'])\n"
            "print(json.dumps(data))\n")
        self.fixture = self.root / "responses.json"
        self.serial = {"schema_version": 1, "board": self.board, "firmware_sha256": self.firmware_sha,
                       "config_sha256": self.config_sha, "tests": [{"id": "spi-contract", "status": "pass"}],
                       "metrics": {"control_deadline": {"value": 50, "unit": "us"}}}
        self.responses = {"identify": {"schema_version": 1, "board": self.board}, "flash": {},
                          "verify_flash": {"schema_version": 1, "board": self.board, "firmware_sha256": self.firmware_sha},
                          "reset": {}, "serial": self.serial, "cleanup": {}}
        self.manifest = {"schema_version": 1, "kind": "orchestrator_model", "lab_id": "model-tests",
                         "board": self.board, "firmware": {"path": str(self.image), "sha256": self.firmware_sha},
                         "config_sha256": self.config_sha, "adapter_files": [file_identity(self.adapter)],
                         "commands": {name: [sys.executable, str(self.adapter), str(self.fixture), name]
                                      for name in self.responses},
                         "operation_timeout_s": 2, "total_timeout_s": 10,
                         "required_tests": ["spi-contract"],
                         "budgets": {"control_deadline": {"maximum": 100, "unit": "us"}}}
        self.manifest_file = self.root / "manifest.json"
        self.report = self.root / "report.json"
        self.leases = self.root / "leases"

    def tearDown(self):
        self.temporary.cleanup()

    def run_hil(self):
        atomic_json(self.fixture, self.responses)
        atomic_json(self.manifest_file, self.manifest)
        return hil_execute(self.manifest_file, self.report, self.leases, model=True)

    def test_model_success_is_explicitly_not_physical_hil(self):
        result = self.run_hil()
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["kind"], "orchestrator_model")
        self.assertIs(result["eligible_physical_hil"], False)
        self.assertEqual([op["name"] for op in result["operations"]], list(self.responses))
        self.assertFalse((self.leases / self.board["id"]).exists())

    def test_model_manifest_cannot_run_as_physical(self):
        atomic_json(self.manifest_file, self.manifest)
        result = hil_execute(self.manifest_file, self.report, self.leases)
        self.assertEqual(result["status"], "fail")
        self.assertEqual(result["operations"], [])

    def test_competing_lease_preserves_current_owner(self):
        with BoardLease(self.leases, self.board["id"]):
            owner = (self.leases / self.board["id"] / "owner.json").read_bytes()
            result = self.run_hil()
            self.assertEqual(result["status"], "fail")
            self.assertEqual((self.leases / self.board["id"] / "owner.json").read_bytes(), owner)
            self.assertEqual(result["operations"], [])

    def test_probe_alias_cannot_race_a_second_board_id(self):
        first = {**self.board, "id": "another-logical-board"}
        with EquipmentLease(self.leases, first):
            result = self.run_hil()
            self.assertEqual(result["status"], "fail")
            self.assertEqual(result["operations"], [])
            self.assertFalse((self.leases / self.board["id"]).exists())
            self.assertTrue((self.leases / first["id"] / "owner.json").is_file())

    def test_wrong_board_is_rejected_before_flash_and_cleanup_runs(self):
        self.responses["identify"]["board"] = {**self.board, "revision": "B"}
        result = self.run_hil()
        self.assertEqual(result["status"], "fail")
        self.assertEqual([item["name"] for item in result["operations"]], ["identify", "cleanup"])

    def test_flash_readback_mismatch_stops_before_reset(self):
        self.responses["verify_flash"]["firmware_sha256"] = "b" * 64
        result = self.run_hil()
        self.assertEqual(result["status"], "fail")
        self.assertNotIn("reset", [item["name"] for item in result["operations"]])

    def test_missing_firmware_fails_without_hardware_commands(self):
        self.image.unlink()
        result = self.run_hil()
        self.assertEqual(result["status"], "fail")
        self.assertEqual(result["operations"], [])

    def test_changed_adapter_is_rejected(self):
        self.adapter.write_text("raise SystemExit(0)\n")
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_changed_firmware_is_rejected(self):
        self.image.write_bytes(b"different")
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_zero_tests_rejected(self):
        self.serial["tests"] = []
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_missing_required_test_rejected(self):
        self.serial["tests"] = [{"id": "other", "status": "pass"}]
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_skipped_test_rejected(self):
        self.serial["tests"][0]["status"] = "skipped"
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_duplicate_test_rejected(self):
        self.serial["tests"].append(self.serial["tests"][0])
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_resource_budget_exceeded_rejected(self):
        self.serial["metrics"]["control_deadline"]["value"] = 101
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_measurement_unit_mismatch_rejected(self):
        self.serial["metrics"]["control_deadline"]["unit"] = "ms"
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_missing_measurement_rejected(self):
        self.serial["metrics"] = {}
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_serial_config_mismatch_rejected(self):
        self.serial["config_sha256"] = "b" * 64
        self.assertEqual(self.run_hil()["status"], "fail")

    def test_timeout_stops_adapter_and_releases_board(self):
        self.responses["flash"] = {"sleep": 5}
        self.manifest["operation_timeout_s"] = 0.2
        result = self.run_hil()
        self.assertEqual(result["status"], "fail")
        self.assertIn("timed out", result["failure"])
        self.assertFalse((self.leases / self.board["id"]).exists())

    def test_failed_cleanup_blocks_hil_success(self):
        self.responses["cleanup"] = {"exit": 4}
        result = self.run_hil()
        self.assertEqual(result["status"], "fail")
        self.assertEqual(result["lease_status"], "quarantined")
        self.assertTrue((self.leases / self.board["id"] / "owner.json").is_file())
        self.assertEqual(len(list(self.leases.glob("probe-*/owner.json"))), 1)

    def test_report_cannot_overwrite_the_flash_image(self):
        atomic_json(self.fixture, self.responses)
        atomic_json(self.manifest_file, self.manifest)
        before = self.image.read_bytes()
        result = hil_execute(self.manifest_file, self.image, self.leases, model=True)
        self.assertEqual(result["status"], "fail")
        self.assertEqual(self.image.read_bytes(), before)
        self.assertEqual(result["operations"], [])


class CommonTests(unittest.TestCase):
    def test_duplicate_json_fields_rejected(self):
        with self.assertRaises(EvidenceError):
            parse_json('{"status":"fail","status":"pass"}')

    def test_nonfinite_json_is_rejected(self):
        with self.assertRaises(EvidenceError):
            parse_json('{"measurement":NaN}')

    def test_shell_metacharacters_are_literal_arguments(self):
        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / "marker"
            argument = "x; touch " + str(marker)
            result = run_adapter([sys.executable, "-c", "import sys; print(sys.argv[1])", argument], 2)
            self.assertEqual(result.decode().strip(), argument)
            self.assertFalse(marker.exists())

    def test_unknown_placeholder_rejected(self):
        with self.assertRaises(EvidenceError):
            command(["tool", "{unknown}"])

    def test_missing_executable_rejected(self):
        with self.assertRaises(EvidenceError):
            run_adapter(["nexus-no-such-adapter"], 1)

    def test_output_limit_rejected(self):
        with self.assertRaises(EvidenceError):
            run_adapter([sys.executable, "-c", "print('a' * 1024)"], 2, output_limit=100)

    def test_secret_fields_rejected_recursively(self):
        with self.assertRaises(EvidenceError):
            reject_secret_fields({"public_identity": [{"private_key": "DO-NOT-LOG"}]})


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.source = self.root / "source"
        self.source.mkdir()
        (self.source / "code.c").write_text("int main(void) { return 0; }\n")
        (self.source / "empty.h").write_bytes(b"")
        (self.source / "LICENSE").write_text("SPDX-License-Identifier: Apache-2.0\n")
        for args in (["init", "-q"], ["add", "."], ["-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "-qm", "fixture"]):
            subprocess.run(["git", "-C", str(self.source), *args], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.commit = subprocess.check_output(["git", "-C", str(self.source), "rev-parse", "HEAD"], text=True).strip()
        self.config = self.root / "effective.config"
        self.config.write_text("CONFIG_PLATFORM_NATIVE=y\n")
        self.header = self.root / "nexus_config.h"
        self.header.write_text("#define PLATFORM_NATIVE 1\n")
        self.cmake = self.root / "config.cmake"
        self.cmake.write_text("set(CONFIG_PLATFORM_NATIVE ON)\n")
        self.elf = self.root / "app.elf"
        self.elf.write_bytes(b"\x7fELF model artifact")
        self.archive = self.root / "libfixture.a"
        self.archive.write_bytes(b"archive model fixture")
        self.map = self.root / "app.map"
        self.map.write_text(str(self.archive) + "(code.c.o)\n")
        self.junit = self.root / "tests.xml"
        self.junit.write_text('<testsuite><testcase name="contract" classname="Fixture" /></testsuite>')
        self.manifest = {"schema_version": 1, "source_root": str(self.source),
                         "product": {"id": "model", "board_profile": "native", "board_revision": "not-applicable", "osal": "native"},
                         "configuration": {"effective": str(self.config), "header": str(self.header), "cmake": str(self.cmake)},
                         "toolchain": {"path": str(Path(sys.executable).resolve()), "version_args": ["--version"]},
                         "artifacts": [{"path": str(self.elf), "role": "elf"}],
                         "link_map": str(self.map),
                         "components": [{"name": "fixture", "version": "1", "license": "Apache-2.0",
                                         "license_file": str(self.source / "LICENSE"), "source_root": str(self.source), "archive": str(self.archive)}],
                         "tests": {"junit": str(self.junit), "source_commit": self.commit,
                                   "config_sha256": digest(self.config), "artifact_sha256": digest(self.elf)}}
        self.input = self.root / "input.json"
        self.output = self.root / "inventory.json"
        self.sbom = self.root / "sbom.json"

    def tearDown(self):
        self.temporary.cleanup()

    def inventory(self):
        atomic_json(self.input, self.manifest)
        return generate(self.input, self.output, self.sbom)

    def vendor_import(self):
        """Reviewed local fixture import, not evidence of a real SDK or board."""
        sdk = self.source / "vendors/fixture"
        (sdk / "Firmware").mkdir(parents=True)
        (sdk / "LICENSES").mkdir()
        (sdk / "README.md").write_text("Source import behavioral fixture only.\n")
        entries = []
        for name, license_id in (("driver.c", "BSD-3-Clause"), ("startup.c", "LicenseRef-Arm-Cortex-M-2012")):
            file = sdk / "Firmware" / name
            file.write_text("/* fixture " + name + " */\n")
            entries.append({"path": "Firmware/" + name, "sha256": digest(file),
                            "bytes": file.stat().st_size, "license": license_id})
            (sdk / "LICENSES" / license_id).write_text("Fixture notice: " + license_id + "\n")
        atomic_json(sdk / "source.lock.json", {"vendor": "Fixture", "package": "FixtureSDK", "version": "3.3.3",
                    "source_url": "https://example.invalid/reviewed-sdk.zip", "download_sha256": "a" * 64,
                    "nested_archive_sha256": "b" * 64, "files": entries})
        for args in (["add", "."], ["-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "-qm", "reviewed import fixture"]):
            subprocess.run(["git", "-C", str(self.source), *args], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.commit = subprocess.check_output(["git", "-C", str(self.source), "rev-parse", "HEAD"], text=True).strip()
        self.manifest["tests"]["source_commit"] = self.commit
        self.manifest["components"][0].update({"version": "3.3.3", "source_kind": "vendor-source-import",
                    "source_root": str(sdk), "license": "BSD-3-Clause AND LicenseRef-Arm-Cortex-M-2012",
                    "license_file": str(sdk / "LICENSES/BSD-3-Clause")})
        return sdk

    def test_explicit_vendor_import_records_upstream_without_fake_vendor_commit(self):
        self.vendor_import()
        inventory = self.inventory()
        verify(inventory)
        source = inventory["components"][0]["source"]
        self.assertEqual(source["kind"], "vendor-source-import")
        self.assertEqual(source["source_commit"], self.commit)
        self.assertEqual(source["files"], 2)
        self.assertEqual(len(source["notices"]), 3)
        self.assertNotIn("commit", source)
        component = json.loads(self.sbom.read_text())["components"][0]
        self.assertEqual(component["licenses"], [{"expression": "BSD-3-Clause AND LicenseRef-Arm-Cortex-M-2012"}])
        properties = {item["name"]: item["value"] for item in component["properties"]}
        self.assertNotIn("nexus:source:commit", properties)
        self.assertEqual(properties["nexus:source:owning-repository-commit"], self.commit)
        self.assertEqual(promote(self.output, level="candidate")["status"], "pass")
        with self.assertRaises(EvidenceError):
            promote(self.output, level="enterprise")

    def test_vendor_subtree_requires_explicit_source_kind(self):
        self.vendor_import()
        del self.manifest["components"][0]["source_kind"]
        with self.assertRaises(EvidenceError):
            self.inventory()

    def test_unknown_source_kind_rejected(self):
        self.manifest["components"][0]["source_kind"] = "guess-from-folder"
        with self.assertRaises(EvidenceError):
            self.inventory()

    def test_vendor_version_and_all_locked_license_terms_required(self):
        self.vendor_import()
        component = self.manifest["components"][0]
        for field, value in (("version", "3.3.2"), ("license", "BSD-3-Clause"),
                             ("license", "BSD-3-Clause OR LicenseRef-Arm-Cortex-M-2012"),
                             ("license", "BSD-3-Clause AND BSD-3-Clause"),
                             ("license_file", str(self.source / "LICENSE"))):
            with self.subTest(field=field, value=value):
                previous = component[field]
                component[field] = value
                with self.assertRaises(EvidenceError):
                    self.inventory()
                component[field] = previous

    def test_vendor_source_lock_and_notice_tamper_blocks_generation_and_gate(self):
        sdk = self.vendor_import()
        inventory = self.inventory()
        for name in ("Firmware/driver.c", "source.lock.json", "README.md", "LICENSES/BSD-3-Clause"):
            with self.subTest(path=name):
                file = sdk / name
                original = file.read_bytes()
                file.write_bytes(original + b"tampered\n")
                with self.assertRaises(EvidenceError):
                    self.inventory()
                with self.assertRaises(EvidenceError):
                    verify(inventory)
                with self.assertRaises(EvidenceError):
                    promote(self.output, level="candidate")
                file.write_bytes(original)

    def test_updating_vendor_lock_does_not_bypass_reviewed_commit(self):
        sdk = self.vendor_import()
        file = sdk / "Firmware/driver.c"
        file.write_text("/* changed without review */\n")
        lock = json.loads((sdk / "source.lock.json").read_text())
        lock["files"][0].update({"sha256": digest(file), "bytes": file.stat().st_size})
        atomic_json(sdk / "source.lock.json", lock)
        with self.assertRaises(EvidenceError):
            self.inventory()

    def test_forged_vendor_kind_and_identity_rejected(self):
        self.vendor_import()
        inventory = self.inventory()
        for field, value in (("kind", "git"), ("kind", "unknown"), ("source_commit", "f" * 40),
                             ("lock_sha256", "f" * 64), ("repository_root", str(self.root))):
            with self.subTest(field=field):
                forged = copy.deepcopy(inventory)
                forged["components"][0]["source"][field] = value
                with self.assertRaises(EvidenceError):
                    verify(forged)

    def test_vendor_sbom_upstream_or_license_forgery_rejected_even_with_new_file_digest(self):
        self.vendor_import()
        inventory = self.inventory()
        original = json.loads(self.sbom.read_text())
        for field, value in (("licenses", [{"license": {"id": "MIT"}}]),
                             ("externalReferences", [{"type": "distribution", "url": "https://example.invalid/other.zip"}])):
            with self.subTest(field=field):
                forged = copy.deepcopy(original)
                forged["components"][0][field] = value
                atomic_json(self.sbom, forged)
                inventory["sbom"] = file_identity(self.sbom)
                with self.assertRaises(EvidenceError):
                    verify(inventory)

    def test_vendor_import_does_not_hide_dirty_owner_repository(self):
        self.vendor_import()
        (self.source / "code.c").write_text("int unrelated_dirty_source;\n")
        inventory = self.inventory()
        self.assertTrue(inventory["source"]["dirty"])
        self.assertFalse(inventory["components"][0]["source"]["dirty"])
        with self.assertRaises(EvidenceError):
            promote(self.output, level="candidate")

    def test_inventory_records_actual_map_members_and_verifies(self):
        inventory = self.inventory()
        verify(inventory)
        self.assertEqual(inventory["components"][0]["link_evidence"][0]["line"], 1)
        self.assertEqual(inventory["tests"]["tests"], 1)
        self.assertEqual(json.loads(self.sbom.read_text())["bomFormat"], "CycloneDX")
        self.assertEqual(inventory["dependency_coverage"]["dynamic_runtime_review"], "pending")

    def test_changed_artifact_rejected(self):
        inventory = self.inventory()
        self.elf.write_bytes(b"tampered")
        with self.assertRaises(EvidenceError):
            verify(inventory)

    def test_changed_source_rejected(self):
        inventory = self.inventory()
        (self.source / "code.c").write_text("int different;\n")
        with self.assertRaises(EvidenceError):
            verify(inventory)

    def test_changed_sbom_rejected(self):
        inventory = self.inventory()
        self.sbom.write_text("{}\n")
        with self.assertRaises(EvidenceError):
            verify(inventory)

    def test_changed_inventory_binding_rejected(self):
        inventory = self.inventory()
        inventory["tests"]["artifact_sha256"] = "b" * 64
        with self.assertRaises(EvidenceError):
            verify(inventory)

    def test_sbom_config_or_dependency_forgery_rejected_with_refreshed_digest(self):
        inventory = self.inventory()
        original = json.loads(self.sbom.read_text())
        for defect in ("config", "dependency"):
            with self.subTest(defect=defect):
                forged = copy.deepcopy(original)
                if defect == "config":
                    forged["metadata"]["properties"][0]["value"] = "f" * 64
                else:
                    forged["dependencies"][0]["dependsOn"] = []
                atomic_json(self.sbom, forged)
                inventory["sbom"] = file_identity(self.sbom)
                with self.assertRaises(EvidenceError):
                    verify(inventory)

    def test_forged_sbom_coverage_rejected(self):
        inventory = self.inventory()
        inventory["dependency_coverage"]["dynamic_runtime_review"] = "complete"
        with self.assertRaises(EvidenceError):
            verify(inventory)

    def test_unlinked_archive_rejected(self):
        self.map.write_text("LOAD " + str(self.archive) + "\n")
        with self.assertRaises(EvidenceError):
            self.inventory()

    def test_zero_tests_rejected(self):
        self.junit.write_text("<testsuite />")
        with self.assertRaises(EvidenceError):
            self.inventory()

    def test_failed_junit_rejected(self):
        self.junit.write_text('<testsuite><testcase name="contract"><failure /></testcase></testsuite>')
        with self.assertRaises(EvidenceError):
            self.inventory()

    def test_contradictory_junit_summary_rejected(self):
        self.junit.write_text('<testsuite failures="1"><testcase name="contract" /></testsuite>')
        with self.assertRaises(EvidenceError):
            self.inventory()

    def test_unbound_test_identity_rejected(self):
        self.manifest["tests"]["config_sha256"] = "b" * 64
        with self.assertRaises(EvidenceError):
            self.inventory()

    def test_clean_candidate_passes_without_claiming_enterprise(self):
        self.inventory()
        result = promote(self.output, level="candidate")
        self.assertEqual(result["status"], "pass")
        self.assertIn("candidate draft only", result["limitations"])

    def test_dirty_candidate_is_blocked(self):
        (self.source / "code.c").write_text("int changed;\n")
        self.inventory()
        with self.assertRaises(EvidenceError):
            promote(self.output, level="candidate")

    def test_enterprise_requires_actual_evidence_inputs(self):
        self.inventory()
        with self.assertRaises(EvidenceError):
            promote(self.output, level="enterprise")

    def test_missing_signature_is_never_accepted(self):
        self.inventory()
        with self.assertRaises(EvidenceError):
            verify_signature(self.output, self.root / "no-public-key", self.root / "no-signature", "a" * 64)

    def test_evidence_assembly_binds_all_four_reports(self):
        self.inventory()
        policy = self.root / "policy.json"
        hil = self.root / "hil.json"
        reviews = self.root / "reviews.json"
        for file in (policy, hil, reviews):
            file.write_text("{}\n")
        payload = assemble(self.output, policy, hil, reviews, self.root / "payload.json")
        self.assertEqual(payload["inventory_sha256"], digest(self.output))
        self.assertEqual(payload["physical_hil_sha256"], digest(hil))


class ManufacturingTests(unittest.TestCase):
    """Manufacturing model tests; no station or device is touched."""

    setUp = AdapterFixture.setUp
    tearDown = AdapterFixture.tearDown

    def manufacture(self):
        responses = copy.deepcopy(self.responses)
        responses["identify"] = {"schema_version": 1, "board": self.board, "device_uid": "chip-uid-1"}
        responses["verify_flash"] = {"schema_version": 1, "device_uid": "chip-uid-1", "firmware_sha256": self.firmware_sha}
        responses["calibrate"] = {"schema_version": 1, "status": "pass", "limits_id": "limits-v1",
                                  "measurements": {"supply": {"value": 3.3, "unit": "V"}}}
        responses["provision"] = {"schema_version": 1, "status": "pass", "device_uid": "chip-uid-1", "identity_id": "identity-1"}
        responses["attest"] = {"schema_version": 1, "status": "pass", "device_uid": "chip-uid-1", "identity_id": "identity-1",
                               "public_key_sha256": "c" * 64, "certificate_sha256": "d" * 64}
        responses.pop("serial")
        responses.update(getattr(self, "manufacturing_overrides", {}))
        atomic_json(self.fixture, responses)
        manifest = {"schema_version": 1, "kind": "manufacturing_station", "lot_id": "lot-1", "station_id": "model-station",
                    "operator_role": "manufacturing-interface", "board": self.board, "enterprise_gate": "model-test-only",
                    "adapter_files": [file_identity(self.adapter)], "operation_timeout_s": 2,
                    "key_reference": "hsm-opaque-reference", "identity_id": "identity-1",
                    "commands": {name: [sys.executable, str(self.adapter), str(self.fixture), name] for name in responses},
                    "calibration_requirements": {"limits_id": "limits-v1", "measurements": {"supply": {"minimum": 3.0, "maximum": 3.6, "unit": "V"}}}}
        gate_file = self.root / "gate-model.json"
        gate_file.write_text("{}\n")
        manifest["enterprise_gate"] = str(gate_file)
        atomic_json(self.manifest_file, manifest)
        inventory = {"product": {"board_profile": self.board["profile"], "board_revision": self.board["revision"]},
                     "artifacts": [{"path": str(self.image), "role": "image", "sha256": self.firmware_sha}],
                     "configuration": {"effective": {"sha256": self.config_sha}}, "source": {"commit": "e" * 40}}
        with patch("scripts.manufacturing.provision_device.verified_release", return_value=inventory):
            return manufacture_execute(self.manifest_file, self.root / "audit", self.report, self.leases)

    def test_model_provisioning_stores_only_public_audit(self):
        result = self.manufacture()
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["device_uid"], "chip-uid-1")
        self.assertEqual(result["public_identity"]["certificate_sha256"], "d" * 64)
        result_bytes = (self.root / "audit/devices/chip-uid-1/result.json").read_text()
        self.assertNotIn("hsm-opaque-reference", result_bytes)
        self.assertTrue((self.root / "audit/identities/identity-1/owner.json").is_file())

    def test_duplicate_uid_does_not_overwrite_previous_audit(self):
        self.assertEqual(self.manufacture()["status"], "pass")
        result_file = self.root / "audit/devices/chip-uid-1/result.json"
        prior = result_file.read_bytes()
        self.assertEqual(self.manufacture()["status"], "fail")
        self.assertEqual(result_file.read_bytes(), prior)

    def test_failed_calibration_prevents_key_provision(self):
        self.manufacturing_overrides = {"calibrate": {"schema_version": 1, "status": "pass", "limits_id": "limits-v1",
                                                     "measurements": {"supply": {"value": 9, "unit": "V"}}}}
        result = self.manufacture()
        self.assertEqual(result["status"], "fail")
        self.assertNotIn("provision", [item["name"] for item in result["operations"]])

    def test_secret_adapter_output_is_not_written_to_audit(self):
        self.manufacturing_overrides = {"provision": {"private_key": "SECRET-DO-NOT-SAVE"}}
        result = self.manufacture()
        self.assertEqual(result["status"], "fail")
        self.assertNotIn("SECRET-DO-NOT-SAVE", self.report.read_text())
        self.assertNotIn("SECRET-DO-NOT-SAVE", (self.root / "audit/devices/chip-uid-1/result.json").read_text())

    def test_unconfigured_station_fails_before_adapter_execution(self):
        atomic_json(self.manifest_file, {})
        result = manufacture_execute(self.manifest_file, self.root / "audit", self.report, self.leases)
        self.assertEqual(result["status"], "fail")
        self.assertEqual(result["operations"], [])


if __name__ == "__main__":
    unittest.main()
