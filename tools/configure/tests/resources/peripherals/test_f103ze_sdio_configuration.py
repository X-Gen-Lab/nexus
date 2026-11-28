"""Check the exact socket and factory contract of the four-line SDIO assembly."""

from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate, selected_contracts


FIXTURE = gate.ROOT / 'tests/integration/assemblies/stm32f1_sdio'
ASSEMBLY = FIXTURE / 'ze-4bit.toml'


class F103zeSdioConfigurationTests(unittest.TestCase):
    def test_four_lines_claim_all_data_pins_on_one_polled_block_port(self):
        result = gate.resolve_ir(ASSEMBLY)
        capability, = selected_contracts.for_configuration(result)
        self.assertEqual(
            (capability.mode, capability.public_class, capability.features,
             capability.unit_bytes, capability.max_transfer_units,
             capability.dma_engines, capability.physical_qualified),
            ('sdio-poll-4bit', 'block', ('POLLING',), 512, 1, (), False))
        claims = {entry.key for entry in result.resource_plan.claims
                  if entry.owner == 'media'}
        self.assertEqual(claims, {
            'controller:SDIO', 'pin:PC8', 'pin:PC9', 'pin:PC10', 'pin:PC11',
            'pin:PC12', 'pin:PD2', 'pin:PC13'})

    def test_four_line_write_uses_same_socket_and_one_block_limit(self):
        result = gate.resolve_ir(FIXTURE / 'ze-4bit-write.toml')
        capability, = selected_contracts.for_configuration(result)
        self.assertEqual(
            (capability.mode, capability.public_class, capability.features,
             capability.unit_bytes, capability.max_transfer_units,
             capability.dma_engines, capability.physical_qualified),
            ('sdio-poll-4bit-write', 'block', ('POLLING',), 512, 1, (), False))
        baseline = gate.resolve_ir(ASSEMBLY)
        self.assertEqual({(c.key, c.owner) for c in result.resource_plan.claims},
                         {(c.key, c.owner) for c in baseline.resource_plan.claims})

    def test_dma_reserves_fixed_channel_and_clock_without_shared_irq(self):
        result = gate.resolve_ir(FIXTURE / 'ze-4bit-rx-dma.toml')
        capability, = selected_contracts.for_configuration(result)
        self.assertEqual(
            (capability.mode, capability.features, capability.dma_engines,
             capability.unit_bytes, capability.max_transfer_units,
             capability.physical_qualified),
            ('sdio-rx-dma-poll', ('POLLING', 'RX_DMA'), ('DMA2',), 512, 1, False))
        self.assertEqual(
            {c.key for c in result.resource_plan.claims if c.owner == 'media'
             and c.key.startswith(('dma:', 'irq:', 'clock:'))},
            {'dma:DMA2:channel4', 'clock:DMA2'})

    def test_width_single_bit_socket_and_write_mode_cannot_silently_mix(self):
        original = ASSEMBLY.read_text().replace(
            '"four-bit-board"', f'"{FIXTURE / "four-bit-board"}"')
        mutations = (
            original.replace('bus_width = 4', 'bus_width = 1'),
            original.replace(str(FIXTURE / 'four-bit-board'),
                             str(FIXTURE / 'board')),
            original.replace('"sdio-poll-4bit"', '"sdio-poll-write"'),
            original.replace('"sdio-poll-4bit"', '"sdio-rx-dma-poll"')
                    .replace('bus_width = 4', 'bus_width = 1'),
        )
        with tempfile.TemporaryDirectory() as temporary:
            assembly = Path(temporary) / 'sdio.toml'
            for authored in mutations:
                with self.subTest(authored=authored), self.assertRaises(
                        gate.ConfigurationError):
                    assembly.write_text(authored)
                    gate.resolve_ir(assembly)
