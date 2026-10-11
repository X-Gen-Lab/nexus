"""Wait cost proof rejects missing measurements and unsafe output boundaries."""
from pathlib import Path
import tempfile
import unittest

import os_wait_cost as cost


class WaitCostAccountingTests(unittest.TestCase):
    def test_actual_gnu_size_accounting_counts_data_in_flash_and_ram(self):
        text = ('text data bss dec hex filename\n'
                '124 16 12 152 98 /fixture with spaces/image.elf\n')
        self.assertEqual(cost.parse_sizes(text), {
            'text_bytes': 124, 'data_bytes': 16, 'bss_bytes': 12,
            'flash_bytes': 140, 'ram_bytes': 28})

    def test_empty_malformed_and_inconsistent_sizes_fail_closed(self):
        for value in ('', 'text data bss\n',
                      'text data bss dec hex filename\n1 2 3 7 6 image\n',
                      'text data bss dec hex filename\n1 2 3 6 7 image\n',
                      'text data bss dec hex filename\n0 2 3 5 5 image\n',
                      'text data bss dec hex filename\n1 -2 3 2 2 image\n'):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    cost.parse_sizes(value)

    def test_nm_retains_table_size_and_readonly_classification(self):
        self.assertEqual(cost.parse_symbols(
            '20000000 00000020 d ports\n08000020 0000000c r ops\n'), {
                'ports': {'size': 32, 'kind': 'd'},
                'ops': {'size': 12, 'kind': 'r'}})

    def test_duplicate_symbols_and_empty_or_stripped_nm_fail(self):
        for value in ('', '20000000 d ports\n',
                      '20000000 00000010 d ports\n'
                      '20000010 00000010 d ports\n'):
            with self.assertRaises(ValueError):
                cost.parse_symbols(value)

    def test_layout_uses_measured_symbols_for_direct_and_shared_models(self):
        direct = {'ports': {'size': 32, 'kind': 'd'}}
        shared = {'ports': {'size': 16, 'kind': 'd'},
                  'ops': {'size': 12, 'kind': 'r'}}
        cost.check_layout(direct, 2, False)
        cost.check_layout(shared, 2, True)

    def test_layout_cannot_accept_wrong_sizes_or_writable_ops(self):
        for symbols, shared in (
                ({'ports': {'size': 16, 'kind': 'd'}}, False),
                ({'ports': {'size': 16, 'kind': 'd'},
                  'ops': {'size': 12, 'kind': 'd'}}, True),
                ({'ports': {'size': 16, 'kind': 'd'}}, True),
                ({'ports': {'size': 32, 'kind': 'd'},
                  'ops': {'size': 12, 'kind': 'r'}}, False)):
            with self.assertRaises(ValueError):
                cost.check_layout(symbols, 2, shared)

    def test_invalid_model_arguments_are_not_coerced(self):
        for count, shared in ((0, False), (3, False), (True, False),
                              (1, 0), (1, 'false')):
            with self.assertRaises(ValueError):
                cost.check_layout({}, count, shared)

    def test_dispatch_counts_real_thumb_instructions_and_ignores_literal_data(self):
        text = ('08000000 <call_arm>:\n'
                ' 8000000: 6800 ldr r0, [r0]\n'
                ' 8000002: 4770 bx lr\n'
                ' 8000004: 08000000 .word 0x08000000\n\n'
                '08000008 <call_wait>:\n'
                ' 8000008: f44f 72fa mov.w r2, #500\n'
                ' 800000c: 4770 bx lr\n\n'
                '08000010 <call_wake>:\n'
                ' 8000010: 4770 bx lr\n')
        self.assertEqual(cost.dispatch_instructions(text), {
            'call_arm': 2, 'call_wait': 2, 'call_wake': 1})

    def test_missing_duplicate_or_empty_dispatch_bodies_fail(self):
        for value in ('', '08000000 <call_arm>:\n',
                      '08000000 <call_arm>:\n 0: 4770 bx lr\n\n'
                      '08000002 <call_arm>:\n 2: 4770 bx lr\n'):
            with self.assertRaises(ValueError):
                cost.dispatch_instructions(value)

    def test_matrix_uses_production_cpu_resolution_and_exact_scope(self):
        variants = cost.matrix()
        self.assertEqual(len(variants), 48)
        self.assertEqual(len({item['name'] for item in variants}), 48)
        self.assertEqual({item['instances'] for item in variants}, {1, 2, 4, 8})
        self.assertEqual({item['optimization'] for item in variants}, {'Os', 'O2'})
        self.assertEqual({item['shared_ops'] for item in variants}, {False, True})
        for variant in variants:
            options = variant['profile']['compile_options']
            self.assertIn('-mthumb', options)
            self.assertIn('-fshort-enums', options)
            if variant['facts']['cpu']['arch'] == 'cortex-m33':
                self.assertIn('-mcpu=cortex-m33+nodsp', options)
                self.assertFalse(variant['facts']['cpu']['dsp'])


class WaitCostOutputOwnershipTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.base = Path(self.directory.name)
        self.root = self.base / 'source'
        self.root.mkdir()

    def test_fresh_output_is_created_without_source_mutation(self):
        output = self.base / 'fresh'
        self.assertEqual(cost.prepare_output(self.root, output), output)
        self.assertTrue(output.is_dir())
        self.assertEqual(list(self.root.iterdir()), [])

    def test_existing_evidence_is_never_overwritten(self):
        output = self.base / 'existing'
        output.mkdir()
        retained = output / 'report.json'
        retained.write_text('retained evidence\n')
        with self.assertRaises(ValueError):
            cost.prepare_output(self.root, output)
        self.assertEqual(retained.read_text(), 'retained evidence\n')

    def test_source_and_ancestor_outputs_are_rejected(self):
        for output in (self.root, self.base, self.root / 'os'):
            with self.assertRaises(ValueError):
                cost.prepare_output(self.root, output)
        self.assertFalse((self.root / 'os').exists())

    def test_new_owned_build_output_is_allowed(self):
        output = self.root / 'build' / 'fresh-cost'
        self.assertEqual(cost.prepare_output(self.root, output), output)

    def test_symlink_aliases_do_not_redirect_artifact_writes(self):
        alias = self.base / 'alias'
        alias.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(ValueError):
            cost.prepare_output(self.root, alias / 'build' / 'fresh-cost')
        self.assertFalse((self.root / 'build').exists())

    def test_missing_compiler_preserves_a_failed_execution_report(self):
        output = self.base / 'missing-tool'
        source = Path(cost.__file__).resolve().parents[2]
        self.assertEqual(cost.run(source, '/missing/arm-none-eabi-gcc', output), 1)
        import json
        report = json.loads((output / 'report.json').read_text())
        self.assertEqual(report['status'], 'failed')
        self.assertIn('compiler missing', report['error'])
        self.assertEqual(report['commands'], [])
        self.assertEqual(report['rows'], [])

    def test_a_different_checkout_cannot_replace_imported_cpu_authority(self):
        output = self.base / 'wrong-root'
        self.assertEqual(cost.run(self.root, '/missing/compiler', output), 2)
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
