"""A real external Git project is a separate, closed reproduction input."""

import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError
from tools.evidence import consumer
from tools.evidence import reproduce
from tools.evidence.common import atomic_json, command, file_identity, verify_file_identity
from tools.evidence.inputs import tree_identity
from tools.evidence.inputs import records_identity
from tools.evidence import proof


class ConsumerInputTests(unittest.TestCase):
    def test_snapshot_digest_preserves_the_existing_path_component_order(self):
        records = [
            {"path": "input/child", "size": 1, "sha256": "a" * 64},
            {"path": "input.json", "size": 1, "sha256": "b" * 64},
        ]
        # Historical Source SDK/toolchain snapshots sort Path components. A
        # string sort would put input.json first and invalidate unchanged bytes.
        import hashlib
        encoded = json.dumps(records, sort_keys=True, separators=(",", ":")).encode()
        self.assertEqual(records_identity(list(reversed(records)))["sha256"],
                         hashlib.sha256(encoded).hexdigest())

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "external"
        self.source.mkdir()
        self.git("init", "--quiet")
        self.project = self.source / "firmware"
        self.project.mkdir()
        (self.project / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.31)\nproject(consumer C)\n")
        (self.project / "assembly.toml").write_text('schema = 2\nboard = "external"\n')
        (self.project / "main.c").write_text("int main(void) { return 0; }\n")
        (self.source / "other-project.c").write_text("excluded repository file\n")
        self.git("add", ".")
        self.git("-c", "user.name=Contract", "-c", "user.email=contract@example.invalid",
                 "commit", "--quiet", "-m", "initial input")

    def git(self, *arguments):
        return subprocess.run(["git", "-C", str(self.source), *arguments],
                              check=True, capture_output=True, text=True).stdout.strip()

    def test_snapshot_copies_only_the_declared_tracked_project(self):
        identity = consumer.identify(self.source, "firmware")
        target = self.root / "copied"
        consumer.copy(identity, target)
        self.assertEqual((target / "main.c").read_bytes(),
                         (self.project / "main.c").read_bytes())
        self.assertEqual(sorted(path.name for path in target.iterdir()),
                         ["CMakeLists.txt", "assembly.toml", "main.c"])
        self.assertNotIn(".git", {path.name for path in target.rglob("*")})
        self.assertEqual(identity, json.loads(json.dumps(identity)))
        consumer.verify(identity)

    def test_untracked_or_changed_inputs_cannot_become_formal_inputs(self):
        identity = consumer.identify(self.source, "firmware")
        for path in (self.project / "main.c", self.source / "untracked.txt"):
            with self.subTest(path=path.name):
                old = path.read_bytes() if path.exists() else None
                path.write_text("changed or undeclared input\n")
                with self.assertRaises(EvidenceError):
                    consumer.verify(identity)
                if old is None:
                    path.unlink()
                else:
                    path.write_bytes(old)

    def test_stale_commit_and_snapshot_identity_are_rejected(self):
        identity = consumer.identify(self.source, "firmware")
        for field in ("commit", "tree"):
            changed = json.loads(json.dumps(identity))
            changed["source"][field] = "a" * 40
            with self.assertRaises(EvidenceError):
                consumer.verify(changed)
        changed = json.loads(json.dumps(identity))
        changed["snapshot"]["sha256"] = "a" * 64
        with self.assertRaises(EvidenceError):
            consumer.copy(changed, self.root / "rejected")
        self.assertFalse((self.root / "rejected").exists())

    def test_parent_project_symlink_and_traversal_are_rejected(self):
        for name in ("../external/firmware", "/firmware", "firmware/../firmware",
                     "firmware\\main.c", "", "firmware,hidden"):
            with self.subTest(name=name), self.assertRaises(EvidenceError):
                consumer.identify(self.source, name)
        (self.source / "redirect").symlink_to(self.project, target_is_directory=True)
        self.git("add", "redirect")
        self.git("-c", "user.name=Contract", "-c", "user.email=contract@example.invalid",
                 "commit", "--quiet", "-m", "symlink input")
        with self.assertRaises(EvidenceError):
            consumer.identify(self.source, "redirect")

    def test_tracked_symlink_cannot_read_outside_the_project(self):
        (self.project / "leak").symlink_to(self.source / "other-project.c")
        self.git("add", "firmware/leak")
        self.git("-c", "user.name=Contract", "-c", "user.email=contract@example.invalid",
                 "commit", "--quiet", "-m", "symlink file")
        with self.assertRaises(EvidenceError):
            consumer.identify(self.source, "firmware")

    def test_git_status_hints_cannot_bind_modified_bytes_to_an_old_commit(self):
        original = (self.project / "main.c").read_bytes()
        for hint, clear in (("--assume-unchanged", "--no-assume-unchanged"),
                            ("--skip-worktree", "--no-skip-worktree")):
            with self.subTest(hint=hint):
                self.git("update-index", hint, "firmware/main.c")
                (self.project / "main.c").write_text("int uncommitted = 7;\n")
                self.assertEqual(self.git("status", "--porcelain"), "")
                try:
                    with self.assertRaisesRegex(EvidenceError, "committed bytes"):
                        consumer.identify(self.source, "firmware")
                finally:
                    (self.project / "main.c").write_bytes(original)
                    self.git("update-index", clear, "firmware/main.c")

    def test_archived_input_contract_rejects_dirty_and_malformed_source(self):
        identity = consumer.identify(self.source, "firmware")
        consumer.contract(identity)
        for key, value in (("dirty", True), ("dirty", 0), ("commit", "main"),
                           ("tree", "a" * 41), ("root", "relative/checkout")):
            changed = copy.deepcopy(identity)
            changed["source"][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(EvidenceError):
                consumer.contract(changed)

    def test_parent_git_environment_cannot_redirect_consumer_identity(self):
        identity = consumer.identify(self.source, "firmware")
        with patch.dict(os.environ, {"GIT_DIR": str(self.root / "missing.git"),
                                    "GIT_WORK_TREE": str(self.root)}):
            self.assertEqual(consumer.identify(self.source, "firmware"), identity)

    def test_submodule_is_not_an_implicit_external_dependency(self):
        commit = self.git("rev-parse", "HEAD")
        self.git("update-index", "--add", "--cacheinfo", "160000", commit,
                 "firmware/undeclared-sdk")
        self.git("-c", "user.name=Contract", "-c", "user.email=contract@example.invalid",
                 "commit", "--quiet", "-m", "undeclared submodule")
        with self.assertRaisesRegex(EvidenceError, "submodule"):
            consumer.identify(self.source, "firmware")

    def test_external_prepare_binds_both_sources_and_uses_the_installed_sdk(self):
        toolchain = self.root / "toolchain"
        toolchain.mkdir()
        (toolchain / "compiler").write_text("separately declared compiler fixture\n")
        sdk = self.root / ".nexus-source-sdk.json"
        atomic_json(sdk, {"kind": "unqualified provenance test fixture"})
        # Isolate preparation from the separately tested complete SDK verifier.
        # This test does not execute containers or issue qualification evidence.
        with patch.object(reproduce, "verify_source_sdk"):
            spec = reproduce.prepare_spec(
                self.source, self.project / "assembly.toml", self.root / "environment.json",
                toolchain, source_sdk=sdk, consumer_source=self.source,
                consumer_project="firmware")
            reproduce.validate_spec(spec)
        self.assertEqual(spec["schema_version"], 2)
        self.assertEqual(spec["consumer"], consumer.identify(self.source, "firmware"))
        self.assertEqual(spec["assembly"], "assembly.toml")
        self.assertEqual(spec["resolved"], "nexus-sdk/generated/resolved.json")
        self.assertIn("{consumer}", spec["commands"][0])
        self.assertIn("-DNexus_DIR={sdk}/lib/cmake/Nexus", spec["commands"][0])
        self.assertNotIn("-DNEXUS_WORKLOAD=empty", spec["commands"][0])

    def test_external_prepare_cannot_take_an_assembly_outside_its_project(self):
        toolchain = self.root / "toolchain"
        toolchain.mkdir()
        (toolchain / "compiler").write_text("fixture")
        sdk = self.root / ".nexus-source-sdk.json"
        atomic_json(sdk, {"kind": "unqualified fixture"})
        with patch.object(reproduce, "verify_source_sdk"), self.assertRaises(EvidenceError):
            reproduce.prepare_spec(
                self.source, self.source / "other-project.c", self.root / "env.json",
                toolchain, source_sdk=sdk, consumer_source=self.source,
                consumer_project="firmware")

    def proof_fixture(self):
        """Synthetic argv structure only; never issue software qualification.

        SDK trust and actual Docker execution have separate integration gates.
        These deliberately nonpublishable bytes isolate external-input checks.
        """
        selected = consumer.identify(self.source, "firmware")
        environment = self.root / "environment.json"
        image = "sha256:" + "a" * 64
        atomic_json(environment, {"image_config_id": image})
        sdk = self.root / "sdk-fixture.json"
        atomic_json(sdk, {"kind": "not_publishable_sdk_unit_fixture"})
        configuration = self.root / "configuration.json"
        atomic_json(configuration, {"kind": "not_qualified_configuration_fixture"})
        spec = {"schema_version": 2, "source_date_epoch": 0,
            "source": selected["source"],
            "consumer": selected, "source_sdk": file_identity(sdk),
            "toolchain": {"path": "/declared/unit-fixture-toolchain"},
            "commands": [["cmake", "-S", "{consumer}", "-B", "{build}",
                          "-DNexus_DIR={sdk}/lib/cmake/Nexus"],
                         ["cmake", "--build", "{build}"]],
            "assembly": "assembly.toml", "resolved": "nexus-sdk/generated/resolved.json",
            "artifacts": {"elf": "unit.elf", "bin": "unit.bin"}}
        spec_path = self.root / "spec.json"
        atomic_json(spec_path, spec)
        evidence = {"status": "passed", "source": selected["source"],
            "consumer": selected, "source_sdk": file_identity(sdk),
            "toolchain": spec["toolchain"], "spec": file_identity(spec_path),
            "environment": file_identity(environment), "physical_status": "not_executed",
            "hermetic_status": "passed_declared_container_boundary",
            "reproducible_status": "bit_exact_elf_bin", "different_artifacts": [], "runs": []}
        for index in range(2):
            directory = self.root / str(index)
            project = directory / "consumer"
            consumer.copy(selected, project)
            package = directory / "sdk"
            source = package / "share/nexus/src"
            source.mkdir(parents=True)
            (source / "fixture.txt").write_text("unqualified SDK structural fixture\n")
            build = directory / "build"
            build.mkdir()
            artifacts = {}
            for role, name in spec["artifacts"].items():
                (build / name).write_text("not an MCU image; argv unit fixture\n")
                artifacts[role] = file_identity(build / name)
            resolved = build / spec["resolved"]
            resolved.parent.mkdir(parents=True)
            resolved.write_bytes(configuration.read_bytes())
            run = {"source_snapshot": tree_identity(source),
                "source_package_snapshot": tree_identity(package),
                "source_directory": str(source), "package_directory": str(package),
                "output_directory": str(build), "source_mount": f"/sdk-{index}/share/nexus/src",
                "package_mount": f"/sdk-{index}", "output_mount": f"/build-{index}",
                "consumer_directory": str(project), "consumer_mount": f"/consumer-{index}",
                "consumer_snapshot": tree_identity(project),
                "consumer_files": {path.relative_to(project).as_posix(): proof.sdk_file_identity(path)
                    for path in project.rglob("*") if path.is_file()},
                "resolved": file_identity(resolved), "artifacts": artifacts,
                "container_user": "1000:1000", "status": "passed", "clean_output": True,
                "commands": []}
            for number, argv in enumerate(spec["commands"]):
                raw = directory / f"command-{number}.log"
                raw.write_text("synthetic argv fixture; Docker was not executed\n")
                actual = reproduce.container_argv(spec, run, argv, image,
                    container_name=f"nexus-repro-unit-{index}-{number}",
                    docker="unit-fixture-docker")
                run["commands"].append({"exit_code": 0, "failure": None,
                                        "argv": actual, "raw": file_identity(raw)})
            evidence["runs"].append(run)
        manifest = {"source": selected["source"], "environment": file_identity(environment),
            "configuration": file_identity(configuration),
            "artifacts": evidence["runs"][0]["artifacts"]}
        return evidence, manifest

    def test_scope_proof_rechecks_complete_external_source_and_mount_contract(self):
        evidence, manifest = self.proof_fixture()
        def resolver(identity):
            return (proof.verify_sdk_file(identity) if identity.get("role") == "sdk_file"
                    else verify_file_identity(identity))
        with patch.object(proof, "proof_document", return_value=(None, evidence)):
            # live=False isolates archived structural proof, not SDK admission.
            proof.validate_reproduction({}, manifest, {}, resolver, live=False)
        variants = []
        changed = copy.deepcopy(evidence)
        changed["consumer"]["source"]["commit"] = "b" * 40
        variants.append(changed)
        changed = copy.deepcopy(evidence)
        changed["runs"][1]["consumer_files"].pop("main.c")
        variants.append(changed)
        changed = copy.deepcopy(evidence)
        changed["runs"][1]["consumer_mount"] = changed["runs"][0]["consumer_mount"]
        variants.append(changed)
        changed = copy.deepcopy(evidence)
        changed["runs"][0]["resolved"]["sha256"] = "b" * 64
        variants.append(changed)
        changed = copy.deepcopy(evidence)
        changed["runs"][0]["commands"][0]["argv"].insert(1, "--mount")
        changed["runs"][0]["commands"][0]["argv"].insert(2, "type=bind,src=/undeclared,dst=/leak")
        variants.append(changed)
        for changed in variants:
            with self.subTest(change=variants.index(changed)), patch.object(
                    proof, "proof_document", return_value=(None, changed)):
                with self.assertRaises(EvidenceError):
                    proof.validate_reproduction({}, manifest, {}, resolver, live=False)


if __name__ == "__main__":
    unittest.main()
