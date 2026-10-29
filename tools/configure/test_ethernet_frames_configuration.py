"""Normal typed SG/VLAN/PTP selection; real provider tests execute hardware."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import tomllib
import unittest

from tools.configure import bindings, configure, factory, selected_contracts


class EthernetFramesConfiguration(unittest.TestCase):
    def fixture(self, family):
        return configure.ROOT / ('cmake/package/verification/io_evolution/'
                                 + family + '-eth-frames.toml')

    def resolve(self, family, changes=(), board_change=None):
        fixture = self.fixture(family)
        content = fixture.read_text()
        package = tomllib.loads(content)['board_package']
        board_path = (fixture.parent / package / 'board.json').resolve()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            board = json.loads(board_path.read_text())
            if board_change:
                board_change(board)
            (path / 'board.json').write_text(json.dumps(board))
            content = content.replace('board_package = "' + package + '"',
                                      'board_package = "."')
            for before, after in changes:
                self.assertIn(before, content)
                content = content.replace(before, after)
            assembly = path / 'assembly.toml'
            assembly.write_text(content)
            return configure.resolve_ir(assembly)

    def test_exact_zg_parts_freeze_storage_and_derive_a_counter_domain(self):
        for family, part, controller in (
                ('f407zg', 'STM32F407ZGT6', 'ETH'),
                ('gd470', 'GD32F470ZGT6', 'ENET')):
            with self.subTest(family=family):
                result = self.resolve(family)
                item, = result.controllers
                self.assertEqual(result.part, part)
                self.assertEqual((item.kind, item.controller, item.mode),
                                 ('eth_frames', controller,
                                  'rmii-vlan-sg-ptp'))
                options = item.options
                self.assertEqual((options.tx_count, options.rx_count,
                                  options.rx_storage_bytes), (8, 8, 256))
                self.assertEqual(options.address, (2, 0, 0, 0, 0, 1))
                self.assertGreater(options.clock_domain, 0)
                self.assertLessEqual(options.clock_domain, 0xffffffff)
                with self.assertRaises(FrozenInstanceError):
                    options.clock_domain = 5
                self.assertFalse(result.physical_qualified)

    def test_factory_uses_distinct_frame_face_and_paired_mdio_without_io(self):
        for family in ('f407zg', 'gd470'):
            with self.subTest(family=family):
                header, source = factory.emit(self.resolve(family))
                self.assertIn('const nx_eth_frames_port_t* '
                              'nx_factory_eth_frames(', header)
                self.assertIn('NX_ETH_FRAMES_ID_MAC', header)
                self.assertIn('NX_ETH_ID_COUNT = 0', header)
                self.assertIn('NX_MDIO_ID_MAC', header)
                self.assertIn('nexus/io/ethernet_frames.h', header)
                self.assertIn('return nx_binding_mac;', source)
                self.assertIn('return nx_binding_mac_mdio;', source)
                self.assertNotIn('initialize(', source)
                self.assertNotIn('NX_PLATFORM_REGISTER_WRITE', source)

    def test_enhanced_storage_ops_epochs_and_actual_private_constructor(self):
        for family, prefix in (('f407zg', 'stm32'), ('gd470', 'gd32')):
            with self.subTest(family=family):
                source = bindings.emit(self.resolve(family))[1]
                for direction in ('tx', 'rx'):
                    self.assertIn('nx_eth_frames_descriptor_t '
                                  's_nx_eth_frames_' + direction
                                  + '_mac[8]', source)
                self.assertIn('s_nx_eth_frames_storage_mac[8][256]', source)
                self.assertIn('nx_eth_frames_mac_t s_nx_port_mac;', source)
                self.assertIn('&nx_eth_frames_mac_ops, &s_nx_port_mac', source)
                self.assertIn('&nx_eth_frames_mdio_ops, &s_nx_port_mac', source)
                self.assertIn(f'nx_{prefix}_eth_frames_cold_prepare(', source)
                self.assertIn(f'nx_{prefix}_eth_frames_cold_close(', source)
                self.assertIn(f'static const nx_{prefix}_eth_frames_cold_config_t',
                              source)
                self.assertIn('.clock_domain = 1u', source)
                self.assertIn(f'nx_{prefix}_eth_resources_capture('
                              '&s_nx_eth_resources, true)', source)
                self.assertNotIn(f'nx_{prefix}_eth_frames_initialize(', source)
                self.assertNotIn('s_nx_port_mac.rx_epoch', source)
                self.assertNotIn('s_nx_port_mac.clock_epoch', source)
                self.assertIn('nx_eth_frames_mac_irq(&s_nx_port_mac)', source)
                self.assertNotIn('static nx_eth_descriptor_t ', source)
                self.assertNotIn('dp83848', source.lower())

    def test_actual_selected_traits_keep_frame_units_and_optional_stamps(self):
        for family in ('f407zg', 'gd470'):
            with self.subTest(family=family):
                records = selected_contracts.for_configuration(
                    self.resolve(family))
                frame, = [row for row in records
                          if row.public_class == 'eth_frames']
                mdio, = [row for row in records if row.public_class == 'mdio']
                self.assertEqual((frame.unit, frame.unit_bytes,
                                  frame.buffer_alignment), ('bytes', 1, 4))
                self.assertEqual(frame.max_transfer_units,
                                 'NX_ETH_FRAME_MAX + 4U')
                self.assertEqual(frame.max_segments, 8)
                self.assertEqual(frame.fields['vlan'], 1)
                self.assertEqual(frame.fields['scatter_gather'], 1)
                self.assertEqual(frame.timestamp_source, 'hardware_event')
                self.assertEqual(frame.abort_scope, 'controller_pending')
                self.assertEqual(frame.dma_engines, ())
                self.assertIn('BUS_MASTER_DMA', frame.features)
                self.assertEqual((mdio.unit, mdio.unit_bytes), ('registers', 2))
                self.assertFalse(frame.physical_qualified)

    def test_sg_storage_requires_real_strides_and_complete_rx_capacity(self):
        for family in ('f407zg', 'gd470'):
            for before, after in (
                    ('tx_count = 8', 'tx_count = 0'),
                    ('rx_count = 8', 'rx_count = true'),
                    ('rx_count = 8', 'rx_count = 9999999'),
                    ('rx_storage_bytes = 256', 'rx_storage_bytes = 60'),
                    ('rx_storage_bytes = 256', 'rx_storage_bytes = 255'),
                    ('rx_storage_bytes = 256', 'rx_storage_bytes = 8192'),
                    ('rx_count = 8', 'rx_count = 5'),
                    ('init_timeout_us = 5000', 'init_timeout_us = 0'),
                    ('drain_timeout_us = 1000', 'drain_timeout_us = 0')):
                with self.subTest(family=family, after=after):
                    with self.assertRaisesRegex(ValueError, 'Ethernet'):
                        self.resolve(family, ((before, after),))

    def test_observed_clock_or_phy_qualification_cannot_be_authored(self):
        for family in ('f407zg', 'gd470'):
            for option in ('clock_domain = 1', 'clock_epoch = 1',
                           'hardware_qualified = true', 'phy_address = 0'):
                with self.subTest(family=family, option=option):
                    with self.assertRaises(ValueError):
                        self.resolve(family, (('tx_count = 8',
                                               'tx_count = 8\n' + option),))

    def test_mode_and_address_cannot_silently_fall_back_to_raw_ethernet(self):
        for family in ('f407zg', 'gd470'):
            for before, after in (
                    ('mode = "rmii-vlan-sg-ptp"', 'mode = "raw-rmii"'),
                    ('address = [2, 0, 0, 0, 0, 1]',
                     'address = [1, 0, 0, 0, 0, 1]'),
                    ('address = [2, 0, 0, 0, 0, 1]',
                     'address = [0, 0, 0, 0, 0, 0]')):
                with self.subTest(family=family, after=after):
                    with self.assertRaises(ValueError):
                        self.resolve(family, ((before, after),))

    def test_unreviewed_f407ve_cannot_inherit_zg_port_g_route(self):
        with self.assertRaisesRegex(ValueError, 'reviewed exact-part route'):
            self.resolve('f407zg', board_change=lambda board:
                         board.update(soc='STM32F407VET6'))

    def test_two_frame_owners_conflict_on_one_physical_controller(self):
        for family in ('f407zg', 'gd470'):
            fixture = self.fixture(family).read_text()
            _, instance = fixture.split('[eth_frames.mac]', 1)
            extra = ('\n[eth_frames.other]' + instance.replace(
                '[eth_frames.mac.irq]', '[eth_frames.other.irq]'))
            with self.subTest(family=family):
                with self.assertRaisesRegex(ValueError, 'Resource conflict'):
                    self.resolve(family, (('calls_os = false',
                                           'calls_os = false' + extra),))

    def test_default_and_ordinary_raw_modes_do_not_gain_enhanced_state(self):
        default = configure.resolve_ir(configure.ROOT /
                    'tools/configure/assemblies/qiming-baremetal.toml')
        self.assertNotIn('s_nx_eth_frames_', bindings.emit(default)[1])
        for label in ('stm32-zg', 'gd32'):
            with self.subTest(label=label):
                result = configure.resolve_ir(configure.ROOT /
                    ('tests/contracts/ethernet_assembly/' + label + '.toml'))
                record, = [row for row in
                            selected_contracts.for_configuration(result)
                            if row.public_class == 'eth']
                self.assertEqual(record.max_transfer_units,
                                 'NX_ETH_FRAME_MAX')
                self.assertEqual(record.fields['vlan'], 0)
                self.assertEqual(record.fields['scatter_gather'], 0)
                self.assertEqual(record.timestamp_source,
                                 'acquire_observation')
                source = bindings.emit(result)[1]
                self.assertNotIn('nx_eth_frames_mac_t', source)
                self.assertNotIn('0x1e000000u', source)


if __name__ == '__main__':
    unittest.main()
