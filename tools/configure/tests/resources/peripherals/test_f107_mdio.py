"""Verify the exact station-management ownership and selected public face."""

from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate, selected_contracts
from tools.configure.generation import factory


FIXTURE = gate.ROOT / 'tests/integration/assemblies/stm32f1_mdio'
ASSEMBLY = FIXTURE / 'f107vc.toml'


class F107MdioTests(unittest.TestCase):
    def test_standalone_factory_owns_only_mdc_mdio_and_mac(self):
        result = gate.resolve_ir(ASSEMBLY)
        selected, = result.controllers
        classes, = factory.classes(result)
        self.assertEqual(classes, ('mdio', 'port', [selected]))
        capability, = selected_contracts.for_configuration(result)
        self.assertEqual((capability.public_class, capability.features,
                          capability.word_bits, capability.max_transfer_units),
                         ('mdio', ('POLLING',), (16,), 1))
        claims = {entry.key for entry in result.resource_plan.claims
                  if entry.owner == 'phy'}
        self.assertEqual(claims, {'controller:ETH', 'peripheral:ETH',
                                  'pin:PC1', 'pin:PA2'})

    def test_rejects_missing_board_permission_wrong_clock_and_second_owner(self):
        original = ASSEMBLY.read_text().replace('"board"', f'"{FIXTURE / "board"}"')
        mutations = (
            (original.replace('"pc1-pa2-external-phy"', '"unknown"'),
             'requires'),
            (original.replace('"hse25-crystal-pll72"', '"hsi8"'),
             'clock'),
            (original + '\n[mdio.second]\nbinding = "station-management"\n'
             'mode = "clause22"\n', 'Resource conflict'),
        )
        with tempfile.TemporaryDirectory() as temporary:
            assembly = Path(temporary) / 'mdio.toml'
            for authored, diagnostic in mutations:
                with self.subTest(diagnostic=diagnostic), self.assertRaisesRegex(
                        gate.ConfigurationError, diagnostic):
                    assembly.write_text(authored)
                    gate.resolve_ir(assembly)
