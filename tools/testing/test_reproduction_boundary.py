"""The complete recorded Docker argv must match the producer's offline boundary."""
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from tools.evidence import proof, reproduce
from tools.evidence.common import (EvidenceError, atomic_json, command,
                                   file_identity, verify_file_identity)
from tools.evidence.container import docker_prefix
from tools.testing import test_reproduction_consumer as consumer_tests


def reference_argv(spec, run, expected, image, *, container_name,
                   docker="docker", legacy=False):
    """Retain the pre-refactor full producer contract as the test reference."""
    replacements = {"source": run["source_mount"], "build": run["output_mount"],
        "toolchain": "/toolchain", "sdk": run["package_mount"],
        "assembly": run["source_mount"] + "/" + spec["assembly"]}
    mounts = ["--mount", "type=bind,src=" + run["package_directory"] +
        ",dst=" + run["package_mount"] + ",readonly", "--mount",
        "type=bind,src=" + run["output_directory"] + ",dst=" + run["output_mount"],
        "--mount", "type=bind,src=" + spec["toolchain"]["path"] +
        ",dst=/toolchain,readonly"]
    if spec["schema_version"] == 2:
        replacements.update(consumer=run["consumer_mount"],
            assembly=run["consumer_mount"] + "/" + spec["assembly"])
        mounts += ["--mount", "type=bind,src=" + run["consumer_directory"] +
                   ",dst=" + run["consumer_mount"] + ",readonly"]
    return [*docker_prefix(docker), "run", "--rm", "--name", container_name,
        "--user", run["container_user"], "--network", "none", "--read-only",
        *([] if legacy else ["--log-driver", "none"]), "--cap-drop", "ALL",
        "--security-opt", "no-new-privileges", "--pids-limit", "256",
        "--tmpfs", "/tmp:rw,nosuid,nodev,size=256m", "--env", "HTTP_PROXY=",
        "--env", "HTTPS_PROXY=", "--env", "ALL_PROXY=", "--env", "http_proxy=",
        "--env", "https_proxy=", "--env", "all_proxy=", "--env",
        "SOURCE_DATE_EPOCH=" + str(spec["source_date_epoch"]), "--env", "LC_ALL=C",
        "--env", "TZ=UTC", *mounts, "--workdir", run["output_mount"], image,
        *command(expected, replacements)]


class ReproductionBoundaryTests(unittest.TestCase):
    def evidence(self, *, version=2, preflight=True, legacy=False):
        self.fixture = consumer_tests.ConsumerInputTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        evidence, manifest = self.fixture.proof_fixture()
        path = Path(evidence["spec"]["path"])
        spec = json.loads(path.read_text())
        spec["source_date_epoch"] = 1700000000
        spec["schema_version"] = version
        if version == 1:
            spec.pop("consumer")
            spec.pop("resolved")
            evidence.pop("consumer")
            spec["commands"][0] = ["cmake", "-S", "{source}", "-B", "{build}",
                                   "-DNEXUS_ASSEMBLY_FILE={assembly}"]
        if preflight:
            evidence["storage_preflight"] = {"kind": "synthetic_scheduling_fixture"}
        atomic_json(path, spec)
        evidence["spec"] = file_identity(path)
        for run_index, run in enumerate(evidence["runs"]):
            for index, (execution, expected) in enumerate(zip(
                    run["commands"], spec["commands"])):
                # Use the literal prior producer shape for RED. Once available,
                # the canonical builder must produce exactly these same bytes.
                execution["argv"] = reference_argv(spec, run, expected,
                    "sha256:" + "a" * 64,
                    container_name=f"nexus-repro-unit-{run_index}-{index}",
                    docker="unit-fixture-docker", legacy=legacy)
        return evidence, manifest, spec

    def validate(self, evidence, manifest):
        def resolver(identity):
            return (proof.verify_sdk_file(identity) if identity.get("role") == "sdk_file"
                    else verify_file_identity(identity))
        with patch.object(proof, "proof_document", return_value=(None, evidence)):
            # Structural archived evidence only; no SDK/hardware qualification.
            proof.validate_reproduction({}, manifest, {}, resolver, live=False)

    def test_current_full_boundary_and_exact_legacy_schema_one_are_accepted(self):
        for values in ({}, {"version": 1},
                       {"version": 1, "preflight": False, "legacy": True}):
            with self.subTest(values=values):
                evidence, manifest, _ = self.evidence(**values)
                self.validate(evidence, manifest)

    def test_shared_builder_retains_full_producer_argv(self):
        self.assertTrue(hasattr(reproduce, "container_argv"),
                        "producer and proof need one canonical Docker argv builder")
        evidence, _, spec = self.evidence()
        run = evidence["runs"][0]
        expected = run["commands"][0]["argv"]
        self.assertEqual(reproduce.container_argv(spec, run, spec["commands"][0],
            "sha256:" + "a" * 64, container_name="nexus-repro-unit-0-0",
            docker="unit-fixture-docker"), expected)

    def test_extra_alias_mount_privilege_entrypoint_and_device_are_rejected(self):
        for extra in (["-v", "/undeclared:/leak:ro"],
                ["--volume", "/undeclared:/leak:ro"], ["--privileged"],
                ["--entrypoint", "/bin/sh"], ["--device", "/dev/anything"],
                ["--mount=type=bind,src=/undeclared,dst=/leak"],
                ["--env", "ADDITIONAL_INPUT=/host"], ["--pid", "host"]):
            with self.subTest(extra=extra):
                evidence, manifest, _ = self.evidence()
                argv = evidence["runs"][0]["commands"][0]["argv"]
                index = argv.index("--read-only")
                argv[index:index] = extra
                with self.assertRaises(EvidenceError):
                    self.validate(evidence, manifest)

    def test_duplicate_network_user_and_environment_flags_are_rejected(self):
        for extra in (["--network", "host"], ["--network", "none"],
                ["--user", "0:0"], ["--env", "SOURCE_DATE_EPOCH=5"],
                ["--log-driver", "json-file"]):
            with self.subTest(extra=extra):
                evidence, manifest, _ = self.evidence()
                argv = evidence["runs"][0]["commands"][0]["argv"]
                index = argv.index("--read-only")
                argv[index:index] = extra
                with self.assertRaises(EvidenceError):
                    self.validate(evidence, manifest)

    def test_daemon_prefix_proxy_environment_and_working_directory_are_exact(self):
        mutations = [lambda argv: argv.__setitem__(12, "--host=tcp://unreviewed:2375"),
            lambda argv: argv.__setitem__(2, "UNRELATED_ENVIRONMENT"),
            lambda argv: argv.__setitem__(argv.index("HTTP_PROXY="), "HTTP_PROXY=http://unreviewed"),
            lambda argv: argv.__setitem__(argv.index("--workdir") + 1, "/other/output"),
            lambda argv: argv.__setitem__(argv.index("--tmpfs") + 1, "/tmp:rw,size=2g")]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                evidence, manifest, _ = self.evidence()
                mutate(evidence["runs"][0]["commands"][0]["argv"])
                with self.assertRaises(EvidenceError):
                    self.validate(evidence, manifest)

    def test_missing_log_driver_is_legacy_only_without_storage_preflight(self):
        for values in ({"legacy": True}, {"version": 1, "legacy": True},
                       {"preflight": False, "legacy": True}):
            with self.subTest(values=values):
                evidence, manifest, _ = self.evidence(**values)
                with self.assertRaises(EvidenceError):
                    self.validate(evidence, manifest)

    def test_malformed_short_argv_and_unbounded_container_name_fail_closed(self):
        for value in ([], ["docker"], ["env", "--name"], ["x"] * 200):
            with self.subTest(value=value):
                evidence, manifest, _ = self.evidence()
                evidence["runs"][0]["commands"][0]["argv"] = value
                with self.assertRaises(EvidenceError):
                    self.validate(evidence, manifest)
        evidence, manifest, _ = self.evidence()
        argv = evidence["runs"][0]["commands"][0]["argv"]
        argv[argv.index("--name") + 1] = "nexus-" + "a" * 130
        with self.assertRaises(EvidenceError):
            self.validate(evidence, manifest)

    def test_invalid_builder_identity_and_epoch_are_rejected(self):
        self.assertTrue(hasattr(reproduce, "container_argv"))
        evidence, _, spec = self.evidence()
        for changes in ({"container_name": "unreviewed"},
                {"container_name": "nexus-" + "x" * 130}, {"docker": "--host=other"}):
            with self.subTest(changes=changes), self.assertRaises(EvidenceError):
                reproduce.container_argv(spec, evidence["runs"][0], spec["commands"][0],
                    "sha256:" + "a" * 64, **({"container_name": "nexus-unit"} | changes))
        for epoch in (True, -1, 2**32):
            with self.subTest(epoch=epoch), self.assertRaises(EvidenceError):
                reproduce.container_argv(spec | {"source_date_epoch": epoch},
                    evidence["runs"][0], spec["commands"][0], "sha256:" + "a" * 64,
                    container_name="nexus-unit")


if __name__ == "__main__":
    unittest.main()
