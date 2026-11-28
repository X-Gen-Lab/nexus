"""Admit the exact F103RB retained backup mode and its exclusive resources."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure import selected_contracts


FIXTURE = gate.ROOT / "tests/integration/assemblies/stm32f1_rtc"
F4_PEER = gate.ROOT / "tests/integration/assemblies/rtc_bypass_assembly/adopt-retained.toml"


class F103RtcConfigurationTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="f103 rtc assembly ")
        self.addCleanup(temp.cleanup)
        self.directory = Path(temp.name)

    def resolve(self, *, extra="", board_clocks=None, backup_options=""):
        board_dir = self.directory / "board"
        board_dir.mkdir(exist_ok=True)
        board = json.loads((FIXTURE / "board/board.json").read_text())
        if board_clocks is not None:
            board["clocks"] = board_clocks
        (board_dir / "board.json").write_text(json.dumps(board))
        assembly = self.directory / "assembly.toml"
        text = (FIXTURE / "rb.toml").read_text()
        assembly.write_text(text.replace('mode = "backup-adopt"',
                                         'mode = "backup-adopt"' + backup_options) + extra)
        return gate.resolve_ir(assembly)

    def test_backup_read_capability_does_not_claim_an_alarm(self):
        result = self.resolve()
        rtc, = selected_contracts.for_configuration(result)
        self.assertEqual((rtc.kind, rtc.mode, rtc.public_class),
                         ("rtc", "backup-adopt", "rtc"))
        self.assertEqual((rtc.unit, rtc.unit_bytes, rtc.max_transfer_units),
                         ("words", 2, 1))
        self.assertEqual(rtc.word_bits, (16,))
        self.assertEqual(rtc.features, ("POLLING",))
        self.assertFalse(rtc.physical_qualified)
        resources = {claim.key for claim in result.resource_plan.claims}
        self.assertTrue({"pwr:control", "domain:backup", "clock:LSE",
                         "clock:RTC", "pin:PC14", "pin:PC15"} <= resources)
        self.assertNotIn("alarm:RTC_A", resources)

    def test_shared_pwr_control_rejects_pvd_and_backup_adoption(self):
        with self.assertRaisesRegex(gate.ConfigurationError, r"pwr:control"):
            self.resolve(extra='\n[exti.supply]\nbinding = "pvd"\n'
                               'mode = "pvd-level"\nthreshold_code = 5\n')

    def test_only_declared_lse_crystal_and_read_only_options(self):
        for clocks in ({}, {"lse_hz": 32000}, {"lse_hz": 32768, "lse_bypass": True}):
            with self.subTest(clocks=clocks), self.assertRaisesRegex(
                    gate.ConfigurationError, "RTC.*LSE|RTC bypass"):
                self.resolve(board_clocks=clocks)
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "does not accept initialization options"):
            self.resolve(backup_options="\nallow_set = true")

    def test_peer_f4_adoption_keeps_calendar_alarm_contract(self):
        contracts = selected_contracts.for_configuration(gate.resolve_ir(F4_PEER))
        rtc = next(item for item in contracts if item.kind == "rtc")
        self.assertEqual((rtc.mode, rtc.unit, rtc.max_transfer_units),
                         ("adopt-retained", "occurrences", 1))


class F103RtcProvisioningConfigurationTests(unittest.TestCase):
    def test_provision_retains_read_only_backup_and_claims_its_dormant_irqs(self):
        result = gate.resolve_ir(FIXTURE / "provision.toml")
        selected, = result.controllers
        self.assertEqual(selected.mode, "backup-provision")
        self.assertIsNone(selected.options.initial)
        self.assertEqual((selected.options.timeout_us, selected.options.poll_limit),
                         (10000000, 8000000))
        rtc, = selected_contracts.for_configuration(result)
        self.assertEqual((rtc.unit, rtc.word_bits, rtc.features),
                         ("words", (16,), ("POLLING",)))
        resources = {claim.key for claim in result.resource_plan.claims}
        self.assertTrue({"domain:backup", "clock:LSE", "clock:RTC", "irq:RTC",
                         "irq:RTC_Alarm", "exti:line:17"} <= resources)

    def test_declared_budgets_seed_and_lse_crystal_constraints(self):
        fixture = FIXTURE / "provision.toml"
        text = fixture.read_text()
        source = json.loads((FIXTURE / "provision-board/board.json").read_text())
        cases = (
            (text.replace("timeout_us = 10000000", ""), {}, "timeout_us and poll_limit"),
            (text.replace("poll_limit = 8000000", "poll_limit = 20000001"), {}, "poll_limit"),
            (text + "\nallow_set = true", {}, "timeout_us and poll_limit only"),
            (text + "\ninitial = { year=26, month=1, day=1, weekday=1, hour=0, minute=0, second=0 }",
             {}, "timeout_us and poll_limit only"),
            (text, {"clocks": {"lse_hz": 32768, "lse_bypass": True}}, "LSE crystal"),
            (text, {"reserved_resources": ["exti:line:17"]}, "Resource conflict exti:line:17"),
        )
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            board = folder / "provision-board/board.json"
            assembly = folder / "fixture.toml"
            board.parent.mkdir()
            for authored, overrides, reason in cases:
                with self.subTest(reason=reason), self.assertRaisesRegex(
                        gate.ConfigurationError, reason):
                    board.write_text(json.dumps({**source, **overrides}))
                    assembly.write_text(authored)
                    gate.resolve_ir(assembly)


if __name__ == "__main__":
    unittest.main()
