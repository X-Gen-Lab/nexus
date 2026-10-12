"""Fail-closed machine-code checks for the M7 clean/disable transition."""
import unittest

import cache_control_compile as gate


DISASSEMBLY = """
00000000 <nx_arch_cache_clean_disable_leaf>:
   0: f3bf 8f4f dsb sy
   4: 4684 mov ip, r0
   6: f64e 7274 movw r2, #61300
   a: f2ce 0200 movt r2, #57344
   e: f04f 4340 mov.w r3, #3221225472
  12: ea4c 0003 orr.w r0, ip, r3
  16: 6010 str r0, [r2, #0]
  18: f1b3 4380 subs.w r3, r3, #1073741824
  1c: d2f9 bcs.n 12 <nx_arch_cache_clean_disable_leaf+0x12>
  1e: f1bc 0c20 subs.w ip, ip, #32
  22: d5f4 bpl.n e <nx_arch_cache_clean_disable_leaf+0xe>
  24: f3bf 8f4f dsb sy
  28: f64e 5214 movw r2, #60692
  2c: f2ce 0200 movt r2, #57344
  30: 6011 str r1, [r2, #0]
  32: f3bf 8f4f dsb sy
  36: f3bf 8f6f isb sy
  3a: 6810 ldr r0, [r2, #0]
  3c: 4770 bx lr
00000040 <another_function>:
  40: b510 push {r4, lr}
"""


class CacheLeafContractTests(unittest.TestCase):
    def test_reviewed_leaf_accepts_with_unrelated_function_after_it(self):
        gate.check_leaf(DISASSEMBLY)

    def test_absent_or_duplicate_leaf_is_rejected(self):
        for text in ('', DISASSEMBLY.replace('nx_arch_cache_clean_disable_leaf',
                                             'foreign_leaf'),
                     DISASSEMBLY + DISASSEMBLY):
            with self.subTest(text=text[:30]), self.assertRaises(ValueError):
                gate.check_leaf(text)

    def test_stack_access_call_and_extra_store_are_rejected(self):
        for operation in ('push {r4, lr}', 'ldr r0, [sp, #4]',
                          'bl 40 <helper>', 'str r0, [r1, #0]'):
            changed = DISASSEMBLY.replace('4684 mov ip, r0',
                                           '4684 ' + operation)
            with self.subTest(operation=operation), self.assertRaises(ValueError):
                gate.check_leaf(changed)

    def test_missing_clean_or_barrier_or_readback_rejects(self):
        for line in ('  16: 6010 str r0, [r2, #0]\n',
                     '  24: f3bf 8f4f dsb sy\n',
                     '  32: f3bf 8f4f dsb sy\n',
                     '  36: f3bf 8f6f isb sy\n',
                     '  3a: 6810 ldr r0, [r2, #0]\n'):
            with self.subTest(line=line), self.assertRaises(ValueError):
                gate.check_leaf(DISASSEMBLY.replace(line, ''))

    def test_wrong_register_address_or_way_geometry_rejects(self):
        for before, after in (('#61300', '#61280'), ('#60692', '#60700'),
                              ('#57344', '#57345'), ('#32', '#64'),
                              ('#1073741824', '#536870912'),
                              ('#3221225472', '#2147483648')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                gate.check_leaf(DISASSEMBLY.replace(before, after))

    def test_wrong_loop_destination_cannot_omit_or_repeat_lines(self):
        for before, after in (('bcs.n 12', 'bcs.n e'),
                              ('bpl.n e', 'bpl.n 12')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                gate.check_leaf(DISASSEMBLY.replace(before, after))

    def test_movw_clears_upper_half_before_ccr_store(self):
        bad = DISASSEMBLY.replace('  2c: f2ce 0200 movt r2, #57344\n', '')
        with self.assertRaisesRegex(ValueError, 'address'):
            gate.check_leaf(bad)

    def test_early_return_cannot_bypass_clean_and_disable(self):
        with self.assertRaises(ValueError):
            gate.check_leaf(DISASSEMBLY.replace('4684 mov ip, r0',
                                                '4770 bx lr'))


if __name__ == '__main__':
    unittest.main()
