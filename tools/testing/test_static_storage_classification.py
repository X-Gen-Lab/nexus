"""Pure classification checks; synthetic symbols are not linked ARM evidence."""
from types import SimpleNamespace
import unittest

from tools.measurement.static_storage import inspect_storage


class GeneratedStorageClassificationTests(unittest.TestCase):
    def observe(self, declarations):
        """Use parsed-symbol shaped input, without compiling a test image."""
        symbols = []
        cursor = 0x20000000
        for name, size, alias in declarations:
            address = cursor if alias is None else symbols[alias]['value']
            symbols.append({'name': name, 'size': size, 'type': 1,
                            'section': 1, 'value': address})
            if alias is None:
                cursor += size
        elf = SimpleNamespace(
            sections=[{'name': '.symtab', 'size': 16, 'flags': 0},
                      {'name': '.bss', 'address': 0x20000000,
                       'size': 16384, 'flags': 3}],
            defined_symbols=symbols)
        metrics = {'ram': {'sram': {'ranges': [[0x20000000, 0x20004000]],
                                   'reserved_bytes': 16384}},
                   'reservations': {'msp': {'bytes': 0},
                                    'heap': {'bytes': 0}}}
        return inspect_storage(elf, metrics)

    def test_enhanced_rings_and_slots_have_disjoint_existing_cost_classes(self):
        report = self.observe((
            ('s_nx_eth_frames_tx_mac', 256, None),
            ('s_nx_eth_frames_rx_mac', 256, None),
            ('s_nx_eth_frames_storage_mac', 2048, None),
            ('s_nx_eth_frames_tx_slots_mac', 64, None),
            ('s_nx_eth_frames_rx_slots_mac.lto_priv.7', 96, None),
            ('s_nx_port_mac', 32, None)))
        self.assertEqual(report['bytes']['buffers'], 2560)
        self.assertEqual(report['bytes']['provider_state'], 192)
        self.assertEqual(report['bytes']['other_static'], 0)
        self.assertEqual(sum(report['bytes'].values()), 2752)
        self.assertEqual(report['unattributed_reserved_bytes'], 16384 - 2752)
        self.assertEqual(report['dynamic_stack_high_water'], 'not_measured')

    def test_old_and_enhanced_exact_buffer_aliases_still_count_once(self):
        report = self.observe((
            ('s_nx_eth_tx_mac', 32, None),
            ('s_nx_eth_frames_tx_alias', 32, 0),
            ('s_nx_eth_rx_slots_mac', 40, None)))
        self.assertEqual(report['bytes']['buffers'], 32)
        self.assertEqual(report['bytes']['provider_state'], 40)
        self.assertEqual(report['bytes']['other_static'], 0)
        self.assertEqual(sum(report['bytes'].values()), 72)

    def test_unrecognized_product_storage_is_not_guessed_from_eth_prefix(self):
        report = self.observe((
            ('s_nx_eth_frames_payload_mac', 20, None),
            ('product_s_nx_eth_frames_tx_mac', 24, None),
            ('s_nx_eth_storage_old', 64, None)))
        self.assertEqual(report['bytes']['other_static'], 44)
        self.assertEqual(report['bytes']['buffers'], 64)
        self.assertEqual(report['bytes']['provider_state'], 0)
        self.assertEqual(sum(report['bytes'].values()), 108)


if __name__ == '__main__':
    unittest.main()
