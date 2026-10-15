"""Real ThreadX matrix audit refuses partial, weak and unrelated evidence."""
import importlib
from pathlib import Path
import tempfile
import unittest


class ThreadXGateTest(unittest.TestCase):
    def gate(self):
        return importlib.import_module('os_threadx')

    def test_six_board_policy_links_and_one_distinct_missing_hook(self):
        rows = self.gate().matrix()
        self.assertEqual(len(rows), 7)
        self.assertEqual(len({row['name'] for row in rows}), 7)
        self.assertEqual(sum(row['rejection'] for row in rows), 1)
        self.assertEqual({(row['board'], row['mask']) for row in rows
                          if not row['rejection']},
                         {(board, mask) for board in
                          ('qiming', 'sky', 'liangshan') for mask in
                          ('basepri', 'primask')})

    def test_negative_requires_exact_link_failure_and_no_elf(self):
        text = ('FAILED: bin/nexus_threadx_runtime.elf\n'
                "undefined reference to `nx_threadx_external_tick_setup'\n")
        self.gate().check_rejection(1, text, False)
        for code, diagnostic, elf in ((0, text, False), (2, text, False),
                                      (1, text, True),
                                      (1, 'compiler header missing', False),
                                      (1, text.replace('FAILED:', 'other:'), False)):
            with self.subTest(code=code, diagnostic=diagnostic, elf=elf):
                with self.assertRaises(ValueError):
                    self.gate().check_rejection(code, diagnostic, elf)

    def test_expected_link_accepts_companion_outputs_and_ninja_metadata(self):
        output = ('FAILED: [code=1] bin/nexus_threadx_runtime.elf '
                  'bin/nexus_threadx_runtime.bin bin/nexus_threadx_runtime.map '
                  'bin/nexus_threadx_runtime.resources.json '
                  '/tmp/threadx/build/bin/nexus_threadx_runtime.bin '
                  '/tmp/threadx/build/bin/nexus_threadx_runtime.map '
                  '/tmp/threadx/build/bin/nexus_threadx_runtime.resources.json\n'
                  "undefined reference to `nx_threadx_external_tick_setup'\n")
        self.gate().check_rejection(1, output, False)
        self.gate().check_rejection(1, output.replace('\n', '\r\n'), False)

    def test_expected_link_rejects_other_target_and_unrelated_symbol_context(self):
        target = 'bin/nexus_threadx_runtime.elf'
        symbol = "undefined reference to `nx_threadx_external_tick_setup'\n"
        invalid = (
            'FAILED: bin/unrelated_nexus_threadx_runtime.elf\n' + symbol,
            'FAILED: ' + target + '.tmp\n' + symbol,
            'FAILED: bin/another.elf\nld ' + target + '\n' + symbol,
            'FAILED: ' + target + '\n' + symbol.replace(
                'nx_threadx_external_tick_setup', 'nx_threadx_external_tick_setup_other'),
            'FAILED: ' + target + '\nother diagnostic\n'
            'FAILED: bin/another.elf\n' + symbol,
            symbol + 'FAILED: ' + target + '\nother diagnostic\n',
            'FAILED: [code=2] ' + target + '\n' + symbol,
        )
        for diagnostic in invalid:
            with self.subTest(diagnostic=diagnostic), self.assertRaises(ValueError):
                self.gate().check_rejection(1, diagnostic, False)

    def symbols(self):
        names = self.gate().REQUIRED_SYMBOLS
        return '\n'.join('08000100 T ' + name for name in sorted(names))

    def test_pendsv_must_be_the_same_strong_official_entry(self):
        text = self.symbols()
        self.gate().check_symbols(text)
        for invalid in (text.replace('T PendSV_Handler', 'W PendSV_Handler'),
                        text.replace('08000100 T __tx_PendSVHandler',
                                     '08000104 T __tx_PendSVHandler'),
                        text + '\n08001000 T malloc'):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                self.gate().check_symbols(invalid)

    def test_missing_retained_api_is_not_a_completed_board_link(self):
        with self.assertRaises(ValueError):
            self.gate().check_symbols(self.symbols().replace(
                '08000100 T nx_threadx_task_start', ''))

    def test_fresh_output_rejects_occupied_and_symlink_paths(self):
        gate = self.gate()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / 'out'
            output.mkdir()
            (output / 'report.json').write_text('{}')
            with self.assertRaises(ValueError):
                gate.output_path(output)
            alias = root / 'alias'
            alias.symlink_to(output, target_is_directory=True)
            with self.assertRaises(ValueError):
                gate.output_path(alias / 'new')

    def test_authored_inputs_use_existing_reviewed_board_configuration(self):
        with tempfile.TemporaryDirectory() as temporary:
            row = self.gate().matrix()[1]
            root = Path(__file__).resolve().parents[2]
            output = self.gate().write_inputs(root, Path(temporary), row)
            text = output.read_text()
            self.assertEqual(text.count('backend = "threadx"'), 1)
            self.assertIn('interrupt_mask = "primask"', text)
            self.assertIn('board = "stm32f407_qiming_v31"', text)

    def test_source_snapshot_detects_new_configure_files(self):
        gate = self.gate()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'tools/configure').mkdir(parents=True)
            (root / 'tools/configure/cpu.py').write_text('version=1\n')
            before = gate.source_inputs(root)
            (root / 'tools/configure/new_provider.py').write_text('version=1\n')
            self.assertNotEqual(before, gate.source_inputs(root))


if __name__ == '__main__':
    unittest.main()
