"""Exact typed I/O selections prepare protocols, never operate equipment."""

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure
from tools.evidence.common import EvidenceError, file_identity
from tools.hil import workloads


class HilIoProtocolTests(unittest.TestCase):
    """Pure real-TOML selection checks without compiler or device effects."""

    cases = ("uart_word_wire", "spi_dma_whole_cs_segments")

    def resolved(self, name):
        directory = ("g474_control_assembly" if name == "pwm-group" else
                     "io_evolution_assembly")
        return configure.resolve_ir(configure.ROOT / "tests/contracts" /
                                    directory / (name + ".toml"),
                                    test_support=name.startswith("native-")).to_dict()

    def selection(self, name, resolved):
        return workloads._selection(name, workloads.WORKLOADS[name], resolved)

    def spi_plan(self, resolved=None, workload=None):
        resolved = resolved or self.resolved("f407-spi-dma-transaction")
        fixture = json.loads((configure.ROOT / "tools/hil/fixtures" /
                              "stm32f407_qiming_v31.json").read_text())
        # Synthetic software-only fixture identity for this exact link fixture;
        # no existing physical Board fixture or observed PCB is relabelled.
        fixture.update(board=resolved["board"], part=resolved["part"],
                       scope="software_reference")
        return workloads.workload_plan(fixture, resolved, workload), fixture

    def protocol(self, definition, instrument, bindings):
        return {
            "id": definition["id"], "bindings": bindings,
            "stimulus": "Reviewed synthetic test protocol; never executed",
            "raw_observations": {key: "Retain " + key for key in
                                 definition["raw_observations"]},
            "instruments": {key: file_identity(instrument) for key in
                            definition["instruments"]},
            "safety": {key: "Review " + key for key in definition["safety"]},
            "limits": {key: {"minimum": 1 if key in workloads.WORK_AMOUNTS
                              else 0, "maximum": 10, "unit": unit}
                       for key, unit in definition["metric_units"].items()},
        }

    def test_four_protocols_remain_explicit_acquisition_obligations(self):
        for name in self.cases:
            with self.subTest(name=name):
                self.assertEqual(workloads.WORKLOADS[name]["work_item"], 7)
                self.assertTrue(workloads.WORKLOADS[name]["raw_observations"])

    def test_actual_word_faces_select_nine_bit_protocol(self):
        for family in ("f407",):
            with self.subTest(family=family):
                self.assertEqual(self.selection("uart_word_wire",
                    self.resolved(family + "-uart9n1")), ("selected", []))

    def test_byte_face_or_unproven_frame_cannot_select_word_protocol(self):
        for change in ({"mode": "irq-byte-event"}, {"frame": None},
                       {"frame": {"data_bits": 8, "parity": "none",
                                  "stop_bits": 1, "flow": "none"}}):
            resolved = self.resolved("f407-uart9n1")
            resolved["controllers"][0].update(change)
            with self.subTest(change=change):
                self.assertEqual(self.selection("uart_word_wire", resolved)[0],
                                 "requires_selection")

    def test_native_word_model_never_selects_physical_protocol(self):
        self.assertEqual(self.selection("uart_word_wire",
            self.resolved("native-uart9n1"))[0], "unsupported")

    def test_actual_enhanced_spi_endpoint_selects_whole_cs_protocol(self):
        for family in ("f407",):
            with self.subTest(family=family):
                self.assertEqual(self.selection("spi_dma_whole_cs_segments",
                    self.resolved(family + "-spi-dma-transaction")),
                    ("selected", []))

    def test_short_spi_or_absent_foreign_endpoint_does_not_select_segments(self):
        short = self.resolved("f407-spi-transaction")
        self.assertEqual(self.selection("spi_dma_whole_cs_segments", short)[0],
                         "requires_selection")
        for children in ([], [{"driver": "spi-dma-transaction",
                               "controller": "foreign", "id": "foreign"}]):
            resolved = self.resolved("f407-spi-dma-transaction")
            resolved["devices"] = children
            with self.subTest(children=children):
                self.assertEqual(self.selection("spi_dma_whole_cs_segments",
                    resolved)[0], "requires_selection")





    def test_plan_preserves_not_executed_and_never_operates_equipment(self):
        resolved = self.resolved("f407-spi-dma-transaction")
        with patch("subprocess.run", side_effect=AssertionError("equipment")), \
                patch("subprocess.Popen", side_effect=AssertionError("equipment")):
            report, _ = self.spi_plan(resolved)
        self.assertEqual(report["physical_status"], "not_executed")
        self.assertEqual(report["status"], "prepared_unbound")
        for name in self.cases:
            item = next(case for case in report["cases"] if case["id"] == name)
            self.assertEqual(item["status"], "not_executed")
            self.assertIsNone(item["reviewed_protocol"])
            self.assertTrue(all(value is None for value in item["limits"].values()))

    def test_segment_protocol_binds_actual_controller_not_another_spi(self):
        resolved = self.resolved("f407-spi-dma-transaction")
        foreign = copy.deepcopy(resolved["controllers"][0])
        foreign.update(id="foreign", controller="SPI2")
        resolved["controllers"].append(foreign)
        plan, fixture = self.spi_plan(resolved)
        definition = next(item for item in plan["cases"]
                          if item["id"] == "spi_dma_whole_cs_segments")
        with tempfile.TemporaryDirectory() as temporary:
            instrument = Path(temporary) / "instrument.json"
            instrument.write_text("synthetic instrument config; never run\n")
            case = self.protocol(definition, instrument, ["spi0"])
            workload = {"schema_version": 1, "kind": "hil_workload",
                        "board": resolved["board"], "part": resolved["part"],
                        "cases": [case]}
            self.assertEqual(workloads.validate_workload(workload, fixture,
                                                        resolved), workload)
            case["bindings"] = ["foreign"]
            with self.assertRaisesRegex(EvidenceError, "actual|selected|protocol"):
                workloads.validate_workload(workload, fixture, resolved)

    def test_word_protocol_requires_native_word_capture_not_serial8_adapter(self):
        definition = workloads.WORKLOADS["uart_word_wire"]
        self.assertIn("native_word_peer", definition["instruments"])
        self.assertIn("word_values_bit8", definition["raw_observations"])
        self.assertNotIn("serial_capture", definition["instruments"])

    def test_sole_word_console_remains_rejected_by_existing_smoke_admission(self):
        fixture = json.loads((configure.ROOT / "tools/hil/fixtures" /
                              "stm32f407_qiming_v31.json").read_text())
        with self.assertRaisesRegex(EvidenceError, "8N1|word"):
            workloads.workload_plan(fixture, self.resolved("f407-uart9n1"))


if __name__ == "__main__":
    unittest.main()
