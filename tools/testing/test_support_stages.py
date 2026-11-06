"""Support queries replay original phase evidence without executing producers."""

import copy
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure
from tools.dev import dev, process
from tools.evidence import candidate, identity
from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.qualification import create_qualification
from tools.maintenance import capabilities, support
from tools.measurement.elf import Elf32, binary_from_elf, image_window
from tools.measurement.resources import measure
from tests.contracts.test_evidence_support import link_fixture
from tests.contracts import test_evidence_scope_proof as scope_fixtures


ROOT = Path(__file__).resolve().parents[2]
ASSEMBLY = ROOT / "tools/configure/assemblies/qiming-baremetal-uart.toml"
FIXTURE = ROOT / "tools/hil/fixtures/stm32f407_qiming_v31.json"


class SupportStageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.source = {"root": str(ROOT), "commit": "1" * 40,
                       "tree": "2" * 40, "dirty": False, "submodules": []}
        self.resolved = configure.resolve_ir(ASSEMBLY, ROOT).to_dict()
        self.config = self.directory / "resolved.json"
        atomic_json(self.config, self.resolved)
        self.artifacts = {}
        for role in ("elf", "bin", "map"):
            path = self.directory / ("unlinked." + role)
            path.write_text("Not linked: only the preparation scope uses this fixture\n")
            self.artifacts[role] = file_identity(path)
        self.manifest = {"schema_version": 1, "candidate_id": "preparation-model",
            "source": self.source, "configuration": file_identity(self.config),
            "artifacts": self.artifacts, "qualifications": []}
        self.manifest_path = self.directory / "manifest.json"

    def preparation(self):
        report = self.directory / "preparation.json"
        argv = [sys.executable, "-B", str(ROOT / "tools/hil/prepare.py"),
                "--fixture", str(FIXTURE), "--resolved", str(self.config),
                "--report", str(report)]
        log = self.directory / "prepare.txt"
        result = process.run(argv, cwd=ROOT, env=process.execution_environment(os.environ),
                             log=log, stage="support-preparation")
        self.assertEqual(result.returncode, 0, result.stderr)
        proof = create_qualification("hil_tooling", self.source, self.config,
            Path(self.artifacts["elf"]["path"]), [{"name": "prepare",
                "argv": argv, "exit_code": result.returncode,
                "raw": file_identity(log)}], evidence=[file_identity(report)])
        path = self.directory / "qualification.json"
        atomic_json(path, proof)
        self.manifest["qualifications"] = [file_identity(path)]
        return path

    def query(self, **kwargs):
        atomic_json(self.manifest_path, self.manifest)
        with patch.object(identity, "observe_git_source", return_value=self.source):
            return capabilities.selection(ASSEMBLY, ROOT,
                manifest_path=self.manifest_path, **kwargs)

    def test_actual_preparation_is_verified_without_promoting_arm_or_hardware(self):
        self.preparation()
        with patch.object(process, "run", side_effect=AssertionError("query ran a producer")):
            result = self.query()
        stage = result["staged_evidence"]["hil_tooling"]
        self.assertEqual(stage["status"], "verified")
        self.assertEqual(stage["trust"], "semantic_verified")
        self.assertEqual(result["validation"]["hil_tooling"], "passed")
        for name in ("host_models", "arm_image", "installed_sdk", "software_candidate",
                     "physical_hil"):
            self.assertEqual(result["validation"][name], "not_assessed")
        self.assertFalse(result["physical_qualified"])
        self.assertIn("host_contracts", result["next_missing_evidence"])

    def test_rehashed_success_text_cannot_establish_a_model_execution(self):
        raw = self.directory / "success.txt"
        raw.write_text("All Native models and physical tests passed\n")
        report = {"schema_version": 1, "kind": "software_qualification",
            "scope": "host_contracts", "status": "passed", "source": self.source,
            "configuration_sha256": self.resolved["configuration_sha256"],
            "elf_sha256": self.artifacts["elf"]["sha256"],
            "executed_checks": [{"name": "forged", "status": "passed", "exit_code": 0,
                                  "raw": file_identity(raw)}]}
        path = self.directory / "model.json"
        atomic_json(path, report)
        self.manifest["qualifications"] = [file_identity(path)]
        result = self.query()
        self.assertEqual(result["staged_evidence"]["host_contracts"]["status"], "corrupt")
        self.assertEqual(result["validation"]["host_models"], "not_assessed")

    def test_missing_original_and_changed_original_have_different_statuses(self):
        path = self.preparation()
        original = path.read_bytes()
        path.unlink()
        missing = self.query()["staged_evidence"]["qualification:0"]
        self.assertEqual(missing["status"], "missing")
        path.write_bytes(original + b" ")
        corrupt = self.query()["staged_evidence"]["qualification:0"]
        self.assertEqual(corrupt["status"], "corrupt")

    def test_historical_source_retains_verified_scope_without_current_status(self):
        self.preparation()
        self.source = {**self.source, "commit": "3" * 40}
        result = self.query()
        stage = result["staged_evidence"]["hil_tooling"]
        self.assertEqual(stage["status"], "historical_source")
        self.assertEqual(stage["trust"], "semantic_verified")
        self.assertEqual(result["validation"]["hil_tooling"], "not_assessed")

    def test_another_configuration_is_never_relabelled_current(self):
        self.preparation()
        other = ROOT / "tools/configure/assemblies/sky-baremetal-uart.toml"
        atomic_json(self.manifest_path, self.manifest)
        with patch.object(identity, "observe_git_source", return_value=self.source):
            result = capabilities.selection(other, ROOT, manifest_path=self.manifest_path)
        self.assertEqual(result["staged_evidence"]["hil_tooling"]["status"],
                         "configuration_mismatch")
        self.assertEqual(result["validation"]["hil_tooling"], "not_assessed")

    def test_leaf_navigation_verifies_only_explicit_leaf_references(self):
        self.preparation()
        atomic_json(self.manifest_path, self.manifest)
        index = self.directory / "index.json"
        leaf = {"source": self.source, "resolved": self.resolved,
                "support_evidence": {"manifest": file_identity(self.manifest_path)}}
        atomic_json(index, {"support_evidence": leaf["support_evidence"],
                            "iterations": {"exact": leaf, "unbound": {"status": "passed"}}})
        with patch.object(identity, "observe_git_source", return_value=self.source):
            result = capabilities.selection(ASSEMBLY, ROOT, results_index=index,
                record_pointer="/iterations/exact", verify_record=True)
            unbound = capabilities.selection(ASSEMBLY, ROOT, results_index=index,
                record_pointer="/iterations/unbound", verify_record=True)
        self.assertEqual(result["staged_evidence"]["hil_tooling"]["status"], "verified")
        self.assertNotIn("staged_evidence", unbound)
        self.assertEqual(unbound["validation"]["hil_tooling"], "not_assessed")
        self.assertEqual(unbound["indexed_evidence"]["verification"], "indexed_observation_only")

    def test_index_link_identity_cannot_be_replaced_with_same_named_manifest(self):
        self.preparation()
        atomic_json(self.manifest_path, self.manifest)
        index = self.directory / "index.json"
        atomic_json(index, {"support_evidence": {"manifest": file_identity(self.manifest_path)}})
        self.manifest_path.write_text("{}\n")
        with patch.object(identity, "observe_git_source", return_value=self.source):
            result = capabilities.selection(ASSEMBLY, ROOT, results_index=index,
                                           record_pointer="", verify_record=True)
        self.assertEqual(result["staged_evidence"]["manifest"]["status"], "corrupt")
        self.assertEqual(result["validation"]["hil_tooling"], "not_assessed")

    def test_dev_support_has_human_and_json_views_and_runs_no_commands(self):
        before = ASSEMBLY.read_bytes()
        for flags in ([], ["--json"]):
            with self.subTest(flags=flags), patch.object(dev, "execute",
                    side_effect=AssertionError("support executed a command")), \
                    patch.object(process, "execution_environment",
                    side_effect=AssertionError("support initialized build environment")):
                output = io.StringIO()
                with redirect_stdout(output), redirect_stderr(io.StringIO()):
                    code = dev.main(["support", "--assembly", str(ASSEMBLY), *flags])
                self.assertEqual(code, 0)
                if flags:
                    result = json.loads(output.getvalue())
                    self.assertIn("query_environment", result)
                    self.assertEqual(result["query_environment"]["operation"], "read_only")
                    self.assertFalse(result["physical_qualified"])
                else:
                    self.assertIn("STM32F407ZGT6", output.getvalue())
                    self.assertIn("Next missing evidence:", output.getvalue())
        self.assertEqual(ASSEMBLY.read_bytes(), before)

    def test_cli_query_is_a_real_read_only_subprocess(self):
        environment = dict(os.environ)
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        result = subprocess.run([sys.executable, "-B", str(ROOT / "tools/dev/dev.py"),
            "support", "--assembly", str(ASSEMBLY), "--json"], cwd=ROOT,
            env=environment, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["kind"], "selected_support")
        self.assertEqual(report["validation"]["software_candidate"], "not_assessed")

    def test_original_failed_checker_remains_failed_after_summary_is_relabelled(self):
        report_path = self.directory / "failed-preparation.json"
        argv = [sys.executable, "-B", str(ROOT / "tools/hil/prepare.py"),
            "--fixture", str(self.directory / "absent-fixture.json"),
            "--resolved", str(self.config), "--report", str(report_path)]
        log = self.directory / "failed.txt"
        executed = process.run(argv, cwd=ROOT, env=process.execution_environment(os.environ),
                               log=log, stage="support-failed-checker")
        self.assertNotEqual(executed.returncode, 0)
        report = {"schema_version": 1, "kind": "software_qualification", "scope": "hil_tooling",
            "status": "failed", "source": self.source,
            "configuration_sha256": self.resolved["configuration_sha256"],
            "elf_sha256": self.artifacts["elf"]["sha256"],
            "executed_checks": [{"name": "preparation", "status": "failed",
                "exit_code": executed.returncode, "argv": argv, "raw": file_identity(log)}],
            "evidence": [file_identity(report_path)]}
        qualification = self.directory / "failed-qualification.json"
        atomic_json(qualification, report)
        self.manifest["qualifications"] = [file_identity(qualification)]
        failed = self.query()
        self.assertEqual(failed["staged_evidence"]["hil_tooling"]["status"], "failed_execution")
        self.assertEqual(failed["staged_evidence"]["hil_tooling"]["trust"],
                         "original_execution_verified")
        report["status"] = report["executed_checks"][0]["status"] = "passed"
        report["executed_checks"][0]["exit_code"] = 0
        atomic_json(qualification, report)
        self.manifest["qualifications"] = [file_identity(qualification)]
        forged = self.query()
        self.assertEqual(forged["staged_evidence"]["hil_tooling"]["status"], "corrupt")
        self.assertEqual(forged["validation"]["physical_hil"], "not_assessed")

    def test_sdk_selection_requires_canonical_policy_and_named_entry(self):
        index = self.directory / "sdk-index.json"
        report = self.directory / "consumer.json"
        atomic_json(report, {"kind": "source_sdk_consumer_verification", "status": "passed"})
        atomic_json(index, {"support_evidence": {"sdk_consumer": {
            "report": file_identity(report), "consumer_index": 1,
            "consumer_id": "incorrect", "plan_sha256": "a" * 64}}})
        with patch.object(identity, "observe_git_source", return_value=self.source):
            result = capabilities.selection(ASSEMBLY, ROOT, results_index=index,
                                           record_pointer="", verify_record=True)
        self.assertEqual(result["staged_evidence"]["sdk_consumer"]["status"], "corrupt")
        self.assertEqual(result["validation"]["installed_sdk"], "not_assessed")

    def test_linked_legacy_candidate_is_only_read_with_explicit_legacy_flag(self):
        path = self.directory / "candidate.json"
        atomic_json(path, {"schema_version": 1, "kind": "sealed_software_candidate"})
        index = self.directory / "legacy-index.json"
        atomic_json(index, {"support_evidence": {"candidate": file_identity(path)}})
        reference = {"source": self.source, "resolved": self.resolved,
            "configuration_sha256": self.resolved["configuration_sha256"],
            "qualified_scopes": sorted(candidate.REQUIRED_SCOPES),
            "sealed_schema_version": 1, "verification": "legacy_schema1_read_only"}
        with patch.object(identity, "observe_git_source", return_value=self.source), \
                patch.object(candidate, "verified_projection", return_value=reference) as verifier:
            result = capabilities.selection(ASSEMBLY, ROOT, results_index=index,
                record_pointer="", verify_record=True, legacy_replay=True)
        self.assertEqual(verifier.call_args.kwargs, {"legacy_replay": True})
        self.assertEqual(result["staged_evidence"]["software_candidate"]["status"],
                         "legacy_read_only")
        for name in ("host_models", "arm_image", "installed_sdk", "software_candidate"):
            self.assertEqual(result["validation"][name], "not_assessed")

    def test_reference_fixture_cannot_be_relabelled_as_physical_execution(self):
        fixture = ROOT / "tools/hil/fixtures/stm32h743_hsi_selected_reference.json"
        unused = self.directory / "station.json"
        atomic_json(unused, {})
        admission = self.directory / "admission.json"
        atomic_json(admission, {"kind": "hil_admission", "status": "ready",
            "physical_status": "passed", "station": file_identity(unused),
            "fixture": file_identity(fixture), "elf": self.artifacts["elf"],
            "resolved": file_identity(self.config), "resources": file_identity(unused)})
        result = self.query(hil_admission_path=admission)
        self.assertEqual(result["staged_evidence"]["hil_admission"]["status"], "corrupt")
        self.assertIn("software-reference", result["staged_evidence"]["hil_admission"]["reason"])
        self.assertFalse(result["physical_qualified"])

    def test_duplicate_scope_cannot_leave_an_earlier_passing_validation(self):
        self.preparation()
        self.manifest["qualifications"] *= 2
        result = self.query()
        self.assertEqual(result["staged_evidence"]["hil_tooling"]["status"], "corrupt")
        self.assertEqual(result["validation"]["hil_tooling"], "not_assessed")

    def test_real_arm_budget_replays_only_its_scope_and_rejects_rehashed_metrics(self):
        elf, resolved_path, budget = link_fixture(self.directory)
        resolved = json.loads(resolved_path.read_text())
        resources = measure(elf, resolved_path, budget)
        resource_path = self.directory / "resource-proof.json"
        atomic_json(resource_path, resources)
        binary = self.directory / "image.bin"
        window = image_window(resolved)
        binary.write_bytes(binary_from_elf(Elf32(elf.read_bytes(), float_abi="hard"),
                           window["origin"], window["origin"] + window["size"]))
        raw = self.directory / "resource-check.txt"
        raw.write_text("Resource checker result is replayed from the actual ARM ELF\n")
        qualification_path = self.directory / "resource-qualification.json"

        def query():
            proof = create_qualification("resource_budget", self.source, resolved_path, elf,
                [{"name": "resources", "exit_code": 0, "raw": file_identity(raw)}],
                evidence=[file_identity(resource_path)])
            atomic_json(qualification_path, proof)
            self.manifest.update(configuration=file_identity(resolved_path),
                artifacts={"elf": file_identity(elf), "bin": file_identity(binary),
                           "map": file_identity(self.directory / "image.map")},
                qualifications=[file_identity(qualification_path)])
            atomic_json(self.manifest_path, self.manifest)
            result = {"validation": {name: "not_assessed" for name in
                ("host_models", "arm_image", "installed_sdk", "software_candidate",
                 "physical_hil", "hil_tooling", "resource_budget", "reproducibility")},
                "physical_qualified": False}
            support.associate(result, resolved, self.source, {"manifest": self.manifest_path})
            return result

        result = query()
        self.assertEqual(result["staged_evidence"]["resource_budget"]["status"], "verified")
        self.assertEqual(result["validation"]["resource_budget"], "passed")
        self.assertEqual(result["validation"]["arm_image"], "not_assessed")
        resources["metrics"]["ram"]["sram"]["reserved_bytes"] -= 1
        atomic_json(resource_path, resources)
        forged = query()
        self.assertEqual(forged["staged_evidence"]["resource_budget"]["status"], "corrupt")
        self.assertEqual(forged["validation"]["resource_budget"], "not_assessed")

    def test_development_package_cannot_promote_formal_sdk_acceptance(self):
        sdk = self.directory / "development-sdk/share/nexus/src"
        sdk.mkdir(parents=True)
        package = {"kind": "nexus-installed-source-sdk", "publishable": False,
            "source_revision": self.source["commit"], "source_tree": self.source["tree"],
            "source_dirty": False}
        sdk_manifest = sdk / ".nexus-source-sdk.json"
        atomic_json(sdk_manifest, package)
        report = self.directory / "complete-consumers.json"
        atomic_json(report, {"kind": "source_sdk_consumer_verification", "status": "passed"})
        # Isolate publication admission from package-byte and matrix replay,
        # whose real execution paths have separate focused coverage.
        with patch.object(support, "verify_sdk_package", return_value=package), \
                patch.object(support.proof, "validate_sdk_consumers", return_value=[]):
            result = self.query(source_sdk_path=sdk_manifest, sdk_consumers_path=report)
        self.assertEqual(result["validation"]["installed_sdk"], "not_assessed")
        self.assertEqual(result["validation"]["software_candidate"], "not_assessed")
        self.assertEqual(result["staged_evidence"]["sdk_package"]["status"],
                         "verified_development_package")
        self.assertFalse(result["staged_evidence"]["sdk_package"]["publishable"])
        self.assertEqual(result["staged_evidence"]["sdk_consumers"]["status"],
                         "verified_bounded")

    def test_manifest_only_query_displays_the_actual_observed_current_source(self):
        result = self.query()
        self.assertEqual(result["current_source"], self.source)

    def test_complete_native_scope_preserves_tool_and_plain_file_observations(self):
        # This executes a real CMake/C/CTest snapshot fixture. Its source scope
        # is the isolated protocol model, never platform acceptance evidence.
        native, _, _ = scope_fixtures.ScopeExecutionTests().native_fixture(self.directory)
        source = {**self.source, "root": str(self.directory)}
        report = create_qualification("host_contracts", source, self.config,
            Path(self.artifacts["elf"]["path"]), native["executed_checks"],
            evidence=native["evidence"])
        report_path = self.directory / "native-qualification.json"
        atomic_json(report_path, report)
        self.manifest.update(source=source,
                             qualifications=[file_identity(report_path)])
        atomic_json(self.manifest_path, self.manifest)
        result = capabilities.selection(ASSEMBLY, ROOT)
        query = support.Query(result, self.resolved, source)
        references = query.load_manifest(self.manifest_path)
        for index, reference in enumerate(references):
            query.qualification(reference, index)
        query.finish()
        self.assertEqual(result["staged_evidence"]["host_contracts"]["status"],
                         "verified")
        self.assertEqual(result["validation"]["host_models"], "passed")
        ninja = next(item for item in result["tool_observations"]
                     if item["tool_kind"] == "ninja")
        self.assertEqual(ninja["observation"], "scope_admission")
        self.assertTrue(ninja["invoked"])
        self.assertIn({key: ninja[key] for key in ("path", "sha256", "size")},
                      list(query.inputs.values()))
        for key, value in (("sha256", "f" * 64), ("size", ninja["size"] + 1)):
            with self.subTest(field=key), self.assertRaisesRegex(
                    EvidenceError, "changed between scope observations"):
                query.remember({**ninja, key: value})

    def test_multiple_empty_file_roles_cannot_bypass_nonempty_raw_admission(self):
        empty = self.directory / "empty.txt"
        empty.write_bytes(b"")
        sdk = support.proof.sdk_file_identity(empty)
        stream = {**sdk, "role": "execution_stream"}
        result = capabilities.selection(ASSEMBLY, ROOT)
        query = support.Query(result, self.resolved, self.source)
        query.remember(sdk)
        query.remember(stream)
        query.finish()
        plain = {key: sdk[key] for key in ("path", "sha256", "size")}
        query.remember(plain)
        with self.assertRaisesRegex(EvidenceError, "invalid artifact size"):
            query.finish()
        atomic_json(self.manifest_path, self.manifest)
        for role in (sdk, stream):
            report = {"schema_version": 1, "kind": "software_qualification",
                "scope": "host_contracts", "status": "passed",
                "source": self.source,
                "configuration_sha256": self.resolved["configuration_sha256"],
                "elf_sha256": self.artifacts["elf"]["sha256"],
                "executed_checks": [{"name": "empty", "status": "passed",
                                     "exit_code": 0, "raw": role}]}
            report_path = self.directory / "empty-qualification.json"
            atomic_json(report_path, report)
            fresh = support.Query(capabilities.selection(ASSEMBLY, ROOT),
                                  self.resolved, self.source)
            fresh.load_manifest(self.manifest_path)
            fresh.qualification(file_identity(report_path), 0)
            self.assertEqual(fresh.rows["host_contracts"]["status"], "corrupt")
            self.assertIn("empty execution raw", fresh.rows["host_contracts"]["reason"])


if __name__ == "__main__":
    unittest.main()
