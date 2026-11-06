"""Exact Qiming preparation keeps fitting declarations separate from equipment."""
import copy
import json
from pathlib import Path
import unittest

from tools.evidence.common import EvidenceError
from tools.hil.admit import validate_fixture, validate_station
from tools.hil.prepare import preparation_plan


ROOT = Path(__file__).resolve().parents[2]
BOARDS = ("stm32f407zg-qiming-core", "stm32f407zg-qiming-v41")


class QimingHilPreparationTests(unittest.TestCase):
    def setUp(self):
        self.fixture = json.loads((ROOT /
            "tools/hil/fixtures/stm32f407_qiming_v31.json").read_text())
        self.resolved = {"board": self.fixture["board"],
                         "part": self.fixture["part"], "backend": "baremetal",
                         "controllers": [{"id": "console", "kind": "uart",
                             "controller": "USART1", "mode": "irq-byte-event",
                             "baud": 115200, "pins": [
                                 {"function": "tx", "pin": "PA9"},
                                 {"function": "rx", "pin": "PA10"}]}]}

    def test_unbound_plan_retains_exact_declarations_without_observations(self):
        self.fixture["board"] = BOARDS[0]
        self.fixture["scope"] = "software_reference"
        self.resolved["board"] = BOARDS[0]
        hardware = {"options": {"debug_port": ["swd", "jtag"],
                                "display_module": ["absent", "ili9341-16bit"]},
                    "selected": {"debug_port": "swd"},
                    "physical_verification":
                        "not_established_by_configuration"}
        self.resolved["hardware"] = hardware
        report = preparation_plan(self.fixture, self.resolved)
        self.assertEqual(report["hardware"], hardware)
        self.assertEqual(report["physical_status"], "not_executed")
        self.assertEqual(report["status"], "prepared_unbound")
        self.assertNotIn("display_module", report["hardware"]["selected"])
        self.assertTrue(any("population" in item and "jumpers" in item
                            for item in report["station_prerequisites"]))
        report["hardware"]["selected"]["debug_port"] = "jtag"
        self.assertEqual(self.resolved["hardware"]["selected"]["debug_port"],
                         "swd")

    def test_new_exact_boards_cannot_drop_reference_scope_or_change_part(self):
        for board in BOARDS:
            with self.subTest(board=board):
                self.fixture["board"] = board
                self.fixture.pop("scope", None)
                with self.assertRaisesRegex(EvidenceError, "reference"):
                    validate_fixture(self.fixture)
                self.fixture["scope"] = "software_reference"
                self.fixture["part"] = "STM32F407VET6"
                with self.assertRaisesRegex(EvidenceError, "identity"):
                    validate_fixture(self.fixture)
                self.fixture["part"] = "STM32F407ZGT6"

    def test_qiming_fixture_templates_prepare_but_never_admit_equipment(self):
        for board, filename in zip(BOARDS,
                ("stm32f407zg_qiming_core.json", "stm32f407zg_qiming_v41.json")):
            with self.subTest(board=board):
                fixture = json.loads((ROOT / "tools/hil/fixtures" /
                                      filename).read_text())
                validate_fixture(fixture)
                self.assertEqual(fixture["board"], board)
                self.assertEqual(fixture["scope"], "software_reference")
                self.assertTrue(all(item["budget"] is None
                                    for item in fixture["pending_qualification"]))
                self.resolved["board"] = board
                report = preparation_plan(fixture, self.resolved)
                self.assertEqual(report["physical_status"], "not_executed")
                with self.assertRaisesRegex(EvidenceError, "reference"):
                    validate_station({}, fixture)

    def test_old_v31_same_part_does_not_gain_a_reference_only_restriction(self):
        validate_fixture(self.fixture)
        with self.assertRaises(EvidenceError) as result:
            validate_station({}, self.fixture)
        self.assertNotIn("software-reference", str(result.exception))
        self.assertNotIn("hardware", preparation_plan(self.fixture,
                                                       self.resolved))

    def test_preparation_rejects_fake_observation_or_invalid_declared_choice(self):
        valid = {"options": {"debug_port": ["swd", "jtag"]},
                 "selected": {"debug_port": "swd"},
                 "physical_verification": "not_established_by_configuration"}
        for change in ({"physical_verification": "passed"},
                       {"selected": {"debug_port": "automatic"}},
                       {"selected": None},
                       {"observed_jumpers": "passed"}, {"options": []}):
            with self.subTest(change=change):
                self.resolved["hardware"] = {**copy.deepcopy(valid), **change}
                with self.assertRaises((EvidenceError, ValueError)):
                    preparation_plan(self.fixture, self.resolved)

    def test_board_only_schema_lock_does_not_ban_the_shared_part(self):
        from tools.hil.admit import SOFTWARE_REFERENCE_BOARD_TARGETS
        schema = json.loads((ROOT /
            "tools/schemas/hil-fixture.schema.json").read_text())
        targets = schema["$defs"]["software_reference_board_target"]["oneOf"]
        self.assertEqual({(item["properties"]["board"]["const"],
                           item["properties"]["part"]["const"])
                          for item in targets},
                         set(SOFTWARE_REFERENCE_BOARD_TARGETS))
        rule = schema["allOf"][-1]
        self.assertEqual(set(rule["if"]["properties"]), {"board"})
        self.assertEqual(rule["then"]["required"], ["scope"])


if __name__ == "__main__":
    unittest.main()
