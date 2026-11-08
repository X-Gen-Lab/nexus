"""Canonical SDK scope requires actual Nexus consumption and original execution."""
from tools.configure.tests.native_fixture import native_fixture

import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

from tools.configure import configure
from tools.dev import process
from tools.evidence import identity, proof
from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
                                   stream_digest, verify_file_identity)
from tools.maintenance import support
from tools.release.transport import consumer_plan


ROOT = Path(__file__).resolve().parents[2]
ASSEMBLY = ROOT / "tools/configure/assemblies/qiming-baremetal-uart.toml"


class InstalledConsumerSupportTests(unittest.TestCase):
    """Share one complete real SDK; each consumer has separate build outputs."""

    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="support installed SDK ")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.root = Path(cls.temporary.name)
        cls.prefix = cls.root / "complete SDK"
        result = process.run([sys.executable, "-B",
            str(ROOT / "cmake/package/package_source_sdk.py"),
            "--source", str(ROOT), "--output", str(cls.prefix),
            "--development-fixture", "--host-support", "--component", "mcuboot"], cwd=ROOT,
            env=process.execution_environment(os.environ),
            log=cls.root / "package.txt", timeout=180,
            stage="support-sdk-fixture")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        cls.sdk = cls.prefix / "share/nexus/src"
        cls.package = support.verify_sdk_package(cls.sdk)
        specification = importlib.util.spec_from_file_location(
            "support_sdk_consumer_producer",
            ROOT / "cmake/package/verify_consumers.py")
        cls.producer = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(cls.producer)

    def setUp(self):
        self.directory = self.root / self._testMethodName
        self.directory.mkdir()
        self.output = self.directory / "consumer output"
        self.output.mkdir()
        self.commands = []
        self.policy = support.consumer_policy()
        self.execute(0, [sys.executable, "-B",
            str(self.sdk / "cmake/package/package_source_sdk.py"),
            "--verify", str(self.sdk)])

    def execute(self, index, argv):
        phase = sum(row["consumer_index"] == index for row in self.commands)
        log = self.output / f"command-{index:02d}-{phase:02d}.log"
        result = process.run(argv, cwd=ROOT,
            env=process.execution_environment(os.environ), log=log,
            timeout=180, stage="source-sdk-consumer")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        receipt = json.loads(result.receipt_path.read_text())
        self.commands.append({"argv": argv, "consumer_index": index,
            "phase_index": phase, "log": log.name, "exit_code": 0,
            "receipt": str(result.receipt_path.relative_to(self.output)),
            **{key: receipt[key] for key in (
                "started_ns", "finished_ns", "duration_seconds", "outcome",
                "child_returncode", "withdrawal_complete")}})
        return result

    def query(self, index=1, observation=None):
        source = identity.observe_git_source(ROOT)
        resolved = configure.resolve_ir(ASSEMBLY, ROOT, test_support=native_fixture(ASSEMBLY)).to_dict()
        config = self.directory / "resolved.json"
        atomic_json(config, resolved)
        unused = self.directory / "unlinked.bin"
        unused.write_text("No candidate artifact qualification requested\n")
        manifest = self.directory / "manifest.json"
        atomic_json(manifest, {"source": source,
            "configuration": file_identity(config),
            "artifacts": {role: file_identity(unused)
                          for role in ("elf", "bin", "map")},
            "qualifications": []})
        rows = [{"consumer_index": row["consumer_index"],
                 "status": "not_started"}
                for row in self.policy["consumers"]]
        kind, specification = consumer_plan()[index - 1]
        name = specification.name if kind == "services" else specification[0]
        rows[index - 1].update(name=name, status="passed")
        if observation is not None:
            rows[index - 1]["service_image"] = observation
        report = self.output / "verification.json"
        atomic_json(report, {"kind": "source_sdk_consumer_verification",
            "status": "failed", "physical_status": "not_executed",
            "source_revision": self.package["source_revision"],
            "source_tree": self.package["source_tree"],
            "prefix": str(self.prefix), "consumers": rows,
            "commands": self.commands,
            "selected_images": [], "service_images": []})
        result = {"validation": {name: "not_assessed" for name in
            ("host_models", "arm_image", "installed_sdk", "software_candidate",
             "physical_hil", "hil_tooling", "resource_budget", "reproducibility")},
            "physical_qualified": False}
        selected = self.policy["consumers"][index - 1]
        support.associate(result, resolved, source, {
            "manifest": file_identity(manifest),
            "source_sdk": file_identity(self.sdk / ".nexus-source-sdk.json"),
            "sdk_consumer": {"report": file_identity(report),
                "consumer_index": index,
                "consumer_id": selected["consumer_id"],
                "plan_sha256": self.policy["plan_sha256"]}})
        atomic_json(self.directory / "support-result.json", result)
        return result

    def test_complete_receipts_without_nexus_consumption_are_rejected(self):
        source = self.output / "native source"
        build = self.output / "native build"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.31)\nproject(unrelated C CXX)\n"
            'set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")\n'
            "add_executable(installed_c main.c)\n"
            "add_executable(installed_cpp main.cpp)\n")
        for extension in ("c", "cpp"):
            (source / ("main." + extension)).write_text(
                "int main(void) { return 0; }\n")
        self.execute(1, ["git", "init", "--quiet", str(source)])
        self.execute(1, ["cmake", "-S", str(source), "-B", str(build),
            "-G", "Ninja", "-DNEXUS_EXPECTED_SOURCE_REVISION="
            + self.package["source_revision"],
            "-DNexus_DIR=" + str(self.prefix / "lib/cmake/Nexus")])
        self.execute(1, ["cmake", "--build", str(build), "--parallel", "1",
                         "--target", "installed_c", "installed_cpp"])
        for target in ("installed_c", "installed_cpp"):
            self.execute(1, [str(build / "bin" / target)])
        result = self.query()
        self.assertEqual(result["staged_evidence"]["sdk_consumer"]["status"],
                         "corrupt")
        self.assertEqual(result["validation"]["installed_sdk"], "not_assessed")

    def test_actual_nexus_consumer_is_bounded_when_its_matrix_failed(self):
        source, build, command = self.producer.prepare_consumer(
            self.prefix, self.output, "native",
            lambda argv: self.execute(1, argv), fixture=True,
            expected_revision=self.package["source_revision"])
        self.execute(1, command)
        self.execute(1, ["cmake", "--build", str(build), "--parallel", "1",
                         "--target", "installed_c", "installed_cpp"])
        for target in ("installed_c", "installed_cpp"):
            self.execute(1, [str(build / "bin" / target)])
        self.assertEqual((build / "resolved-sdk.txt").read_text(), str(self.sdk))
        generated = build / "nexus-sdk/generated"
        selected, _ = identity.resolved_identity(generated / "resolved.json")
        self.assertEqual(selected, configure.resolve_ir(
            self.sdk / "tests/support/native/assemblies/native.toml", self.sdk, test_support=native_fixture(self.sdk / "tests/support/native/assemblies/native.toml")).to_dict())
        self.assertTrue(identity.resolved_input_files(
            generated / "resolved.json", selected))
        self.assertTrue((generated / "nexus_factory_strong.h").is_file())
        entries = json.loads((build / "compile_commands.json").read_text())
        products = [entry for entry in entries
                    if Path(entry["file"]) in (source / "main.c", source / "main.cpp")]
        self.assertEqual(len(products), 2)
        for entry in products:
            self.assertFalse(self.producer.private_compile_inputs(entry, self.sdk))
        result = self.query()
        # Incomplete inventory cannot establish an exact whole-source relation,
        # even when this selected consumer's receipts are semantically valid.
        complete = all(row.startswith(" ") for row in
                       result["current_source"]["submodules"])
        expected = "verified_bounded" if complete else "historical_source"
        self.assertEqual(result["staged_evidence"]["sdk_consumer"]["status"],
                         expected)
        self.assertEqual(result["staged_evidence"]["sdk_consumer"]["trust"],
                         "semantic_verified")
        self.assertEqual(result["staged_evidence"]["sdk_consumer"]
                         ["recorded_matrix_status"], "failed")
        self.assertEqual(result["validation"]["installed_sdk"], "not_assessed")
        self.assertIn('#include "nexus_factory_strong.h"',
                      (source / "main.c").read_text())
        self.commands.pop()
        rejected = self.query()
        self.assertEqual(rejected["staged_evidence"]["sdk_consumer"]["status"],
                         "corrupt")

    def native_selected_observation(self, profile):
        index, specification = next((index, specification)
            for index, (kind, specification) in enumerate(consumer_plan(), 1)
            if kind == "selected" and specification[2] == "native-time-" + profile)
        source, build, command = self.producer.prepare_selected_consumer(
            self.prefix, self.output, *specification,
            lambda argv: self.execute(index, argv),
            expected_revision=self.package["source_revision"])
        # This helper regression uses a real development SDK. Full-plan
        # admission independently requires the canonical publishable command.
        self.execute(index, [*command, "-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON"])
        self.execute(index, ["cmake", "--build", str(build), "--parallel", "1",
                             "--target", "installed_selected"])
        observation = self.producer.check_selected_image(self.sdk, source, build,
            lambda argv: self.execute(index, argv),
            specification=specification)
        observation.update(name=specification[0],
                           assembly=str(self.sdk / specification[1]))
        inputs = proof.ScopeInputs({"evidence": [observation]},
                                   verify_file_identity, None)
        rows = [row for row in self.commands if row["consumer_index"] == index]
        def validate(record=observation, commands=rows):
            return proof.validate_consumer_observation(inputs, self.sdk,
                source, build, "selected", specification, record, commands,
                self.output)
        validate()
        self.assertEqual(observation["kind"], "native-time-host-observation")
        self.assertNotIn("runtime_executed", observation)
        self.assertNotIn("inputs", observation)
        for update in ({"kind": "external-provider-host-model"},
                       {"elf": observation["bindings"]},
                       {"sdk_manifest": observation["elf"]},
                       {"symbols": {"unselected": {
                           "type": "b", "address": 1, "size": 1}}},
                       {"inputs": {"elf": observation["elf"]},
                        "runtime_executed": True}):
            with self.subTest(update=update), self.assertRaises(EvidenceError):
                validate({**observation, **update})
        image = observation["elf"]["path"]
        for omitted in ([image],
                        ["nm", "-S", "--defined-only", "--format=posix", image]):
            changed = [row for row in rows if row["argv"] != omitted]
            with self.subTest(omitted=omitted), self.assertRaises(EvidenceError):
                validate(commands=changed)
        with self.assertRaises(EvidenceError):
            validate(commands=[*rows, rows[-1]])

    def test_native_selected_pair_replays_its_actual_observation(self):
        self.native_selected_observation("pair")

    def test_native_selected_empty_replays_its_actual_observation(self):
        self.native_selected_observation("empty")

    def test_arm_services_without_provider_retention_replay_their_checker(self):
        for index, (kind, specification) in enumerate(consumer_plan(), 1):
            if (kind != "services" or not specification.arm or
                    specification.scenario not in {"image_window", "mcuboot_control"}):
                continue
            with self.subTest(scenario=specification.scenario):
                source, build, command = self.producer.prepare_service_consumer(
                    self.prefix, self.output, specification,
                    lambda argv: self.execute(index, argv), fixture=True,
                    expected_revision=self.package["source_revision"])
                self.execute(index, command)
                self.execute(index, ["cmake", "--build", str(build),
                    "--parallel", "1", "--target", "installed_services"])
                observation = self.producer.check_service_image(self.sdk,
                    source, build, specification,
                    lambda argv: self.execute(index, argv))
                self.assertIsNone(observation["retention"])
                self.assertFalse(observation["runtime_executed"])
                result = self.query(index, observation)
                selected = result["staged_evidence"]["sdk_consumer"]
                self.assertEqual(selected["status"], "configuration_mismatch")
                self.assertEqual(selected["trust"], "semantic_verified")
                self.assertEqual(result["validation"]["installed_sdk"],
                                 "not_assessed")
                for update in ({"retention": observation["inputs"]["elf"]},
                               {"runtime_executed": True},
                               {"symbols": {}},
                               {"forbidden_symbols": {"libc_heap": ["malloc"],
                                                      "cpp_runtime": []}}):
                    rejected = self.query(index, {**observation, **update})
                    self.assertEqual(rejected["staged_evidence"]["sdk_consumer"]
                                     ["status"], "corrupt")
                symbol_command = self.commands.pop()
                self.assertEqual(symbol_command["argv"][0], "arm-none-eabi-nm")
                rejected = self.query(index, observation)
                self.assertEqual(rejected["staged_evidence"]["sdk_consumer"]
                                 ["status"], "corrupt")
                self.commands.append(symbol_command)

    def test_sdk_verification_without_B_preserves_package_inventory(self):
        prefix = self.directory / "ordinary Python SDK"
        shutil.copytree(self.prefix, prefix)
        script = """
import json
from pathlib import Path
import sys
from tools.evidence.identity import verify_sdk_package
root = Path(sys.argv[1])
before = sorted(str(path.relative_to(root)) for path in root.rglob('*'))
bytecode_before = sys.dont_write_bytecode
path_before = sys.path[:]
try:
    verify_sdk_package(root / 'share/nexus/src')
    result = {'status': 'verified'}
except Exception as error:
    result = {'status': 'failed', 'failure': str(error)}
result['inventory_unchanged'] = before == sorted(
    str(path.relative_to(root)) for path in root.rglob('*'))
result['bytecode_option_restored'] = sys.dont_write_bytecode == bytecode_before
result['import_path_restored'] = sys.path == path_before
print(json.dumps(result))
"""
        environment = process.execution_environment(os.environ)
        environment.pop("PYTHONDONTWRITEBYTECODE", None)
        execution = process.run([sys.executable, "-c", script, str(prefix)],
            cwd=ROOT, env=environment, log=self.directory / "no-B-query.txt",
            timeout=180, stage="support-read-only-query")
        self.assertEqual(execution.returncode, 0, execution.stderr)
        result = json.loads(execution.stdout)
        atomic_json(self.directory / "no-B-query-result.json", result)
        self.assertEqual(result["status"], "verified", result)
        self.assertTrue(result["inventory_unchanged"], result)
        self.assertTrue(result["bytecode_option_restored"], result)
        self.assertTrue(result["import_path_restored"], result)

    def test_installed_query_clis_without_B_preserve_package_inventory(self):
        def inventory(root):
            records = {}
            for path in sorted(root.rglob("*")):
                name = str(path.relative_to(root))
                if path.is_dir():
                    records[name] = None
                else:
                    with path.open("rb") as stream:
                        records[name] = (path.stat().st_size, stream_digest(stream))
            return records

        observations = []
        environment = process.execution_environment(os.environ)
        for variable in ("PYTHONDONTWRITEBYTECODE", "PYTHONPYCACHEPREFIX",
                         "PYCACHEPREFIX"):
            environment.pop(variable, None)
        for name in ("support", "capabilities", "explain", "package-check"):
            prefix = self.directory / (name + " SDK")
            shutil.copytree(self.prefix, prefix)
            sdk = prefix / "share/nexus/src"
            assembly = sdk / "tests/support/native/assemblies/native-uart.toml"
            if name == "support":
                argv = [str(sdk / "tools/dev/dev.py"), "support",
                        "--assembly", str(assembly), "--test-support", "--json"]
            elif name == "capabilities":
                argv = [str(sdk / "tools/maintenance/capabilities.py"),
                        "--assembly", str(assembly), "--test-support"]
            elif name == "explain":
                argv = [str(sdk / "tools/configure/configure.py"), "explain",
                        "--assembly", str(assembly), "--test-support", "--json"]
            else:
                starter = self.directory / "copied starter"
                shutil.copytree(ROOT / "tests/support/external_provider",
                                starter)
                argv = [str(sdk / "tools/configure/integration_package.py"),
                        "check", "--package", str(starter / "provider"), "--json"]
            before = inventory(prefix)
            execution = process.run([sys.executable, *argv], cwd=self.directory,
                env=environment, log=self.directory / (name + "-query.txt"),
                timeout=180, stage="support-installed-read-only-query")
            after = inventory(prefix)
            observations.append({"name": name, "exit_code": execution.returncode,
                "inventory_unchanged": before == after,
                "added": sorted(after.keys() - before.keys()),
                "removed": sorted(before.keys() - after.keys()),
                "changed": sorted(key for key in before.keys() & after.keys()
                                  if before[key] != after[key])})
        atomic_json(self.directory / "cli-query-results.json", {
            "observations": observations})
        self.assertTrue(all(row["exit_code"] == 0 and row["inventory_unchanged"]
                            for row in observations), observations)


if __name__ == "__main__":
    unittest.main()
