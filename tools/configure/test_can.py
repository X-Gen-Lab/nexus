"""CAN assembly selects exact resources and immutable shared filter ownership."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import can, configure as gate


class CanConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def authored(self, second=False, operation="loopback", extra=""):
        content = ('schema=2\nboard="stm32f407_qiming_v31"\n'
                   'backend="baremetal"\nclock="hse8-pll168"\n'
                   '[factory]\ncapabilities=true\n'
                   '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                   '[can_domain]\nsplit=14\nfilters=['
                   '{controller="bus0",bank=0,id=291,mask=2047,'
                   'format="standard",fifo=0,remote=false,match_remote=true}')
        if second:
            content += (',{controller="bus1",bank=14,id=801,mask=2047,'
                        'format="standard",fifo=1,remote=false,match_remote=true}')
        content += ']\n'
        for index in range(2 if second else 1):
            content += (f'[can.bus{index}]\nbinding="can{index+1}"\n'
                        f'mode="classic-irq"\noperation="{operation}"\n'
                        'event_capacity=8\nbitrate=500000\n'
                        'drain_timeout_us=1000\npoll_limit=128\nfifo_order=true\n'
                        'timing={prescaler=6,segment1=11,segment2=2,sjw=2}\n'
                        f'[can.bus{index}.irq]\npriority=5\ncalls_os=false\n')
        path = self.root / 'assembly.toml'
        path.write_text(content + extra)
        return path

    def test_two_controllers_share_one_immutable_domain_and_distinct_exact_irqs(self):
        ir = gate.resolve_ir(self.authored(second=True))
        self.assertEqual(ir.can_domain.split, 14)
        self.assertEqual(len(ir.can_domain.filters), 2)
        with self.assertRaises(FrozenInstanceError):
            ir.controllers[0].options.timing.prescaler = 7
        claims = {item['key'] for item in ir['claims']}
        self.assertIn('can:filter-domain', claims)
        self.assertIn('can:filter-bank:0', claims)
        self.assertIn('can:filter-bank:14', claims)
        for vector in ('CAN1_TX', 'CAN1_RX0', 'CAN1_RX1', 'CAN1_SCE',
                       'CAN2_TX', 'CAN2_RX0', 'CAN2_RX1', 'CAN2_SCE'):
            self.assertIn('irq:' + vector, claims)
        self.assertFalse(any(name.startswith('dma:') for name in claims))

    def test_constructor_rejects_a_foreign_vendor_binding(self):
        for fixture, foreign in (("stm32-first.toml", "gd32"),
                                 ("gd32-first.toml", "stm32")):
            with self.subTest(fixture=fixture):
                result = gate.resolve_ir(gate.ROOT /
                    "tests/contracts/can_assembly" / fixture)
                item = next(item for item in result.controllers
                            if item.kind == "can")
                with self.assertRaisesRegex(ValueError, "provider"):
                    can.constructor(item, result, foreign)

    def test_generated_factory_and_partial_start_cleanup_are_explicit(self):
        path = self.authored(second=True)
        output = self.root / 'generated'
        gate.configure(path, output)
        source = (output / 'bindings.c').read_text()
        header = (output / 'nexus_factory.h').read_text()
        self.assertIn('nx_factory_can(', header)
        self.assertIn('&nx_classic_can_ops, &s_nx_port_bus0', source)
        self.assertEqual(source.count('static nx_can_filter_domain_t '), 1)
        self.assertIn('nx_stm32_can_domain_initialize', source)
        self.assertIn('nx_can_port_stop(&s_nx_face_bus0)', source)
        self.assertIn('nx_can_filter_domain_stop(&s_nx_can_domain)', source)
        marker = 'result.primary = prepare_bus1();'
        begin = source.index('nx_platform_start_result_t nx_platform_start')
        prior = source.rfind('s_nx_platform_initialized = 2u;', begin,
                            source.index(marker))
        self.assertGreater(prior, begin)

    def test_timing_requires_exact_apb_bitrate_and_valid_sampling_segments(self):
        for previous, replacement in (('prescaler=6', 'prescaler=7'),
                                      ('segment1=11', 'segment1=17'),
                                      ('sjw=2', 'sjw=3')):
            with self.subTest(replacement=replacement):
                path = self.authored()
                path.write_text(path.read_text().replace(previous, replacement))
                with self.assertRaisesRegex(ValueError, 'CAN'):
                    gate.resolve_ir(path)

    def test_filter_bank_owner_split_format_and_duplicate_claim_are_checked(self):
        for previous, replacement in (('bank=0', 'bank=14'),
                                      ('id=291', 'id=2048'),
                                      ('format="standard"', 'format="fd"'),
                                      ('controller="bus0"', 'controller="absent"')):
            with self.subTest(replacement=replacement):
                path = self.authored()
                path.write_text(path.read_text().replace(previous, replacement))
                with self.assertRaisesRegex(ValueError, 'CAN'):
                    gate.resolve_ir(path)
        path = self.authored(second=True)
        path.write_text(path.read_text().replace('bank=14', 'bank=0'))
        with self.assertRaisesRegex(ValueError, 'CAN'):
            gate.resolve_ir(path)

    def test_derived_capability_uses_actual_common_io_operations_source(self):
        from tools.maintenance import capabilities
        rows = capabilities.collect(gate.ROOT)
        selected = [row for row in rows if row["kind"] == "can"]
        self.assertEqual(len(selected), 4)
        for row in selected:
            self.assertEqual(row["exports"], ["io/src/controllers/can_controller.c"])
            self.assertTrue(row["fixtures"])

    def test_no_can_configuration_produces_no_domain_state_or_driver_calls(self):
        output = self.root / 'empty'
        gate.configure(gate.ROOT / 'tools/configure/assemblies/qiming-baremetal.toml', output)
        source = (output / 'bindings.c').read_text()
        self.assertNotIn('static nx_can_filter_domain_t', source)
        self.assertNotIn('nx_stm32_can_initialize', source)

    def test_zero_storage_unbounded_drain_and_unknown_operations_are_rejected(self):
        for previous, replacement in (('event_capacity=8', 'event_capacity=0'),
                                      ('drain_timeout_us=1000', 'drain_timeout_us=0'),
                                      ('poll_limit=128', 'poll_limit=0'),
                                      ('operation="loopback"', 'operation="automatic"')):
            with self.subTest(replacement=replacement):
                path = self.authored()
                path.write_text(path.read_text().replace(previous, replacement))
                with self.assertRaisesRegex(ValueError, 'CAN'):
                    gate.resolve_ir(path)

    def test_second_controller_cannot_touch_reserved_primary_filter_domain(self):
        path = self.authored(second=True)
        content = path.read_text()
        start = content.index('[can.bus0]')
        end = content.index('[can.bus1]')
        content = content[:start] + content[end:]
        content = content.replace(
            '{controller="bus0",bank=0,id=291,mask=2047,'
            'format="standard",fifo=0,remote=false,match_remote=true},', '')
        board = gate.load(gate.ROOT / 'boards/stm32f407_qiming_v31/board.json')
        board['reserved_resources'].append('controller:CAN1')
        package = self.root / 'board'
        package.mkdir()
        (package / 'board.json').write_text(json.dumps(board))
        path.write_text(content.replace('board="stm32f407_qiming_v31"',
                                        'board_package="board"'))
        with self.assertRaisesRegex(ValueError, 'CAN.*reserved'):
            gate.resolve_ir(path)


if __name__ == '__main__':
    unittest.main()
