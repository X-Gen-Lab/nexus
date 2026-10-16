"""Consumers bind actual ARM ABI to canonical IR, never autodetect ELF flags."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence import candidate, reproduce
from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
                                   load_json, verify_file_identity)
from tools.evidence.identity import canonical_digest
from tools.evidence.proof import validate_resources
from tools.evidence.qualification import create_qualification
from tools.measurement import compare, interfaces
from tools.measurement.elf import Elf32
from tools.measurement.resources import measure_data
from tests.contracts.evidence_abi_support import build_layout, link_abi_fixture
from tests.contracts import test_evidence_candidate as candidate_fixtures
from tests.contracts.test_interface_cost import image


class ConsumerAbiTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def candidate_fixture(self, declared, **kwargs):
        fixture = candidate_fixtures.CandidateGateTests(methodName="runTest")
        fixture.setUp()
        self.addCleanup(fixture.directory.cleanup)
        manifest, _, _ = fixture.prepared()
        elf, resolved, _ = link_abi_fixture(fixture.root, declared, **kwargs)
        manifest["configuration"] = file_identity(resolved)
        manifest["artifacts"] = {kind: file_identity(fixture.root / ("image." + kind))
                                 for kind in ("elf", "bin", "map")}
        manifest["qualifications"] = []
        for scope in sorted(candidate.REQUIRED_SCOPES):
            path = fixture.root / (scope + ".json")
            atomic_json(path, create_qualification(scope, fixture.source, resolved,
                                                    elf, fixture.checks))
            manifest["qualifications"].append(file_identity(path))
        return fixture, manifest

    def test_candidates_seal_and_verify_all_three_declared_cpu_abis(self):
        for abi in ("hard", "soft", "softfp"):
            with self.subTest(abi=abi):
                fixture, manifest = self.candidate_fixture(abi)
                path = fixture.root / "manifest.json"
                atomic_json(path, manifest)
                # External OCI/scope trust is isolated; all linked-byte and IR
                # gates execute. These model packages are not qualifications.
                with patch.object(candidate, "verify_environment", return_value={}), \
                        patch.object(candidate, "validate_scope_proof", return_value=([], None)):
                    candidate.seal(path, fixture.root / "sealed")
                    candidate.verify(fixture.root / "sealed/candidate.json")

    def test_candidate_rejects_both_abi_mismatch_directions(self):
        for declared, linked in (("soft", "hard"), ("hard", "soft"),
                                 ("softfp", "hard"), ("hard", "softfp")):
            with self.subTest(declared=declared):
                _, manifest = self.candidate_fixture(declared, linked=linked)
                with patch.object(candidate, "verify_environment", return_value={}):
                    with self.assertRaisesRegex(EvidenceError, "ABI required"):
                        candidate.validate_manifest(manifest)

    def test_candidate_rejects_malformed_present_cpu(self):
        for cpu in (None, {}, {"float_abi": "auto"}, {"float_abi": 1}):
            with self.subTest(cpu=cpu):
                _, manifest = self.candidate_fixture("hard", cpu=cpu)
                with patch.object(candidate, "verify_environment", return_value={}):
                    with self.assertRaisesRegex(EvidenceError, "resolved CPU"):
                        candidate.validate_manifest(manifest)

    def test_candidate_requires_actual_linked_configuration_identity(self):
        _, manifest = self.candidate_fixture("hard")
        path = Path(manifest["configuration"]["path"])
        resolved = load_json(path)
        resolved["board"] = "different-but-canonical"
        resolved.pop("configuration_sha256")
        resolved["configuration_sha256"] = canonical_digest(resolved)
        atomic_json(path, resolved)
        manifest["configuration"] = file_identity(path)
        with patch.object(candidate, "verify_environment", return_value={}):
            with self.assertRaisesRegex(EvidenceError, "ELF does not bind"):
                candidate.validate_manifest(manifest)

    def resource_fixture(self, declared, **kwargs):
        elf, resolved_path, budget = link_abi_fixture(self.root, declared, **kwargs)
        resolved = load_json(resolved_path)
        producer = kwargs.get("linked", declared)
        elf_abi = "hard" if producer == "hard" else "soft"
        metrics, violations = measure_data(Elf32(elf.read_bytes(), float_abi=elf_abi),
                                           resolved, load_json(budget))
        evidence = {"kind": "elf_resource_budget", "status": "passed",
            "physical_status": "not_executed", "elf": file_identity(elf),
            "resolved": file_identity(resolved_path), "budget": file_identity(budget),
            "metrics": metrics, "violations": violations,
            "configuration_inputs": [{"label": "image.c",
                                       "file": file_identity(self.root / "image.c")}]}
        path = self.root / "resources.json"
        atomic_json(path, evidence)
        return ({"evidence": [file_identity(path)]},
                {"configuration": file_identity(resolved_path),
                 "artifacts": {"elf": file_identity(elf)}}, resolved)

    def test_resource_scope_rechecks_all_three_declared_cpu_abis(self):
        for abi in ("hard", "soft", "softfp"):
            with self.subTest(abi=abi):
                validate_resources(*self.resource_fixture(abi), verify_file_identity)

    def test_resource_scope_rejects_both_abi_mismatches_and_malformed_cpu(self):
        for declared, linked, cpu in (("soft", "hard", {"float_abi": "soft"}),
                                     ("hard", "soft", {"float_abi": "hard"}),
                                     ("softfp", "hard", {"float_abi": "softfp"}),
                                     ("hard", "softfp", {"float_abi": "hard"}),
                                     ("hard", "hard", None)):
            with self.subTest(declared=declared, linked=linked, cpu=cpu):
                args = self.resource_fixture(declared, linked=linked, cpu=cpu)
                with self.assertRaisesRegex(EvidenceError, "ABI required|resolved CPU"):
                    validate_resources(*args, verify_file_identity)

    def test_reproduction_validates_all_three_declared_cpu_abis(self):
        for abi in ("hard", "soft", "softfp"):
            with self.subTest(abi=abi):
                root = self.root / abi
                elf, _, _ = build_layout(root, abi)
                artifacts = {"elf": file_identity(elf), "bin": file_identity(elf.with_suffix(".bin"))}
                reproduce.validate_build_artifacts(root, artifacts)

    def test_reproduction_rejects_both_abi_mismatches_and_malformed_cpu(self):
        for index, (declared, linked, cpu) in enumerate((
                ("soft", "hard", {"float_abi": "soft"}),
                ("hard", "soft", {"float_abi": "hard"}),
                ("softfp", "hard", {"float_abi": "softfp"}),
                ("hard", "softfp", {"float_abi": "hard"}),
                ("hard", "hard", {}))):
            with self.subTest(declared=declared, linked=linked):
                root = self.root / str(index)
                elf, _, _ = build_layout(root, declared, linked=linked, cpu=cpu)
                artifacts = {"elf": file_identity(elf), "bin": file_identity(elf.with_suffix(".bin"))}
                with self.assertRaisesRegex(EvidenceError, "ABI required|resolved CPU"):
                    reproduce.validate_build_artifacts(root, artifacts)

    def test_historical_absent_cpu_remains_hard_only(self):
        root = self.root / "hard"
        elf, _, _ = build_layout(root, "hard", cpu="absent")
        reproduce.validate_build_artifacts(root, {"elf": file_identity(elf),
            "bin": file_identity(elf.with_suffix(".bin"))})
        for abi in ("soft", "softfp"):
            with self.subTest(abi=abi):
                root = self.root / abi
                elf, _, _ = build_layout(root, abi, cpu="absent")
                with self.assertRaisesRegex(EvidenceError, "hard-float ABI required"):
                    reproduce.validate_build_artifacts(root,
                        {"elf": file_identity(elf),
                         "bin": file_identity(elf.with_suffix(".bin"))})

    def interface_cli(self, elf, resolved=None, output=None):
        argv = ["--elf", str(elf), "--output", str(output or self.root / "interface.json")]
        if resolved is not None:
            argv += ["--resolved", str(resolved)]
        stdout, stderr = io.StringIO(), io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            try:
                code = interfaces.main(argv)
            except SystemExit as error:
                code = error.code
        return code, stderr.getvalue()

    def test_interface_cli_binds_all_three_abis_to_canonical_cpu(self):
        for abi in ("hard", "soft", "softfp"):
            with self.subTest(abi=abi):
                elf, resolved, _ = link_abi_fixture(self.root, abi)
                code, error = self.interface_cli(elf, resolved)
                self.assertEqual(code, 0, error)

    def test_interface_cli_rejects_both_abi_mismatches_and_malformed_cpu(self):
        for declared, linked, cpu in (("soft", "hard", {"float_abi": "soft"}),
                                     ("hard", "soft", {"float_abi": "hard"}),
                                     ("softfp", "hard", {"float_abi": "softfp"}),
                                     ("hard", "softfp", {"float_abi": "hard"}),
                                     ("hard", "hard", None)):
            with self.subTest(declared=declared, linked=linked):
                elf, resolved, _ = link_abi_fixture(self.root, declared, linked=linked, cpu=cpu)
                code, error = self.interface_cli(elf, resolved)
                self.assertEqual(code, 1, error)
                self.assertRegex(error, "ABI required|resolved CPU")
                self.assertFalse((self.root / "interface.json").exists())

    def test_interface_cli_legacy_without_resolved_is_hard_only(self):
        for abi, expected in (("hard", 0), ("soft", 1), ("softfp", 1)):
            with self.subTest(abi=abi):
                elf, _, _ = link_abi_fixture(self.root, abi)
                code, error = self.interface_cli(elf)
                self.assertEqual(code, expected, error)

    def test_interface_output_cannot_overwrite_input_or_declared_source(self):
        elf, resolved, _ = link_abi_fixture(self.root, "soft")
        for path in (elf, resolved, self.root / "input_paths.json", self.root / "image.c"):
            with self.subTest(path=path.name):
                before = path.read_bytes()
                code, error = self.interface_cli(elf, resolved, path)
                self.assertEqual(code, 1, error)
                self.assertEqual(path.read_bytes(), before)

    def test_family_operations_tables_are_measured_and_writable_tables_rejected(self):
        for family in ("stm32f103", "stm32g474", "stm32h743", "stm32u575", "gd32f303"):
            with self.subTest(family=family):
                objects = [("nx_" + family + "_uart_ops", 32)]
                result = interfaces.inspect_interfaces(image(objects))
                self.assertEqual(result["operations_flash_bytes"], 32)
                with self.assertRaisesRegex(EvidenceError, "read-only"):
                    interfaces.inspect_interfaces(image(objects, True))

    def matrix_fixture(self, declared, *, extra_source="", **kwargs):
        source = self.root / ("source-" + declared)
        source.mkdir(exist_ok=True)
        subprocess.run(["git", "init", "-q", str(source)], check=True)
        (source / "tracked.txt").write_text("ABI consumer fixture\n")
        subprocess.run(["git", "-C", str(source), "add", "."], check=True)
        subprocess.run(["git", "-C", str(source), "-c", "user.name=Fixture",
            "-c", "user.email=fixture@example.invalid", "commit", "-qm", "fixture"], check=True)
        assembly = self.root / "fixture.json"
        atomic_json(assembly, {"board_package": "board.json"})
        atomic_json(self.root / "fixture-empty.json", {"board_package": "board.json"})
        def build_boundary(argv, log):
            if "-B" in argv:
                build_layout(Path(argv[argv.index("-B") + 1]), declared,
                             extra_source=extra_source, **kwargs)
            # Replace only external build orchestration; ELF, canonical IR,
            # resource and interface gates all consume actual ARM bytes.
            return reproduce.run_logged([sys.executable, "-c", "print('ARM fixture linked')"], log)
        with patch.object(compare, "run_logged", side_effect=build_boundary):
            return compare.compare(source, assembly, self.root / ("matrix-" + declared),
                workloads=("empty",), optimizations=("Os",))

    def test_compare_reparses_actual_elf_with_resolved_abi(self):
        for abi in ("hard", "soft", "softfp"):
            with self.subTest(abi=abi):
                report = self.matrix_fixture(abi)
                self.assertEqual(report["status"], "passed", report.get("failure"))
                self.assertGreater(report["images"][0]["resources"]["size"], 0)

    def test_empty_image_rejects_a_retained_family_named_provider(self):
        report = self.matrix_fixture("soft", extra_source=
            "\nvoid nx_stm32f103_gpio_probe(void) {}\n")
        self.assertEqual(report["status"], "failed")
        proof = report["images"][0]["unreferenced_device_gc"]
        self.assertEqual(proof["active_device_provider_symbols"], ["nx_stm32f103_gpio_probe"])

    def test_compare_retains_failed_report_for_malformed_present_cpu(self):
        report = self.matrix_fixture("hard", cpu=None)
        self.assertEqual(report["status"], "failed")
        self.assertIn("resolved CPU", report["images"][0]["failure"])
        self.assertEqual(load_json(self.root / "matrix-hard/matrix.json"), report)

    def test_interface_rejects_stale_inputs_and_unbound_canonical_config(self):
        elf, resolved_path, _ = link_abi_fixture(self.root, "soft")
        original = load_json(resolved_path)
        original_source = (self.root / "image.c").read_text()
        (self.root / "image.c").write_text("changed input after link")
        code, error = self.interface_cli(elf, resolved_path)
        self.assertEqual(code, 1)
        self.assertIn("input changed", error)
        (self.root / "image.c").write_text(original_source)
        original["board"] = "changed-canonical-board"
        original.pop("configuration_sha256")
        original["configuration_sha256"] = canonical_digest(original)
        atomic_json(resolved_path, original)
        code, error = self.interface_cli(elf, resolved_path)
        self.assertEqual(code, 1)
        self.assertIn("ELF does not bind", error)

    def test_reproduction_rejects_unbound_canonical_configuration_and_bin(self):
        elf, resolved, _ = build_layout(self.root, "soft")
        binary = elf.with_suffix(".bin")
        artifacts = {"elf": file_identity(elf), "bin": file_identity(binary)}
        contents = binary.read_bytes()
        binary.write_bytes(b"not the linked binary")
        with self.assertRaisesRegex(EvidenceError, "BIN bytes differ"):
            reproduce.validate_build_artifacts(self.root, artifacts)
        binary.write_bytes(contents)
        config = load_json(resolved)
        config["board"] = "different-canonical-config"
        config.pop("configuration_sha256")
        config["configuration_sha256"] = canonical_digest(config)
        atomic_json(resolved, config)
        with self.assertRaisesRegex(EvidenceError, "ELF does not bind"):
            reproduce.validate_build_artifacts(self.root, artifacts)


if __name__ == "__main__":
    unittest.main()
