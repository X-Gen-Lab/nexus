"""Keep the exact Board wake input as a single resolved resource claim."""
import json
import tempfile
from pathlib import Path
import unittest

from tools.configure import configure as gate

FIXTURE = (gate.ROOT / "tests/integration/assemblies/stm32f1_standby" /
           "f103rb-pa0.toml")


class F103StandbyConfigTests(unittest.TestCase):
    def test_exact_wake_choice_and_pad_owner_reach_the_build(self):
        ir = gate.resolve_ir(FIXTURE)
        self.assertEqual((ir.part, ir.backend, ir.clock_profile),
                         ("STM32F103RBT6", "baremetal", "hsi8"))
        claims = {item.key: item for item in ir.claims}
        self.assertEqual((claims["pin:PA0"].owner, claims["pin:PA0"].mode),
                         ("standby-pa0", "reserved"))
        self.assertFalse(ir.physical_qualified)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "configured"
            gate.configure(FIXTURE, output)
            generated = (output / "selection.cmake").read_text()
            self.assertIn("standby_wakeup=pa0-wkup", generated)
            self.assertIn("pin:PA0=standby-pa0", generated)

    def test_omitting_hardware_choices_overrides_stale_build_values(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assembly = root / "unselected.toml"
            text = FIXTURE.read_text().replace(
                'board_package = "board"',
                'board_package = ' + json.dumps(str(FIXTURE.parent / 'board')))
            section = '[hardware]\nstandby_wakeup = "pa0-wkup"\n\n'
            self.assertIn(section, text)
            assembly.write_text(text.replace(section, ''))
            output = root / "configured"
            gate.configure(assembly, output)
            generated = (output / "selection.cmake").read_text()
            self.assertIn('set(NEXUS_SELECTED_BOARD_HARDWARE "")', generated)
            self.assertIn('set(NEXUS_BOARD_RESERVED_PINS "")', generated)
