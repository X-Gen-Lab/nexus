"""Named System clock profiles have reviewed domains and select no PLL."""
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.configure import configure
from tools.configure.domain_policies import DomainPolicies


class FixedClockPolicyTests(unittest.TestCase):
    def test_named_profiles_publish_only_reviewed_domains(self):
        policy = DomainPolicies.from_validated(None, {}, {"domains": {
            "schema_version": 1,
            "clock": {"model": "fixed-profile", "reviews": {
                "lab-clock": ["internal8"], "other-clock": ["internal4"]}},
        }}).clock
        facts = {"source": "hsi", "input_hz": 8000000, "core_hz": 8000000,
                 "apb1_hz": 4000000, "apb1_timer_hz": 8000000}
        plan = policy.plan("internal8", facts)
        policy.validate_clock_policy("lab-clock", plan)
        self.assertEqual(plan["input"]["source"], "hsi")
        self.assertEqual(plan["domains"]["apb1"]["nominal_hz"], 4000000)
        self.assertEqual(plan["domains"]["apb1_timer"]["nominal_hz"], 8000000)
        self.assertNotIn("apb2", plan["domains"])
        self.assertNotIn("parameters", plan)
        self.assertFalse(plan["physical_qualified"])
        with self.assertRaisesRegex(ValueError, "outside"):
            policy.validate_clock_policy("other-clock", plan)
        with self.assertRaisesRegex(ValueError, "Unreviewed"):
            policy.plan("custom", facts)
        with self.assertRaisesRegex(ValueError, "clock domain"):
            policy.validate_consumers(plan, (SimpleNamespace(controller="BUS"),),
                                      controller_domains={"BUS": "apb2"})

    def test_watchdog_route_requires_reviewed_run_clock(self):
        assemblies = configure.ROOT / "tests/integration/assemblies"
        watchdog = assemblies / "stm32f1_watchdog"
        basic = configure.resolve_ir(watchdog / "rb.toml", configure.ROOT)
        self.assertEqual(basic.controllers[0].kind, "watchdog")
        f1 = assemblies / "stm32f1_family"
        board = json.loads((f1 / "rb-mco-board/board.json").read_text())
        hsi_board = json.loads((watchdog / "software-board/board.json").read_text())
        board["bindings"].extend(hsi_board["bindings"])
        source = (f1 / "f103rb-mco-baremetal.toml").read_text()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "board").mkdir()
            (root / "board/board.json").write_text(json.dumps(board))
            assembly = root / "watchdog.toml"
            assembly.write_text(source.replace(
                'board_package = "rb-mco-board"', 'board_package = "board"') +
                '\n[watchdog.iwdg]\nbinding = "iwdg"\nmode = "independent"\n')
            with self.assertRaisesRegex(ValueError, "requires clock profile hsi8"):
                configure.resolve_ir(assembly, configure.ROOT)
