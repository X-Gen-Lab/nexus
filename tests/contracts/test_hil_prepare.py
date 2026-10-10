"""Prepare station work without turning software scope into hardware evidence."""

import copy
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class HilPreparationTests(unittest.TestCase):
    def setUp(self):
        self.fixture = json.loads((ROOT / "tools/hil/fixtures/gd32f470_liangshan.json").read_text())
        self.resolved = {"board": self.fixture["board"],
                         "part": self.fixture["part"], "backend": "baremetal",
                         "controllers": [
                             {"id": "console", "kind": "uart", "mode": "irq-byte-event",
                              "controller": "USART0", "baud": 115200,
                              "pins": [{"function": "tx", "pin": "PA9"},
                                       {"function": "rx", "pin": "PA10"}]},
                             {"id": "sensor", "kind": "adc", "mode": "trigger-block",
                              "controller": "ADC0", "pins": []}]}

    def prepare(self, resolved=None):
        from tools.hil.prepare import preparation_plan
        return preparation_plan(self.fixture, resolved or self.resolved)

    def test_plan_has_selected_modes_and_unexecuted_physical_scope(self):
        plan = self.prepare()
        self.assertEqual(plan["kind"], "hil_preparation")
        self.assertEqual(plan["status"], "prepared_unbound")
        self.assertEqual(plan["physical_status"], "not_executed")
        self.assertEqual(plan["controllers"][1]["mode"], "trigger-block")
        self.assertEqual(plan["required_tests"], self.fixture["required_tests"])
        self.assertTrue(plan["unreviewed_budgets"])
        self.assertNotIn("chip_uid", plan)
        self.assertTrue(all(case["status"] == "not_executed"
                            for case in plan["pending_qualification"]))

    def test_plan_never_accepts_different_hardware_or_console_wiring(self):
        from tools.evidence.common import EvidenceError
        for change in ("board", "part", "pin", "baud"):
            resolved = copy.deepcopy(self.resolved)
            if change in ("board", "part"):
                resolved[change] = "different-hardware"
            elif change == "pin":
                resolved["controllers"][0]["pins"][0]["pin"] = "PB6"
            else:
                resolved["controllers"][0]["baud"] = 9600
            with self.subTest(change=change), self.assertRaises(EvidenceError):
                self.prepare(resolved)

    def test_missing_or_duplicate_smoke_console_is_rejected(self):
        from tools.evidence.common import EvidenceError
        for controllers in (self.resolved["controllers"][1:],
                            [self.resolved["controllers"][0]] * 2):
            resolved = {**self.resolved, "controllers": controllers}
            with self.subTest(controllers=controllers), self.assertRaises(EvidenceError):
                self.prepare(resolved)

    def test_preparation_is_repeatable_without_mutating_authored_inputs(self):
        fixture = copy.deepcopy(self.fixture)
        resolved = copy.deepcopy(self.resolved)
        self.assertEqual(self.prepare(), self.prepare())
        self.assertEqual(fixture, self.fixture)
        self.assertEqual(resolved, self.resolved)

    def test_three_station_plans_retain_dma_stream_and_wake_measurements(self):
        required = {"irq_executor_wake", "uart_rx_block", "adc_trigger_block",
                    "dma_memory_domain", "multi_controller_isolation"}
        for path in (ROOT / "tools/hil/fixtures").glob("*.json"):
            if path.name == "station.template.json":
                continue
            fixture = json.loads(path.read_text())
            with self.subTest(board=fixture["board"]):
                self.assertLessEqual(required,
                    {case["id"] for case in fixture["pending_qualification"]})


if __name__ == "__main__":
    unittest.main()
