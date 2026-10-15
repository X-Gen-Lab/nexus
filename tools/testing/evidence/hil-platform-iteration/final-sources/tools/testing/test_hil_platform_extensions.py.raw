"""Synthetic current-mode HIL contracts never operate a physical station."""

import copy
import json
from pathlib import Path
import unittest

from tools.evidence.common import EvidenceError
from tools.hil.admit import validate_fixture, validate_station
from tools.testing import test_hil_workloads


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
        self.assertEqual(len(report["cases"]), 21)
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


if __name__ == "__main__":
    unittest.main()
