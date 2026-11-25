"""Exact support queries retain selection identity without claiming execution."""
from tools.configure.tests.native_fixture import native_fixture, with_native_support, canonical_assembly

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.maintenance import capabilities


ROOT = Path(__file__).resolve().parents[2]
ASSEMBLIES = ROOT / "tools/configure/assemblies"


class ExactSupportSelectionTests(unittest.TestCase):
    def test_os_profile_projects_exact_kernel_abi_tick_and_stack_policy(self):
        result = capabilities.selection(
            ASSEMBLIES / "qiming-freertos-uart.toml", ROOT, test_support=native_fixture(ASSEMBLIES / "qiming-freertos-uart.toml"))
        selected = result["os"]
        self.assertEqual(selected["backend"], "freertos")
        self.assertEqual(selected["kernel"], [{
            "path": "ext/freertos",
            "commit": "062e1e0d8b176ae73d257ac928766b33bea17b6f"}])
        self.assertEqual(selected["port"], "GCC/ARM_CM4F")
        self.assertEqual(selected["tick"],
                         {"source": "systick", "hz": 1000,
                          "reload": 167999, "tickless": False})
        self.assertEqual(selected["abi"]["cpu"]["float_abi"], "hard")
        self.assertIn("-fshort-enums", selected["abi"]["compile_options"])
        self.assertEqual(selected["storage"]["minimum_task_stack_words"], 20)
        self.assertEqual(selected["storage"]["stack_alignment_bytes"], 8)
        self.assertTrue(selected["capabilities"]["direct_notification"])
        self.assertIsNone(selected["storage"]["system_call_stack_words"])
        self.assertEqual(selected["execution"], "not_assessed")
        self.assertEqual(selected["runtime_stack_peak"], "not_measured")
        self.assertEqual(selected["linked_costs"], "not_measured")
        self.assertFalse(result["physical_qualified"])

    def test_os_minimal_policy_does_not_advertise_disabled_direct_notify(self):
        with tempfile.TemporaryDirectory() as temporary:
            assembly = Path(temporary) / "minimal.toml"
            assembly.write_text(
                (ASSEMBLIES / "qiming-freertos-uart.toml").read_text()
                + '\n[os]\nprofile = "minimal"\n')
            result = capabilities.selection(assembly, ROOT, test_support=native_fixture(assembly))
        selected = result["os"]
        self.assertEqual(selected["profile"]["profile"], "minimal")
        self.assertFalse(selected["capabilities"]["direct_notification"])
        self.assertTrue(selected["capabilities"]["joinable_task"])
        self.assertTrue(selected["capabilities"]["permanent_task"])
        self.assertIn("Task/control/stack storage remains live until successful join.",
                      selected["restrictions"])

    def test_non_kernel_queries_do_not_invent_a_kernel_or_stack_size(self):
        for name, backend in (("native-uart.toml", "native"),
                              ("qiming-baremetal-uart.toml", "baremetal")):
            with self.subTest(backend=backend):
                result = capabilities.selection(canonical_assembly(ROOT, name), ROOT, test_support=native_fixture(canonical_assembly(ROOT, name)))
                selected = result["os"]
                self.assertEqual(selected["backend"], backend)
                self.assertEqual(selected["kernel"], [])
                self.assertIsNone(selected["profile"])
                self.assertIsNone(selected["port"])
                self.assertIsNone(selected["tick"])
                self.assertIsNone(
                    selected["storage"]["minimum_task_stack_words"])
                self.assertFalse(selected["capabilities"]["direct_notification"])
                self.assertFalse(selected["capabilities"]["permanent_task"])
                self.assertEqual(selected["capabilities"]["joinable_task"],
                                 backend == "native")

    def test_system_review_does_not_invent_controller_or_physical_approval(self):
        for name in ("qiming-baremetal-uart.toml", "native-uart.toml"):
            with self.subTest(assembly=name):
                result = capabilities.selection(canonical_assembly(ROOT, name), ROOT, test_support=native_fixture(canonical_assembly(ROOT, name)))
                self.assertIn("silicon_admission", result)
                review = result["silicon_admission"]
                if name == "native-uart.toml":
                    self.assertIsNone(review)
                else:
                    self.assertEqual("stm32f4", review["provider"])
                    self.assertEqual(result["selection"]["part"], review["part"])
                    self.assertEqual("reject", review["unknown_revision"])
                    self.assertTrue(review["reviewed_revision_ids"])
                    self.assertIn("ES0182 Rev19", review["review_scope"])
                    self.assertIs(review["physical_qualified"], False)
                self.assertTrue(all(item.get("silicon_admission", "missing")
                                    is None for item in result["controllers"]))
                self.assertIs(result["physical_qualified"], False)

    def test_selected_part_backend_clock_and_dma_remain_exact(self):
        result = capabilities.selection(
            ASSEMBLIES / "qiming-baremetal-uart-dma.toml", ROOT, test_support=native_fixture(ASSEMBLIES / "qiming-baremetal-uart-dma.toml"))
        self.assertEqual(result["selection"]["part"], "STM32F407ZGT6")
        self.assertEqual(result["selection"]["backend"], "baremetal")
        self.assertEqual(result["selection"]["clock_profile"], "hse8-pll168")
        uart, = result["controllers"]
        self.assertEqual(uart["controller"], "USART1")
        self.assertEqual(uart["mode"], "dma-tx")
        self.assertEqual(uart["dma"], ["DMA2:stream7:channel4"])
        self.assertEqual(uart["contract"]["public_class"], "uart")
        self.assertEqual(uart["contract"]["unit"], "bytes")
        self.assertTrue(result["memory_domains"])
        self.assertTrue(result["resource_owners"])
        self.assertEqual(len(result["configuration_sha256"]), 64)
        self.assertTrue(result["input_identities"])

    def test_board_variants_are_not_collapsed_to_family_support(self):
        qiming = capabilities.selection(
            ASSEMBLIES / "qiming-baremetal-uart.toml", ROOT, test_support=native_fixture(ASSEMBLIES / "qiming-baremetal-uart.toml"))
        sky = capabilities.selection(
            ASSEMBLIES / "sky-baremetal-uart.toml", ROOT, test_support=native_fixture(ASSEMBLIES / "sky-baremetal-uart.toml"))
        self.assertNotEqual(qiming["selection"]["part"],
                            sky["selection"]["part"])
        self.assertNotEqual(qiming["configuration_sha256"],
                            sky["configuration_sha256"])

    def test_configuration_does_not_manufacture_execution_qualification(self):
        result = capabilities.selection(
            ASSEMBLIES / "qiming-baremetal-uart.toml", ROOT, test_support=native_fixture(ASSEMBLIES / "qiming-baremetal-uart.toml"))
        self.assertEqual(result["validation"]["configuration"], "validated")
        for stage in ("host_models", "arm_image", "installed_sdk",
                      "software_candidate", "physical_hil"):
            self.assertEqual(result["validation"][stage], "not_assessed")
        self.assertIs(result["physical_qualified"], False)
        self.assertTrue(result["limitations"])

    def test_native_empty_selection_cannot_invent_devices_or_hardware(self):
        result = capabilities.selection(ROOT / "tests/support/native/assemblies/native-empty.toml", ROOT, test_support=native_fixture(ROOT / "tests/support/native/assemblies/native-empty.toml"))
        self.assertEqual(result["controllers"], [])
        self.assertEqual(result["endpoints"], [])
        self.assertEqual(result["validation"]["physical_hil"],
                         "not_applicable")
        self.assertEqual(result["validation"]["host_models"], "not_assessed")
        self.assertIs(result["physical_qualified"], False)

    def test_ethernet_companion_face_is_not_lost_or_made_an_endpoint(self):
        result = capabilities.selection(
            ROOT / "tests/contracts/ethernet_assembly/stm32-zg.toml", ROOT, test_support=native_fixture(ROOT / "tests/contracts/ethernet_assembly/stm32-zg.toml"))
        self.assertEqual(result["endpoints"], [])
        mac, = result["controllers"]
        self.assertEqual(mac["contract"]["public_class"], "eth")
        companion, = result["companion_interfaces"]
        self.assertEqual(companion["public_class"], "mdio")
        self.assertEqual(companion["id"], mac["id"])
        self.assertEqual(companion["controller"], mac["controller"])

    def test_read_only_query_preserves_input_bytes(self):
        source = ROOT / "tests/support/native/assemblies/native-uart.toml"
        before = source.read_bytes()
        result = capabilities.selection(source, ROOT, test_support=native_fixture(source))
        self.assertEqual(source.read_bytes(), before)
        self.assertEqual(result, json.loads(json.dumps(result)))

    def test_invalid_mode_is_rejected_by_actual_resolver(self):
        source = ASSEMBLIES / "qiming-baremetal-uart-dma.toml"
        with tempfile.TemporaryDirectory(prefix="nexus-support-query-") as value:
            invalid = Path(value) / "assembly.toml"
            invalid.write_text(source.read_text().replace(
                'mode = "dma-tx"', 'mode = "invented-mode"'))
            with self.assertRaises(ValueError):
                capabilities.selection(invalid, ROOT, test_support=native_fixture(invalid))

    def test_cli_emits_exact_json_without_updating_derived_document(self):
        before = (ROOT / capabilities.DOCUMENT).read_bytes()
        stream = io.StringIO()
        with redirect_stdout(stream):
            code = capabilities.main(with_native_support([
                "--root", str(ROOT), "--assembly",
                str(ROOT / "tests/support/native/assemblies/native-uart.toml")]))
        self.assertEqual(code, 0)
        report = json.loads(stream.getvalue())
        self.assertEqual(report["kind"], "selected_support")
        self.assertEqual((ROOT / capabilities.DOCUMENT).read_bytes(), before)


class SupportEvidenceSelectionTests(unittest.TestCase):
    def setUp(self):
        from tools.configure import configure
        from tools.evidence.common import atomic_json
        from tools.measurement import resources
        from tests.contracts.test_evidence_support import link_fixture

        temporary = tempfile.TemporaryDirectory(prefix="support evidence ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.assembly = self.directory / "assembly.toml"
        self.assembly.write_text((ASSEMBLIES / "qiming-baremetal-empty.toml")
            .read_text().replace("main_stack_bytes = 2048", "main_stack_bytes = 1024")
            .replace("libc_heap_bytes = 0", "libc_heap_bytes = 64"))
        generated = self.directory / "generated"
        configure.configure(self.assembly, generated)
        self.resolved = generated / "resolved.json"
        value = json.loads(self.resolved.read_text())
        linked = self.directory / "linked"
        linked.mkdir()
        self.compiler = shutil.which("arm-none-eabi-gcc")
        self.assertIsNotNone(self.compiler, "Prepared ARM compiler required")
        self.elf, _, _ = link_fixture(linked)
        image = linked / "image.c"
        handlers = value["cortex_m_irq_vectors"]
        declarations = "".join("void " + handler + "(void) {}\n"
                               for handler in handlers)
        entries = ", ".join("[" + str(16 + number) + "] = " + handler
                            for handler, number in handlers.items())
        vector = "const void* const vectors[" + str(
            16 + value["irq"]["external_count"]) + "] = {&__nx_msp_end, Reset_Handler, "
        vector += entries + "};"
        source = image.read_text().replace(
            "const void* const vectors[] = {&__nx_msp_end, Reset_Handler};", vector)
        image.write_text(declarations + source)
        script = linked / "memory.ld"
        script.write_text(re.sub(r"__nx_resolved_sha256_(\d+) = 0x[0-9a-f]+;",
            lambda match: "__nx_resolved_sha256_" + match[1] + " = 0x" +
            value["configuration_sha256"][int(match[1]) * 8:int(match[1]) * 8 + 8] + ";",
            script.read_text()))
        command = [self.compiler, "-mcpu=cortex-m4", "-mthumb",
            "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-nostdlib", "-Os",
            "-T", str(script), str(linked / "image.c"), "-o", str(self.elf)]
        subprocess.run(command, capture_output=True, check=True, timeout=30)
        self.resource_report = self.directory / "resources.json"
        self.resource = resources.measure(self.elf, self.resolved,
                                          generated / "resource-budget.json")
        atomic_json(self.resource_report, self.resource)

    def query(self, *reports):
        return capabilities.selection(self.assembly, ROOT, evidence=reports)

    def test_resource_identity_match_does_not_invent_source_or_qualification(self):
        result = self.query(self.resource_report)
        record, = result["evidence"]
        self.assertEqual(record["status"], "not_assessed")
        self.assertEqual(record["identities"], {
            "source": "not_recorded", "configuration": "matched",
            "backend_modes": "matched", "inputs": "matched",
            "artifact": "matched"})
        self.assertEqual(record["reason"], "source_identity_not_recorded")
        self.assertEqual(result["validation"]["resource_budget"], "not_assessed")
        self.assertIs(result["physical_qualified"], False)

    def test_unknown_or_forged_report_cannot_promote_a_selection(self):
        forged = self.directory / "forged.json"
        variants = (
            dict(self.resource, kind="invented", status="passed"),
            dict(self.resource, metrics={}),
            dict(self.resource, elf={**self.resource["elf"], "sha256": "0" * 64}),
        )
        for report in variants:
            with self.subTest(kind=report["kind"], metrics=bool(report.get("metrics"))):
                forged.write_text(json.dumps(report))
                result = self.query(forged)
                self.assertEqual(result["evidence"][0]["status"], "not_assessed")
                self.assertEqual(result["validation"]["arm_image"], "not_assessed")
                self.assertEqual(result["validation"]["resource_budget"], "not_assessed")

    def test_changed_assembly_keeps_the_old_report_unassociated(self):
        self.assembly.write_text(self.assembly.read_text() +
                                 '\n[factory]\ncapabilities=true\n')
        result = self.query(self.resource_report)
        self.assertEqual(result["evidence"][0]["status"], "not_assessed")
        self.assertNotEqual(result["evidence"][0]["identities"]["configuration"],
                            "matched")

    def test_read_only_cli_accepts_repeated_evidence_paths(self):
        stream = io.StringIO()
        original = self.resource_report.read_bytes()
        with redirect_stdout(stream):
            code = capabilities.main([
                "--assembly", str(self.assembly), "--evidence",
                str(self.resource_report), "--evidence", str(self.resource_report)],
                support_command=True)
        self.assertEqual(code, 0)
        self.assertEqual(len(json.loads(stream.getvalue())["evidence"]), 2)
        self.assertEqual(original, self.resource_report.read_bytes())

    def test_sdk_identity_match_does_not_promote_arm_or_hardware(self):
        with patch.object(capabilities, "selection_evidence", return_value={
                "kind": "source_sdk_consumer_verification",
                "status": "identity_matched",
                "software_qualified": False}):
            result = self.query(self.resource_report)
        self.assertEqual(result["validation"]["installed_sdk"],
                         "identity_matched")
        self.assertEqual(result["validation"]["arm_image"], "not_assessed")
        self.assertEqual(result["validation"]["physical_hil"], "not_assessed")
        self.assertFalse(result["physical_qualified"])

    def test_malformed_optional_report_preserves_the_validated_selection(self):
        broken = self.directory / "incomplete.json"
        broken.write_text('{"schema_version": 1,')
        result = self.query(broken, self.resource_report)
        self.assertEqual(result["validation"]["configuration"], "validated")
        self.assertEqual([item["status"] for item in result["evidence"]],
                         ["not_assessed", "not_assessed"])

    def test_sdk_build_receipt_rejects_changed_output(self):
        from tools.dev import process
        from tools.evidence.common import EvidenceError

        source = self.directory / "consumer"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(SupportReceipt NONE)\n"
            "add_custom_target(consumer ALL COMMAND "
            "${CMAKE_COMMAND} -E echo installed-consumer)\n")
        build = self.directory / "consumer-build"
        subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja"],
                       capture_output=True, check=True, timeout=30)
        argv = ["cmake", "--build", str(build)]
        result = process.run(argv, cwd=self.directory, env=os.environ.copy(),
                             log=self.directory / "consumer.log",
                             timeout=30, stage="source-sdk-consumer")
        self.assertEqual(result.returncode, 0)
        consumer = {"consumer_index": 1}
        report = {"commands": [{
            **consumer, "argv": argv, "exit_code": 0,
            "withdrawal_complete": True,
            "receipt": str(result.receipt_path.relative_to(self.directory)),
        }]}
        capabilities._successful_build(self.directory, report, consumer, build)
        result.receipt_path.with_name("stdout.txt").write_text("replaced")
        with self.assertRaisesRegex(EvidenceError, "build output has changed"):
            capabilities._successful_build(self.directory, report, consumer, build)

    def test_build_provenance_identity_is_observed_and_never_qualified(self):
        from tools.dev import provenance
        source = {"root": str(ROOT), "commit": "1" * 40, "tree": "2" * 40,
                  "dirty": False, "submodules": []}
        record = provenance.provenance(source, [self.resolved], [self.elf],
            [[self.compiler, "--version"]], configuration=self.resolved)
        report = self.directory / "provenance.json"
        report.write_text(json.dumps(record))
        with patch("tools.evidence.identity.observe_git_source", return_value=source):
            result = self.query(report)
        self.assertEqual(result["evidence"][0]["status"], "identity_matched")
        self.assertEqual(result["validation"]["arm_image"], "identity_matched")
        self.assertEqual(result["validation"]["software_candidate"], "not_assessed")
        with patch("tools.evidence.identity.observe_git_source",
                   return_value={**source, "tree": "3" * 40}):
            stale = self.query(report)
        self.assertEqual(stale["evidence"][0]["status"], "not_assessed")
        self.assertEqual(stale["validation"]["arm_image"], "not_assessed")


if __name__ == "__main__":
    unittest.main()
