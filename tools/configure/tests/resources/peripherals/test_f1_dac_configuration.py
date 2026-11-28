"""STM32F103ZE selects one analog pad under the exclusive DAC owner."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


class F103DacConfigurationTests(unittest.TestCase):
    fixtures = gate.ROOT / "tests/integration/assemblies/stm32f1_dac"

    def test_direct_channel_two_claims_only_pa5_and_the_dac(self):
        selected = gate.resolve_ir(self.fixtures / "f103ze-dac2.toml")
        port, = selected.controllers
        self.assertEqual(("DAC", (2,)), (port.controller, port.options.channels))
        self.assertEqual({"controller:DAC", "pin:PA5"},
                         {claim.key for claim in selected.claims
                          if claim.owner == "output"})

    def test_channels_cannot_cross_wiring_or_share_the_dac(self):
        with tempfile.TemporaryDirectory() as temporary:
            assembly = Path(temporary) / "dac.toml"
            source = (self.fixtures / "f103ze-dac2.toml").read_text()
            source = source.replace('board_package = "ze-board"',
                                    f'board_package = "{self.fixtures / "ze-board"}"')
            assembly.write_text(source.replace("channels = [2]", "channels = [1]"))
            with self.assertRaisesRegex(ValueError, "Board-authorized analog pads"):
                gate.resolve_ir(assembly)
            other = (source.split("[dac.output]", 1)[1]
                     .split("[factory]", 1)[0]
                     .replace('binding = "dac2"', 'binding = "dac1"')
                     .replace("channels = [2]", "channels = [1]"))
            assembly.write_text(source + "\n[dac.other]" + other)
            with self.assertRaisesRegex(ValueError, "controller:DAC"):
                gate.resolve_ir(assembly)
