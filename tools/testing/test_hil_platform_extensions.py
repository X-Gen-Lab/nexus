"""Synthetic current-mode HIL contracts never operate a physical station."""

import copy
import json
from pathlib import Path
import tempfile
import unittest

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.evidence.identity import canonical_digest
from tools.hil.admit import validate_fixture, validate_raw, validate_station
from tools.hil.prepare import preparation_plan
from tools.testing import test_hil_workloads
from tests.contracts import test_hil_admission


class HilPlatformExtensionTests(unittest.TestCase):
    def setUp(self):
        self.fixture = test_hil_workloads.HilWorkloadTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.module = self.fixture.module
        controllers = self.fixture.resolved["controllers"]
        self.fixture.resolved["controllers"] = controllers[:9]

    def case(self, name):
        return self.fixture.case(name)

    def add(self, kind, mode):
        self.fixture.resolved["controllers"].append({
            "id": kind + "0", "kind": kind, "controller": kind.upper(),
            "mode": mode, "pins": []})

    def test_current_protocol_inventory_has_no_invented_observations(self):
        report = self.fixture.plan()
        self.assertEqual(len(report["cases"]), 25)
        for item in report["cases"]:
            self.assertEqual(item["status"], "not_executed")
            self.assertTrue(all(value is None
                                for value in item["limits"].values()))

    def test_all_four_receive_modes_are_real_dma_selection(self):
        self.fixture.resolved["controllers"][2]["mode"] = "short-poll"
        for mode in ("rx-dma-idle", "dma-duplex", "rx-dma-continuous",
                     "dma-duplex-continuous"):
            self.fixture.resolved["controllers"][1]["mode"] = mode
            with self.subTest(mode=mode):
                self.assertEqual(self.case("uart_spi_dma")["availability"],
                                 "selected")
                self.assertEqual(self.case("uart_rx_dma")["availability"],
                                 "selected")

    def test_unknown_dma_spelling_cannot_select_receive_protocol(self):
        self.fixture.resolved["controllers"][1]["mode"] = "future-rx-dma"
        self.assertEqual(self.case("uart_rx_dma")["availability"],
                         "requires_selection")

    def test_continuous_adc_requires_exact_selected_mode(self):
        adc = self.fixture.resolved["controllers"][5]
        for mode, availability in (("single-shot", "requires_selection"),
                                   ("trigger-dma", "requires_selection"),
                                   ("trigger-dma-continuous", "selected")):
            adc["mode"] = mode
            with self.subTest(mode=mode):
                self.assertEqual(self.case("adc_continuous")["availability"],
                                 availability)

    def test_bus_protocols_require_selected_exact_provider_modes(self):
        for kind, mode, name in (("can", "classic-irq", "can_bus"),
                                 ("usb_device", "device-fs", "usb_device"),
                                 ("eth", "raw-rmii", "ethernet_raw"),
                                 ("timer_capture", "edge-capture",
                                  "timer_capture"),
                                 ("advanced_pwm", "complementary-pwm",
                                  "advanced_pwm_break")):
            with self.subTest(kind=kind):
                self.assertEqual(self.case(name)["availability"],
                                 "requires_selection")
                self.add(kind, "future-mode")
                self.assertEqual(self.case(name)["availability"],
                                 "requires_selection")
                self.fixture.resolved["controllers"][-1]["mode"] = mode
                self.assertEqual(self.case(name)["availability"], "selected")

    def test_wrong_binding_cannot_satisfy_can_bus_controller(self):
        self.add("can", "classic-irq")
        workload = self.fixture.reviewed(("can_bus",))
        workload["cases"][0]["bindings"] = ["uart0"]
        with self.assertRaises(EvidenceError):
            self.fixture.validate(workload)

    def test_fault_protocol_requires_observer_masks_and_retained_effects(self):
        case = self.case("fail_stop")
        self.assertLessEqual({"fault_observer_record", "incoming_masks",
                             "retained_loans", "terminal_progress"},
                             set(case["raw_observations"]))

    def test_reference_g4_fixture_prepares_without_a_physical_station(self):
        path = Path("tools/hil/fixtures/stm32g474re_hse24_reference.json")
        fixture = json.loads(path.read_text())
        resolved = copy.deepcopy(self.fixture.resolved)
        resolved.update(board=fixture["board"], part=fixture["part"])
        resolved["controllers"] = [{
            "id": "console", "kind": "uart", "controller": "LPUART1",
            "mode": "irq-byte-event", "baud": 115200,
            "pins": [{"function": "tx", "pin": "PA2"},
                     {"function": "rx", "pin": "PA3"}]}]
        report = self.module.workload_plan(fixture, resolved)
        self.assertEqual(fixture["scope"], "software_reference")
        self.assertEqual(report["physical_status"], "not_executed")
        self.assertEqual(report["status"], "prepared_unbound")
        self.assertNotIn("station_id", report)

    def test_template_cannot_pretend_to_be_reviewed_or_supply_numeric_budgets(self):
        template_path = Path("tools/hil/templates/workload.json")
        template = json.loads(template_path.read_text())
        self.assertEqual({item["id"] for item in template["cases"]},
                         set(self.module.WORKLOADS))
        for item in template["cases"]:
            self.assertTrue(all(bound["maximum"] is None
                                for bound in item["limits"].values()))
        with self.assertRaises(EvidenceError):
            self.fixture.validate(template)

    def test_reference_fixture_cannot_self_promote_by_deleting_scope(self):
        path = Path("tools/hil/fixtures/stm32g474re_hse24_reference.json")
        fixture = json.loads(path.read_text())
        fixture.pop("scope")
        with self.assertRaises(EvidenceError):
            validate_fixture(fixture)

    def test_reference_fixture_cannot_admit_a_physical_station(self):
        path = Path("tools/hil/fixtures/stm32g474re_hse24_reference.json")
        fixture = json.loads(path.read_text())
        with self.assertRaises(EvidenceError):
            validate_station({}, fixture)

    def test_new_protocols_require_positive_amount_of_real_work(self):
        for kind, mode in (("can", "classic-irq"), ("eth", "raw-rmii"),
                           ("usb_device", "device-fs"),
                           ("timer_capture", "edge-capture"),
                           ("advanced_pwm", "complementary-pwm")):
            self.add(kind, mode)
        for name, metric in (("uart_rx_dma", "received_bytes"),
                             ("uart_direction_flow", "flow_cases"),
                             ("adc_continuous", "sample_count"),
                             ("can_bus", "frame_count"),
                             ("ethernet_raw", "frame_count"),
                             ("usb_device", "packet_count"),
                             ("timer_capture", "edge_count"),
                             ("advanced_pwm_break", "break_cases")):
            with self.subTest(protocol=name):
                workload = self.fixture.reviewed((name,))
                limits = workload["cases"][0]["limits"]
                self.assertIn(metric, limits)
                limits[metric]["minimum"] = 0
                with self.assertRaises(EvidenceError):
                    self.fixture.validate(workload)

    def test_threadx_does_not_inherit_freertos_hardware_workload_contract(self):
        self.fixture.resolved["backend"] = "threadx"
        with self.assertRaises(EvidenceError):
            self.fixture.plan()


class HilReferenceAdmissionTests(unittest.TestCase):
    REFERENCES = (
        ("stm32g474re_hse24_reference", "stm32g474re-hse24-reference",
         "STM32G474RET6"),
        ("stm32h743_nucleo_zi2", "stm32h743-nucleo-zi2", "STM32H743ZIT6"),
        ("stm32u575_nucleo_zi_q", "stm32u575-nucleo-zi-q", "STM32U575ZIT6Q"),
    )

    def references(self):
        for name, board, part in self.REFERENCES:
            path = Path("tools/hil/fixtures", name + ".json")
            yield name, board, part, json.loads(path.read_text())

    def test_reference_identity_matches_current_exact_board_facts(self):
        for name, board, part, fixture in self.references():
            with self.subTest(board=board):
                facts = json.loads(Path("boards", name, "board.json").read_text())
                self.assertEqual((fixture["board"], fixture["part"]),
                                 (board, part))
                self.assertEqual((facts["id"], facts["soc"]), (board, part))
                self.assertIsNone(facts["physical_pcb_revision"])
                validate_fixture(fixture)

    def test_reference_preparation_retains_unbound_scope_and_null_budgets(self):
        for _, board, part, fixture in self.references():
            with self.subTest(board=board):
                serial = fixture["serial"]
                resolved = {
                    "board": board, "part": part, "backend": "baremetal",
                    "controllers": [{
                        "id": "console", "kind": "uart", "mode": "irq-bytes",
                        "controller": serial["controller"], "baud": 115200,
                        "pins": [{"function": key, "pin": serial[key]}
                                 for key in ("tx", "rx")]}],
                }
                report = preparation_plan(fixture, resolved)
                self.assertEqual(report["status"], "prepared_unbound")
                self.assertEqual(report["physical_status"], "not_executed")
                self.assertNotIn("station_id", report)
                self.assertEqual(fixture["scope"], "software_reference")
                for case in report["pending_qualification"]:
                    self.assertIsNone(case["budget"])
                    self.assertEqual(case["status"], "not_executed")

    def test_reference_scope_cannot_be_deleted_to_promote_a_fixture(self):
        for _, board, _, fixture in self.references():
            fixture.pop("scope")
            with self.subTest(board=board), self.assertRaises(EvidenceError):
                validate_fixture(fixture)

    def test_reference_board_cannot_be_promoted_by_substituting_a_part(self):
        for _, board, _, fixture in self.references():
            fixture.pop("scope")
            fixture["part"] = "STM32F407ZGT6"
            with self.subTest(board=board), self.assertRaises(EvidenceError):
                validate_fixture(fixture)

    def test_reference_part_cannot_be_relabelled_as_another_board(self):
        for _, _, part, fixture in self.references():
            fixture["board"] = "source-relabelled-board"
            with self.subTest(part=part), self.assertRaises(EvidenceError):
                validate_fixture(fixture)

    def test_direct_station_validation_cannot_bypass_reference_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "synthetic-observation.txt")
            path.write_text("Synthetic admission input; no equipment operated.\n")
            identity = file_identity(path)
            for _, board, part, fixture in self.references():
                station = {
                    "schema_version": 1, "kind": "hil_station",
                    "station_id": "synthetic-station", "board": board,
                    "part": part, "pcb_revision": "synthetic-pcb",
                    "chip_uid": "synthetic-uid", "outputs_disconnected": True,
                    "probe": {
                        "serial": "synthetic-probe", "transport": "swd",
                        "interface": fixture["probe_interfaces"][0],
                        "tool": identity, "interface_config": identity,
                        "target_config": identity},
                    "serial": {
                        "path": "/dev/serial/by-id/synthetic-input",
                        "baudrate": 115200, "electrical_mode": "ttl_3v3",
                        "wiring_checked": True, "ground_connected": True},
                    "power": {"source": "synthetic-supply", "voltage_mv": 3300},
                    "budgets": {"latency": {"maximum": 100, "unit": "us"}},
                }
                # Establish that the external station fields reach admission.
                control = copy.deepcopy(fixture)
                control.pop("scope")
                control.update(board="synthetic-f407", part="STM32F407ZGT6")
                baseline = dict(station, board=control["board"],
                                part=control["part"])
                validate_station(baseline, control)
                fixture.pop("scope")
                with self.subTest(board=board), self.assertRaises(EvidenceError):
                    validate_station(station, fixture)


class HilReferenceRawValidationTests(unittest.TestCase):
    REFERENCES = HilReferenceAdmissionTests.REFERENCES

    def references(self):
        return HilReferenceAdmissionTests.references(self)

    def setUp(self):
        self.fixture = test_hil_admission.HilAdmissionTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def changed_admission(self, baseline, fixture):
        admission = copy.deepcopy(baseline)
        admission.update(board=fixture["board"], part=fixture["part"])
        return admission

    def baseline(self):
        admission, _ = self.fixture.prepared()
        report = validate_raw(self.fixture.raw_record(admission), admission)
        self.assertEqual(report["physical_status"],
                         "qualified_for_recorded_scope")
        return admission

    def assert_raw_rejected(self, admission):
        raw = self.fixture.raw_record(admission)
        self.assertEqual(raw["admission_sha256"], canonical_digest(admission))
        with self.assertRaises(EvidenceError):
            validate_raw(raw, admission)

    def test_direct_raw_validator_rejects_real_software_reference_fixtures(self):
        baseline = self.baseline()
        for name, board, _, fixture in self.references():
            admission = self.changed_admission(baseline, fixture)
            admission["fixture"] = file_identity(Path(
                "tools/hil/fixtures", name + ".json"))
            with self.subTest(board=board):
                self.assert_raw_rejected(admission)

    def test_direct_raw_validator_rejects_rehashed_scope_deletion(self):
        baseline = self.baseline()
        for name, board, _, fixture in self.references():
            admission = self.changed_admission(baseline, fixture)
            fixture.pop("scope")
            path = self.fixture.root / (name + ".json")
            atomic_json(path, fixture)
            admission["fixture"] = file_identity(path)
            with self.subTest(board=board):
                self.assert_raw_rejected(admission)

    def test_direct_raw_validator_rejects_admission_only_board_part_relabel(self):
        baseline = self.baseline()
        for _, board, _, fixture in self.references():
            admission = self.changed_admission(baseline, fixture)
            with self.subTest(board=board):
                self.assert_raw_rejected(admission)


if __name__ == "__main__":
    unittest.main()
