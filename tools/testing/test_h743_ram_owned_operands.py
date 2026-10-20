"""Byte-defined operand classification for the fixed H743 RAM window.

These small regressions exercise decoder/address rules without a compiler.
Actual production ELF and mutation runs remain a separate required boundary.
"""
from __future__ import annotations

from types import SimpleNamespace
import unittest

from tools.evidence.common import EvidenceError
from tools.measurement import flash_ram_static as checked


class OwnedOperandTests(unittest.TestCase):
    def effect(self, encoded, values=None):
        data = bytes.fromhex(encoded)
        op = checked._decode(0x24000000, data, bridge=False)
        incoming = [None] * 16 if values is None else values
        return checked._constant_effect(
            0x24000000, data, op, incoming, lambda *_: 0x24004000,
            all_accesses=True)

    def policy(self):
        symbols = [
            {'name': 'g_nx_stm32h743_flash_ram_workspace', 'value': 0x24004000,
             'size': 1800, 'type': 1, 'binding': 1},
            {'name': 'g_nx_stm32h743_system', 'value': 0x24005000,
             'size': 48, 'type': 1, 'binding': 1},
        ]
        return checked._OwnedMemoryPolicy(SimpleNamespace(
            defined_symbols=symbols,
            loads=[{'flags': 6, 'virtual': 0x24004000, 'memory': 0x1100}],
            symbol=lambda name: {'__nx_flash_ram_start': 0x24000000,
                                 '__nx_flash_ram_end': 0x24001000}[name]))

    def test_postindexed_word_load_retains_exact_updated_base(self):
        values = [None] * 16
        values[3] = 0x24004000
        after, accesses = self.effect('53f8042b', values)
        self.assertEqual(0x24004004, after[3])
        self.assertEqual([(0x24000000, 0x24004000, 4, True, 3)], accesses)

    def test_writeback_data_base_overlap_is_not_an_address_proof(self):
        for encoding in ('53f8043b', '43f8043b'):
            with self.subTest(encoding=encoding):
                with self.assertRaisesRegex(EvidenceError, 'writeback/data'):
                    self.effect(encoding)

    def test_every_indexed_transfer_records_its_architectural_base_write(self):
        for opcode in (0xf850, 0xf840, 0xf810, 0xf800, 0xf830, 0xf820):
            for base in range(8, 12):
                for mode in (0x0b04, 0x0f04, 0x0d04):
                    encoded = ((opcode | base).to_bytes(2, 'little') +
                               mode.to_bytes(2, 'little'))
                    with self.subTest(encoding=encoded.hex()):
                        op = checked._decode(0, encoded, bridge=False)
                        self.assertIn(base, op.writes)

    def test_multiple_transfers_record_implicit_base_writes(self):
        for encoding, expected in (('03c3', {3}), ('03cb', {0, 1, 3}),
                                   ('09cb', {0, 3})):
            with self.subTest(encoding=encoding):
                op = checked._decode(0, bytes.fromhex(encoding), bridge=False)
                self.assertEqual(expected, set(op.writes))
        # Current admitted dual and wide multiple subsets have no writeback;
        # widening them needs its own byte-defined proof before admission.
        for encoding in ('f8e90201', 'f8e80201', 'b8e80300', 'a8e80300'):
            with self.subTest(encoding=encoding):
                with self.assertRaisesRegex(EvidenceError, 'unsupported Thumb'):
                    checked._decode(0, bytes.fromhex(encoding), bridge=False)

    def test_store_observation_includes_unknown_and_exact_width(self):
        _, accesses = self.effect('5a60')  # str r2,[r3,#4]
        self.assertEqual([(0x24000000, None, 4, False, 3)], accesses)
        values = [None] * 16
        values[3] = 0x24004000
        _, accesses = self.effect('5a70', values)  # strb r2,[r3,#1]
        self.assertEqual([(0x24000000, 0x24004001, 1, False, 3)], accesses)

    def test_literal_and_direct_sp_loads_are_explicit_operands(self):
        _, literal = self.effect('004b')
        self.assertEqual([(0x24000000, 0x24000004, 4, True, 15)], literal)
        _, stack = self.effect('019b')
        self.assertEqual([(0x24000000, None, 4, True, 13)], stack)

    def test_canonical_pop_pc_uses_the_matching_stack_restore_proof(self):
        _, accesses = self.effect('5df804fb')
        self.assertEqual([], accesses)

    def test_equality_edge_uses_only_unique_immediate_cmp_predecessor(self):
        code = {}
        for address, encoding in ((0, '9442'), (2, '01d1'),
                                  (4, '7047'), (8, '7047')):
            code[address] = ({'encoding': encoding}, checked._decode(
                address, bytes.fromhex(encoding), bridge=False), False)
        values = [checked.UNKNOWN_CONSTANT] * 16
        values[2] = frozenset({0x24004000})
        taken, following = checked._equality_successors(code, 2, tuple(values))
        self.assertEqual(checked.UNKNOWN_CONSTANT, taken[4])
        self.assertEqual(frozenset({0x24004000}), following[4])
        # A branch can enter the BNE without executing CMP: no inference.
        code[10] = ({'encoding': 'fae7'}, checked._Op('branch', 2), False)
        self.assertEqual((tuple(values), tuple(values)),
                         checked._equality_successors(code, 2, tuple(values)))

    def test_pc_and_sp_cannot_become_stale_constants_through_cmp(self):
        for encoding in ('7845', '6845', '8745', '8545'):
            with self.subTest(encoding=encoding):
                with self.assertRaisesRegex(EvidenceError, 'CMP.*PC/SP'):
                    checked._decode(0, bytes.fromhex(encoding), bridge=False)
                code = {0: ({'encoding': encoding}, checked._Op(), False),
                        2: ({'encoding': '01d1'}, checked._Op('conditional', 8),
                            False),
                        4: ({'encoding': '7047'}, checked._Op('return'), False)}
                values = [checked.UNKNOWN_CONSTANT] * 16
                values[0] = frozenset({0x24004000})
                self.assertEqual((tuple(values), tuple(values)),
                                 checked._equality_successors(
                                     code, 2, tuple(values)))

    def test_register_shifted_address_addition_is_exact(self):
        values = [None] * 16
        values[1], values[2] = 0x52002000, 1
        after, _ = self.effect('01eb0221', values)  # add.w r1,r1,r2,lsl#8
        self.assertEqual(0x52002100, after[1])

    def test_owned_reads_and_mmio_need_exact_extent_width_direction(self):
        policy = self.policy()
        self.assertEqual('workspace', policy.classify(0x240046a0, 4, True))
        self.assertEqual('system', policy.classify(0x24005018, 8, True))
        self.assertEqual('mmio', policy.classify(0x52002010, 4, True))
        for address, width, load in ((0x24004707, 2, True),
                                     (0x52002010, 4, False),
                                     (0x52002010, 2, True),
                                     (0xe000ed18, 4, True),
                                     (None, 4, True),
                                     (0xffffffff, 4, True)):
            with self.subTest(address=address, width=width, load=load):
                with self.assertRaises(EvidenceError):
                    policy.classify(address, width, load)

    def test_staged_command_and_private_stack_are_not_writable_data(self):
        policy = self.policy()
        for offset in (0, 664, 1688, 1692, 1696, 1700, 1732, 1736,
                       1740, 1756, 1760):
            with self.subTest(offset=offset):
                with self.assertRaises(EvidenceError):
                    policy.classify(0x24004000 + offset, 4, False)
        for offset, width in ((1744, 4), (1748, 4), (1752, 1), (1755, 1),
                              (1768, 4), (1793, 1)):
            self.assertEqual('workspace', policy.classify(
                0x24004000 + offset, width, False))

    def test_fixed_sp_reads_need_local_frame_or_actual_caller_slots(self):
        self.assertTrue(checked._owned_stack_access(16, 4, 4, {}))
        self.assertTrue(checked._owned_stack_access(16, 16, 4, {0: None}))
        self.assertFalse(checked._owned_stack_access(16, 20, 4, {0: None}))
        self.assertFalse(checked._owned_stack_access(16, -4, 4, {}))
        self.assertFalse(checked._owned_stack_access(16, 12, 8, {}))

    def test_only_verified_assembly_can_retain_frame_on_nonreturning_tail(self):
        code = {}
        for address, encoding in ((0, '10b5'), (2, '7de0')):
            data = bytes.fromhex(encoding)
            code[address] = ({'encoding': encoding},
                             checked._decode(address, data, bridge=True), False)
        options = dict(ordinary_c=False, transfer=lambda *_: ())
        with self.assertRaisesRegex(EvidenceError, 'tailcall retains'):
            checked._walk(code, 0, {2: 1}, {1: False}, **options)
        peak, returns, _ = checked._walk(
            code, 0, {2: 1}, {1: False}, allow_retained_terminal=True, **options)
        self.assertEqual((8, False), (peak, returns))
        with self.assertRaisesRegex(EvidenceError, 'tailcall retains'):
            checked._walk(code, 0, {2: 1}, {1: True},
                          allow_retained_terminal=True, **options)

    def invocation_graph(self, rows, calls):
        functions = {}
        for index, name, start, instructions, targets in (
                (1, checked.PREFIX + 'program_words', 0x100,
                 ((0x100, '7047', False),), {}),
                (0, checked.PREFIX + 'transaction', 0, rows, calls)):
            code = {}
            for address, encoding, conditional in instructions:
                code[address] = ({'encoding': encoding}, checked._decode(
                    address, bytes.fromhex(encoding), bridge=False), conditional)
            functions[index] = {'symbol': {'name': name, 'value': start | 1},
                                'code': code, 'calls': targets}
        return checked._program_word_invocations(
            functions, (1, 0), {0: True, 1: True}, 1)

    def test_positive_weight_cycle_rejects_and_zero_weight_loop_converges(self):
        with self.assertRaisesRegex(EvidenceError, 'program pulse invocation'):
            self.invocation_graph(((0, '00f07ef8', False),
                                   (4, 'fce7', False)), {0: 1})
        report = self.invocation_graph(((0, 'fed1', False),
                                        (2, '00f07df8', False),
                                        (6, '7047', False)), {2: 1})
        self.assertEqual(1, report['maximum_invocations'])
        self.assertEqual(8, report['maximum_dynamic_word_stores'])

    def test_it_skipped_call_retains_zero_or_one_pulse_alternatives(self):
        report = self.invocation_graph(((0, '00f07ef8', True),
                                        (4, '7047', False)), {0: 1})
        self.assertEqual(1, report['maximum_invocations'])


if __name__ == '__main__':
    unittest.main()
