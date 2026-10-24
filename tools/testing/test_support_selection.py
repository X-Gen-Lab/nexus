"""Exact support queries retain selection identity without claiming execution."""

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
    def test_selected_part_backend_clock_and_dma_remain_exact(self):
        result = capabilities.selection(
            ASSEMBLIES / "qiming-baremetal-uart-dma.toml", ROOT)
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
            ASSEMBLIES / "qiming-baremetal-uart.toml", ROOT)
        sky = capabilities.selection(
            ASSEMBLIES / "sky-baremetal-uart.toml", ROOT)
        self.assertNotEqual(qiming["selection"]["part"],
                            sky["selection"]["part"])
        self.assertNotEqual(qiming["configuration_sha256"],
                            sky["configuration_sha256"])

    def test_configuration_does_not_manufacture_execution_qualification(self):
        result = capabilities.selection(
            ASSEMBLIES / "qiming-baremetal-uart.toml", ROOT)
        self.assertEqual(result["validation"]["configuration"], "validated")
        for stage in ("host_models", "arm_image", "installed_sdk",
                      "software_candidate", "physical_hil"):
            self.assertEqual(result["validation"][stage], "not_assessed")
        self.assertIs(result["physical_qualified"], False)
        self.assertTrue(result["limitations"])

    def test_native_empty_selection_cannot_invent_devices_or_hardware(self):
        result = capabilities.selection(ASSEMBLIES / "native-empty.toml", ROOT)
        self.assertEqual(result["controllers"], [])
        self.assertEqual(result["endpoints"], [])
        self.assertEqual(result["validation"]["physical_hil"],
                         "not_applicable")
        self.assertEqual(result["validation"]["host_models"], "not_assessed")
        self.assertIs(result["physical_qualified"], False)

    def test_ethernet_companion_face_is_not_lost_or_made_an_endpoint(self):
        result = capabilities.selection(
            ROOT / "tests/contracts/ethernet_assembly/stm32-zg.toml", ROOT)
        self.assertEqual(result["endpoints"], [])
        mac, = result["controllers"]
        self.assertEqual(mac["contract"]["public_class"], "eth")
        companion, = result["companion_interfaces"]
        self.assertEqual(companion["public_class"], "mdio")
        self.assertEqual(companion["id"], mac["id"])
        self.assertEqual(companion["controller"], mac["controller"])

    def test_read_only_query_preserves_input_bytes(self):
        source = ASSEMBLIES / "native-uart.toml"
        before = source.read_bytes()
        result = capabilities.selection(source, ROOT)
        self.assertEqual(source.read_bytes(), before)
        self.assertEqual(result, json.loads(json.dumps(result)))

    def test_invalid_mode_is_rejected_by_actual_resolver(self):
        source = ASSEMBLIES / "qiming-baremetal-uart-dma.toml"
        with tempfile.TemporaryDirectory(prefix="nexus-support-query-") as value:
            invalid = Path(value) / "assembly.toml"
            invalid.write_text(source.read_text().replace(
                'mode = "dma-tx"', 'mode = "invented-mode"'))
            with self.assertRaises(ValueError):
                capabilities.selection(invalid, ROOT)

    def test_cli_emits_exact_json_without_updating_derived_document(self):
        before = (ROOT / capabilities.DOCUMENT).read_bytes()
        stream = io.StringIO()
        with redirect_stdout(stream):
            code = capabilities.main([
                "--root", str(ROOT), "--assembly",
                str(ASSEMBLIES / "native-uart.toml")])
        self.assertEqual(code, 0)
        report = json.loads(stream.getvalue())
        self.assertEqual(report["kind"], "selected_support")
        self.assertEqual((ROOT / capabilities.DOCUMENT).read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
