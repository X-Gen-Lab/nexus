"""Candidate scope gates recheck actual ELF budgets and reject summary-only proof."""

import copy
import hashlib
import io
import json
import os
import shutil
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from scripts.ci.tdd_gate import validate_google_report
from scripts.ci import io_lifecycle_matrix
from scripts.validation.junit import validate_junit
from tools.evidence.identity import canonical_digest
from tools.evidence.common import EvidenceError, atomic_json, file_identity, stream_digest, verify_file_identity
from tools.evidence.proof import (validate_resources, validate_scope_proof, sdk_file_identity,
    ScopeInputs, validate_sdk_consumers, verify_execution_stream, verify_sdk_file)
from tools.dev.process import run
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

    def test_arbitrary_success_log_cannot_establish_executed_scopes(self):
        with tempfile.TemporaryDirectory() as directory:
            raw = Path(directory) / "success.log"
            raw.write_text("success: all tests and hardware passed\n")
            for scope in ("host_contracts", "arm_link", "hil_tooling"):
                report = {"scope": scope, "evidence": [],
                          "executed_checks": [{"name": "success",
                              "exit_code": 0, "status": "passed",
                              "raw": file_identity(raw)}]}
                with self.subTest(scope=scope), self.assertRaisesRegex(
                        EvidenceError, "evidence document"):
                    validate_scope_proof(report, {}, {})

    def test_unknown_scope_never_silently_has_no_proof(self):
        with self.assertRaisesRegex(EvidenceError, "unsupported.*scope"):
            validate_scope_proof({"scope": "invented", "evidence": []}, {}, {})

    def test_source_package_identity_cannot_substitute_consumer_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sdk.json"
            atomic_json(path, {"kind": "nexus-installed-source-sdk",
                              "publishable": True, "source_dirty": False,
                              "source_revision": "a" * 40,
                              "source_tree": "b" * 40})
            report = {"scope": "source_sdk", "evidence": [file_identity(path)]}
            manifest = {"source": {"commit": "a" * 40, "tree": "b" * 40}}
            with self.assertRaisesRegex(EvidenceError, "consumer.*evidence document"):
                validate_scope_proof(report, manifest, {})

    def test_four_selected_images_cannot_claim_the_full_consumer_plan(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "consumers.json"
            sdk = Path(directory) / "share/nexus/src/.nexus-source-sdk.json"
            atomic_json(sdk, {"kind": "nexus-installed-source-sdk",
                "publishable": True, "source_dirty": False,
                "source_revision": "a" * 40, "source_tree": "b" * 40,
                "files_sha256": {}, "package_files_sha256": {}})
            atomic_json(path, {"kind": "source_sdk_consumer_verification",
                "status": "passed", "source_revision": "a" * 40,
                "source_tree": "b" * 40, "physical_status": "not_executed",
                "consumers": [{"consumer_index": index, "status": "passed"}
                              for index in range(1, 5)],
                "selected_images": [{"status": "passed"}] * 4,
                "commands": []})
            # Package-byte verification has independent coverage. This isolates
            # the execution-scope distinction from package transport details.
            with patch("tools.evidence.proof.verify_sdk_package", return_value={}), \
                    self.assertRaisesRegex(EvidenceError, "complete.*consumer plan"):
                validate_scope_proof({"scope": "source_sdk",
                    "evidence": [file_identity(path), file_identity(sdk)]},
                    {"source": {"commit": "a" * 40, "tree": "b" * 40}}, {})

    def test_completed_failure_is_inspectable_but_cannot_pass_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "failed.log"
            result = run([sys.executable, "-c", "print('failed'); raise SystemExit(7)"],
                         cwd=root, env=os.environ.copy(), log=log, stage="fixture")
            self.assertEqual(result.returncode, 7)
            inputs = ScopeInputs({"evidence": [file_identity(log)]},
                                 verify_file_identity, None)
            with self.assertRaisesRegex(EvidenceError, "completed owned"):
                inputs.command(log)
            receipt = inputs.command(log, allow_failure=True)
            self.assertEqual(receipt["exit_code"], 7)
            self.assertEqual(receipt["outcome"], "completed")

    def test_empty_execution_stream_never_becomes_a_qualification_raw(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stderr.txt"
            path.write_bytes(b"")
            identity = {**sdk_file_identity(path), "role": "execution_stream"}
            self.assertEqual(verify_execution_stream(identity), path)
            path.write_bytes(b"modified")
            with self.assertRaisesRegex(EvidenceError, "stream identity changed"):
                verify_execution_stream(identity)

    def test_sdk_scope_input_role_can_share_the_same_retained_nonempty_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "consumer.c"
            path.write_text("int main(void) { return 0; }\n")
            identity = file_identity(path)
            inputs = ScopeInputs({"evidence": [identity]}, verify_file_identity,
                                 [sdk_file_identity(path)])
            self.assertEqual(inputs.path(path), path)
            changed = {**identity, "sha256": "0" * 64}
            with self.assertRaisesRegex(EvidenceError, "ambiguous"):
                ScopeInputs({"evidence": [changed]}, verify_file_identity,
                            [identity])

    def test_legacy_sdk_replays_original_members_without_current_requirements(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory)
            root = prefix / "share/nexus/src"
            root.mkdir(parents=True)
            source = root / "original.c"
            source.write_text("original frozen SDK source\n")
            exported = prefix / "lib/cmake/Nexus/NexusConfig.cmake"
            exported.parent.mkdir(parents=True)
            exported.write_text("# original frozen export\n")
            manifest_path = root / ".nexus-source-sdk.json"
            atomic_json(manifest_path, {"schema": 1, "kind": "nexus-installed-source-sdk",
                "publishable": True, "source_dirty": False,
                "source_revision": "a" * 40, "source_tree": "b" * 40,
                "files_sha256": {"original.c": file_identity(source)["sha256"]},
                "package_files_sha256": {"lib/cmake/Nexus/NexusConfig.cmake":
                                          file_identity(exported)["sha256"]}})
            identity = file_identity(manifest_path)
            closure = {"manifest_sha256": identity["sha256"],
                "files": {"original.c": sdk_file_identity(source)},
                "package_files": {"lib/cmake/Nexus/NexusConfig.cmake": sdk_file_identity(exported)}}
            report = {"scope": "source_sdk", "evidence": [identity]}
            manifest = {"source": {"commit": "a" * 40, "tree": "b" * 40}}
            with patch("tools.evidence.proof.verify_sdk_package",
                       side_effect=AssertionError("current requirements may not run")):
                _, actual = validate_scope_proof(report, manifest, {},
                    sdk_packages=[closure], legacy_replay=True)
            self.assertEqual(actual, closure)
            changed = {**closure, "files": {}}
            with self.assertRaisesRegex(EvidenceError, "member map"):
                validate_scope_proof(report, manifest, {},
                    sdk_packages=[changed], legacy_replay=True)


class ScopeExecutionTests(unittest.TestCase):
    """Isolated checker ports exercise evidence admission, not platform tests."""

    def execute(self, root, argv, log):
        result = run(argv, cwd=root, env=os.environ.copy(), log=log,
                     stage="scope-fixture")
        self.assertEqual(result.returncode, 0, result.stderr)
        return {"name": "native", "status": "passed", "exit_code": 0,
                "argv": argv, "raw": file_identity(log)}

    def copy_checker_helpers(self, root, checker):
        from tools.evidence.proof import CHECKER_SOURCES
        repository = Path(__file__).resolve().parents[2]
        for name in CHECKER_SOURCES[checker]:
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                shutil.copyfile(repository / name, target)

    def native_fixture(self, root):
        build = root / "build/native-debug"
        output = build / "tdd"
        (output / "google").mkdir(parents=True)
        binary = build / "bin/nexus_contracts"
        google = output / "google/nexus_contracts.xml"
        xml = ('<testsuites name="AllTests" tests="1" failures="0" errors="0" '
               'skipped="0"><testsuite name="Ownership" tests="1">'
               '<testcase classname="Ownership" name="Isolation" status="run" '
               'result="completed"/></testsuite></testsuites>')
        (root / "model.c").write_text("int model_run(void) { return 1; }\n")
        (root / "contracts.c").write_text(
            '#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n'
            'int model_run(void);\n'
            '#define TEST(suite, name) static void suite##_##name(void)\n'
            '#define EXPECT_EQ(a, b) do { if ((a) != (b)) exit(1); } while (0)\n'
            'TEST(Ownership, Isolation) {\n'
            '    int actual = model_run();\n'
            '    EXPECT_EQ(actual, 1);\n    EXPECT_EQ(actual - 1, 0);\n}\n'
            'int main(int argc, char **argv) {\n'
            '    for (int i = 1; i < argc; ++i) {\n'
            '        if (strcmp(argv[i], "--gtest_list_tests") == 0) {\n'
            '            puts("Ownership.\\n  Isolation"); return 0;\n        }\n    }\n'
            '    Ownership_Isolation();\n'
            f'    FILE *out = fopen({json.dumps(str(google))}, "w");\n'
            '    if (!out) return 2;\n'
            f'    fputs({json.dumps(xml)}, out);\n'
            '    fclose(out); puts("1 test passed"); return 0;\n}\n')
        (root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.20)\n'
            'project(scope_native_fixture LANGUAGES C)\n'
            'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
            'set(CMAKE_C_STANDARD 11)\n'
            'add_executable(nexus_contracts contracts.c model.c)\n'
            'set_target_properties(nexus_contracts PROPERTIES '
            'RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")\n'
            'add_custom_target(nexus_google_contracts DEPENDS nexus_contracts)\n'
            'enable_testing()\n'
            'add_test(NAME contracts COMMAND nexus_contracts)\n')
        atomic_json(root / "CMakePresets.json", {"version": 3,
            "configurePresets": [{"name": "native-debug", "generator": "Ninja",
                                  "binaryDir": str(build)}],
            "testPresets": [{"name": "native-debug", "configurePreset": "native-debug"}]})
        catalogue = root / "tools/testing/io_lifecycle_matrix.json"
        catalogue.parent.mkdir(parents=True)
        atomic_json(catalogue, {"schema": 1, "scope": "fixture protocol only",
            "properties": [{"id": "fixture-isolation", "family": "fixture",
                "peripheral": "fixture", "mode": "host", "target": "nexus_contracts",
                "case": "Ownership.Isolation", "test_source": "contracts.c",
                "production_sources": ["model.c"], "model_sources": [],
                "assertions": ["EXPECT_EQ(actual, 1)", "EXPECT_EQ(actual - 1, 0)"],
                "calls": ["model_run()"], "invariant": "fixture runs its actual C object",
                "limitations": "synthetic protocol port, not platform acceptance"}],
            "contract_sources": ["CMakeLists.txt", "CMakePresets.json"],
            "unproven": [{"scope": "hardware", "reason": "isolated fixture"}]})
        self.execute(root, [shutil.which("cmake"), "-S", str(root), "-B", str(build),
                            "-G", "Ninja"], output / "configure.txt")
        self.execute(root, [shutil.which("cmake"), "--build", str(build), "--parallel", "1"],
                     output / "build.txt")
        atomic_json(build / "google-contracts.json",
                    {"schema": 1, "executables": [str(binary)]})

        def metadata(argv, environment, log):
            self.execute(root, argv, log)
            return log.read_text()

        io_lifecycle_matrix.capture_complete(root, build, catalogue,
            output / "io-property-inputs.json", metadata, os.environ.copy())
        ctest = shutil.which("ctest")
        ctest_xml = build / "ctest-results.xml"
        self.execute(root, [str(ctest), "--test-dir", str(build),
                            "--show-only=json-v1"], output / "ctest-plan.txt")
        self.execute(root, [str(binary), "--gtest_filter=*", "--gtest_list_tests"],
                     output / "discovery-0.txt")
        self.execute(root, [str(ctest), "--preset", "native-debug",
            "--output-on-failure", "--no-tests=error", "--parallel", "1",
            "--output-junit", str(ctest_xml)], output / "ctest-execution.txt")
        summary = output / "execution.json"
        atomic_json(summary, {"schema": 1,
            "kind": "development-contract-execution", "preset": "native-debug",
            "passed": 1, "suites": [{"binary": "nexus_contracts",
                                      **validate_google_report(google)}],
            "ctest": validate_junit(ctest_xml), "scope": "fixture port only"})
        checker = root / "scripts/ci/native_contracts.py"
        checker.parent.mkdir(parents=True)
        shutil.copyfile(io_lifecycle_matrix.__file__, checker.parent / "io_lifecycle_matrix.py")
        self.copy_checker_helpers(root, "scripts/ci/native_contracts.py")
        checker.write_text("print('complete isolated checker port')\n")
        check = self.execute(root, [sys.executable, "-B", str(checker),
                                   "--preset", "native-debug"], root / "native.log")
        report = {"scope": "host_contracts", "executed_checks": [check],
                  "evidence": [file_identity(summary)]}
        return report, {"source": {"root": str(root)}}, build

    def test_complete_native_plan_and_portable_original_receipts_are_admitted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "source"
            root.mkdir()
            report, manifest, _ = self.native_fixture(root)
            inputs, closure = validate_scope_proof(report, manifest, {})
            self.assertIsNone(closure)
            self.assertTrue(any(item.get("role") == "execution_stream" and
                                item["size"] == 0 for item in inputs))
            packed = Path(directory) / "packed"
            packed.mkdir()
            for identity in inputs:
                shutil.copyfile(identity["path"], packed / identity["sha256"])
            shutil.rmtree(root)
            validate_scope_proof(report, manifest, {}, proof_inputs=inputs,
                                 resolver=lambda item: packed / item["sha256"])
            missing = [item for item in inputs
                       if not item["path"].endswith("discovery-0.command.json")]
            with self.assertRaisesRegex(EvidenceError, "missing retained"):
                validate_scope_proof(report, manifest, {}, proof_inputs=missing,
                                     resolver=lambda item: packed / item["sha256"])
            missing_dependency = [item for item in inputs
                                  if not item["path"].endswith("/.ninja_deps")]
            with self.assertRaisesRegex(EvidenceError, "missing retained"):
                validate_scope_proof(report, manifest, {}, proof_inputs=missing_dependency,
                                     resolver=lambda item: packed / item["sha256"])

    def test_native_checker_helpers_are_retained_and_rehashed_omissions_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "source"
            root.mkdir()
            report, manifest, _ = self.native_fixture(root)
            repository = Path(__file__).resolve().parents[2]
            required = ("scripts/ci/tdd_gate.py", "scripts/validation/junit.py",
                        "tools/dev/presets.py", "tools/dev/process.py")
            for name in required:
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(repository / name, target)
            retained, _ = validate_scope_proof(report, manifest, {})
            paths = {item["path"] for item in retained}
            self.assertTrue({str(root / name) for name in required} <= paths,
                            "maintained Python checker helpers were omitted")
            packed = Path(directory) / "packed"
            packed.mkdir()
            for identity in retained:
                shutil.copyfile(identity["path"], packed / identity["sha256"])
            shutil.rmtree(root)
            for name in required:
                omitted = [item for item in retained if item["path"] != str(root / name)]
                envelope = packed / "rehashable-admission-inputs.json"
                atomic_json(envelope, {"scope_inputs": omitted})
                # Rehashing the outer closure cannot remove an intrinsically
                # required checker helper from read-only portable admission.
                envelope_identity = file_identity(envelope)
                declared = json.loads(verify_file_identity(envelope_identity).read_text())
                with self.subTest(helper=name), self.assertRaisesRegex(
                        EvidenceError, "missing retained"):
                    validate_scope_proof(report, manifest, {}, proof_inputs=declared["scope_inputs"],
                                         resolver=lambda item: packed / item["sha256"])

    def test_native_junit_zero_skipped_filtered_and_failed_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report, manifest, build = self.native_fixture(root)
            xml = build / "tdd/google/nexus_contracts.xml"
            original = xml.read_text()
            for replacement in (
                    '<testsuites name="AllTests" tests="0"/>',
                    original.replace('result="completed"/>',
                                     'result="completed"><skipped/></testcase>'),
                    original.replace('result="completed"/>',
                                     'result="completed"><failure/></testcase>'),
                    original.replace('status="run"', 'status="notrun"')):
                xml.write_text(replacement)
                with self.subTest(xml=replacement), self.assertRaises(ValueError):
                    validate_scope_proof(report, manifest, {})
            xml.write_text(original)
            plan = build / "google-contracts.json"
            atomic_json(plan, {"schema": 1, "executables": []})
            with self.assertRaisesRegex(EvidenceError, "executable plan"):
                validate_scope_proof(report, manifest, {})

    def test_success_from_another_checker_does_not_qualify_host_contracts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report, manifest, _ = self.native_fixture(root)
            wrong = root / "another.py"
            wrong.write_text("print('success')\n")
            changed = copy.deepcopy(report)
            changed["executed_checks"] = [self.execute(
                root, [sys.executable, "-B", str(wrong)], root / "wrong.log")]
            with self.assertRaisesRegex(EvidenceError, "maintained scope checker"):
                validate_scope_proof(changed, manifest, {})

    def test_filtered_registration_and_interrupted_checker_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report, manifest, build = self.native_fixture(root)
            ctest_plan = build / "tdd/ctest-plan.txt"
            # Re-execute discovery with an actual filtered registration.
            script = root / "CMakeLists.txt"
            original = script.read_text()
            script.write_text(original.replace(
                "COMMAND nexus_contracts)", "COMMAND nexus_contracts --gtest_filter=Ownership.*)"))
            self.execute(root, [shutil.which("cmake"), "-S", str(root), "-B", str(build)],
                         build / "tdd/refilter.txt")
            self.execute(root, [shutil.which("ctest"), "--test-dir", str(build),
                                "--show-only=json-v1"], ctest_plan)
            with self.assertRaisesRegex(EvidenceError, "without filters"):
                validate_scope_proof(report, manifest, {})
            script.write_text(original)
            self.execute(root, [shutil.which("cmake"), "-S", str(root), "-B", str(build)],
                         build / "tdd/unfilter.txt")
            self.execute(root, [shutil.which("ctest"), "--test-dir", str(build),
                                "--show-only=json-v1"], ctest_plan)
            checker = root / "scripts/ci/native_contracts.py"
            checker.write_text("import time\nprint('started', flush=True)\ntime.sleep(5)\n")
            check = report["executed_checks"][0]
            result = run(check["argv"], cwd=root, env=os.environ.copy(),
                         log=root / "native.log", timeout=0.05,
                         stage="scope-fixture")
            self.assertEqual(result.returncode, 124)
            check["raw"] = file_identity(root / "native.log")
            with self.assertRaisesRegex(EvidenceError, "completed owned"):
                validate_scope_proof(report, manifest, {})

    def test_native_requires_complete_preexecution_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report, manifest, build = self.native_fixture(root)
            snapshot = build / "tdd/io-property-inputs.json"
            snapshot.unlink()
            with self.assertRaisesRegex((EvidenceError, ValueError), "snapshot|input|file"):
                validate_scope_proof(report, manifest, {})

    def test_native_rejects_changed_compiler_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report, manifest, build = self.native_fixture(root)
            snapshot = build / "tdd/io-property-inputs.json"
            before = json.loads(snapshot.read_text())
            before["complete_sources"][str(root / "model.c")] = "0" * 64
            atomic_json(snapshot, before)
            with self.assertRaisesRegex((EvidenceError, ValueError), "changed|snapshot|identity"):
                validate_scope_proof(report, manifest, {})

    def test_real_hil_prepare_checker_remains_tooling_only(self):
        repository = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = repository / "tools/hil/fixtures/stm32f407_qiming_v31.json"
            selected = json.loads(fixture.read_text())
            console = selected["serial"]
            resolved = {"schema_version": 1, "board": selected["board"],
                "part": selected["part"], "backend": "baremetal",
                "controllers": [{"id": "console", "kind": "uart",
                    "controller": console["controller"], "mode": "irq-blocks",
                    "baud": console["baudrate"], "pins": [
                        {"function": "tx", "pin": console["tx"]},
                        {"function": "rx", "pin": console["rx"]}]}]}
            resolved["configuration_sha256"] = canonical_digest(resolved)
            configuration = root / "resolved.json"
            atomic_json(configuration, resolved)
            output = root / "preparation.json"
            argv = [sys.executable, "-B", str(repository / "tools/hil/prepare.py"),
                    "--fixture", str(fixture), "--resolved", str(configuration),
                    "--report", str(output)]
            check = self.execute(root, argv, root / "prepare.log")
            report = {"scope": "hil_tooling", "executed_checks": [check],
                      "evidence": [file_identity(output)]}
            manifest = {"source": {"root": str(repository)},
                        "configuration": file_identity(configuration)}
            inputs, _ = validate_scope_proof(report, manifest, resolved)
            self.assertTrue(inputs)
            echo = self.execute(root, ["/bin/echo", *argv[1:]], root / "echo.log")
            spoofed = {**report, "executed_checks": [echo]}
            with self.assertRaisesRegex(EvidenceError, "trusted.*Python"):
                validate_scope_proof(spoofed, manifest, resolved)
            changed = json.loads(output.read_text())
            changed["physical_status"] = "passed"
            atomic_json(output, changed)
            report["evidence"] = [file_identity(output)]
            with self.assertRaisesRegex(EvidenceError, "preparation differs"):
                validate_scope_proof(report, manifest, resolved)

    def test_same_arm_elf_requires_actual_build_provenance_and_receipt(self):
        repository = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf, configuration, _ = link_fixture(root)
            compiler = os.environ.get("NEXUS_ARM_GCC") or shutil.which("arm-none-eabi-gcc")
            cmake = Path(shutil.which("cmake")).resolve()
            compiler_argv = [compiler, "-mcpu=cortex-m4", "-mthumb",
                "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-nostdlib", "-Os",
                "-Wl,-Map=" + str(root / "image.map"), "-T", str(root / "memory.ld"),
                str(root / "image.c"), "-o", str(elf)]
            writer = root / "binary.py"
            writer.write_text("import sys\nfrom pathlib import Path\n"
                f"sys.path.insert(0, {str(repository)!r})\n"
                "from tools.measurement.elf import Elf32, binary_from_elf\n"
                f"elf = Path({str(elf)!r})\n"
                "elf.with_suffix('.bin').write_bytes(binary_from_elf("
                "Elf32(elf.read_bytes()), 0x08000000, 0x08100000))\n"
                "print('actual ARM fixture linked')\n")
            def cmake_word(value):
                return '"' + str(value).replace("\\", "/").replace('"', '\\"') + '"'
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\nproject(scope_arm NONE)\n"
                "add_custom_target(arm_fixture ALL\n COMMAND " +
                " ".join(cmake_word(value) for value in compiler_argv) +
                "\n COMMAND " + " ".join(cmake_word(value) for value in
                    (sys.executable, "-B", writer)) + "\n VERBATIM)\n")
            atomic_json(root / "CMakePresets.json", {"version": 6,
                "configurePresets": [{"name": "arm-fixture", "generator": "Ninja",
                    "binaryDir": str(root / "build/arm-fixture")}],
                "buildPresets": [{"name": "arm-fixture", "configurePreset": "arm-fixture"}]})
            self.execute(root, [str(cmake), "--preset", "arm-fixture"], root / "configure.log")
            inner_argv = [str(cmake), "--build", "--preset", "arm-fixture"]
            log = root / "build/quality/developer-command.txt"
            checker = root / "tools/dev/dev.py"
            checker.parent.mkdir(parents=True)
            self.copy_checker_helpers(root, "tools/dev/dev.py")
            checker.write_text("import os, sys\nfrom pathlib import Path\n"
                f"sys.path.insert(0, {str(repository)!r})\n"
                "from tools.dev.process import run\n"
                f"result = run({inner_argv!r}, cwd=Path({str(root)!r}), "
                f"env=os.environ.copy(), log=Path({str(log)!r}), stage='build')\n"
                "print(result.stdout)\nraise SystemExit(result.returncode)\n")
            argv = [sys.executable, "-B", str(checker), "build", "--preset", "arm-fixture"]
            check = self.execute(root, argv, root / "arm.log")
            view = json.loads(log.with_suffix(".command.json").read_text())
            receipt = Path(view["attempt_receipt"])
            manifest = {"source": {"root": str(root)},
                "configuration": file_identity(configuration),
                "artifacts": {role: file_identity(root / ("image." + role))
                              for role in ("elf", "bin", "map")}}
            provenance = root / "build-provenance.json"
            atomic_json(provenance, {"schema_version": 1, "kind": "build_provenance",
                "source": manifest["source"], "configuration": manifest["configuration"],
                "status": "identities_recorded", "software_qualified": False,
                "action": "build", "commands": [inner_argv],
                "artifacts": list(manifest["artifacts"].values()),
                "actual_execution": {"receipt": file_identity(receipt),
                    "duration_seconds": view["duration_seconds"],
                    "stdout_stderr": [view["stdout"], view["stderr"]]}})
            report = {"scope": "arm_link", "executed_checks": [check],
                      "evidence": [file_identity(provenance)]}
            resolved = json.loads(configuration.read_text())
            inputs, _ = validate_scope_proof(report, manifest, resolved)
            self.assertTrue(inputs)
            changed = copy.deepcopy(manifest)
            changed["artifacts"]["map"] = file_identity(configuration)
            with self.assertRaisesRegex(EvidenceError, "same-artifact"):
                validate_scope_proof(report, changed, resolved)



if __name__ == "__main__":
    unittest.main()
