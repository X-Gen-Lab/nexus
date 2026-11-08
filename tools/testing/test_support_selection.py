"""Exact support queries retain selection identity without claiming execution."""
from tools.configure.tests.native_fixture import native_fixture, with_native_support, canonical_assembly

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import unittest

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




    def test_absent_revision_review_remains_explicitly_unknown(self):
        for name in ("qiming-baremetal-uart.toml", "native-uart.toml"):
            with self.subTest(assembly=name):
                result = capabilities.selection(canonical_assembly(ROOT, name), ROOT, test_support=native_fixture(canonical_assembly(ROOT, name)))
                self.assertIn("silicon_admission", result)
                self.assertIsNone(result["silicon_admission"])
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


if __name__ == "__main__":
    unittest.main()
