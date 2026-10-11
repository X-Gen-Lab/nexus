"""Boundary tests for actual optional-memory matrix orchestration."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

from scripts.ci import gd32_memory


class MemoryMatrixTests(unittest.TestCase):
    def test_preprocessing_does_not_overwrite_actual_build_depfiles(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'input.c'
            source.write_text('unsigned live;\n')
            depfile = root / 'actual.d'
            depfile.write_text('original build dependency bytes\n')
            obj = root / 'actual.o'
            obj.write_bytes(b'original actual object')
            entry = {'arguments': ['gcc', '-std=c11', '-MMD', '-MP', '-MT',
                'actual.o', '-MF', str(depfile), '-o', str(obj), '-c', str(source)]}
            result = subprocess.run(gd32_memory.preprocessing_command(entry),
                capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('unsigned live;', result.stdout)
            self.assertEqual(depfile.read_text(),
                'original build dependency bytes\n')
            self.assertEqual(obj.read_bytes(), b'original actual object')

    def test_attached_output_option_cannot_destroy_the_actual_object(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'input.c'
            source.write_text('unsigned live;\n')
            obj = root / 'actual.o'
            obj.write_bytes(b'original actual object')
            entry = {'arguments': ['gcc', '-std=c11', '-o' + str(obj),
                                  '-c', str(source)]}
            result = subprocess.run(gd32_memory.preprocessing_command(entry),
                capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(obj.read_bytes(), b'original actual object')
            self.assertIn('unsigned live;', result.stdout)

    def test_changed_source_identity_fails_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / 'source.c'
            source.write_text('original source\n')
            identities = [gd32_memory.arch_compile.digest(source)]
            gd32_memory.verify_inputs(identities)
            source.write_text('changed source\n')
            with self.assertRaisesRegex(ValueError, 'source changed'):
                gd32_memory.verify_inputs(identities)

    def test_missing_tool_retains_actual_failure_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'missing'
            status = gd32_memory.run(gd32_memory.ROOT,
                '/nonexistent/arm-none-eabi-gcc', output)
            self.assertEqual(status, 1)
            report = json.loads((output / 'report.json').read_text())
            self.assertEqual(report['status'], 'failed')
            self.assertEqual(report['physical_status'], 'not_executed')
            self.assertIn('compiler missing', report['error'])
            self.assertEqual(report['variants'], [])

    def test_existing_output_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            report = output / 'report.json'
            report.write_bytes(b'original failed evidence')
            with self.assertRaisesRegex(ValueError, 'must be fresh'):
                gd32_memory.run(gd32_memory.ROOT,
                    '/nonexistent/arm-none-eabi-gcc', output)
            self.assertEqual(report.read_bytes(), b'original failed evidence')

    def test_real_expected_nonzero_keeps_raw_diagnostics(self):
        with tempfile.TemporaryDirectory() as temporary:
            recorder = gd32_memory.arch_compile.Recorder(Path(temporary))
            record = gd32_memory.rejected(recorder,
                [sys.executable, '-c',
                 'import sys; print("expected refusal"); sys.exit(1)'],
                'actual-negative', 'expected refusal')
            self.assertEqual(record['returncode'], 1)
            self.assertIn('expected refusal',
                Path(record['stdout']['path']).read_text())

    def test_unrelated_nonzero_is_never_accepted_as_expected_refusal(self):
        with tempfile.TemporaryDirectory() as temporary:
            recorder = gd32_memory.arch_compile.Recorder(Path(temporary))
            with self.assertRaisesRegex(ValueError, 'different reason'):
                gd32_memory.rejected(recorder, [sys.executable, '-c',
                    'import sys; print("unrelated error"); sys.exit(1)'],
                    'wrong-negative', 'expected refusal')

    def test_success_with_refusal_text_is_still_unexpected_acceptance(self):
        with tempfile.TemporaryDirectory() as temporary:
            recorder = gd32_memory.arch_compile.Recorder(Path(temporary))
            with self.assertRaisesRegex(ValueError, 'unexpectedly accepted'):
                gd32_memory.rejected(recorder, [sys.executable, '-c',
                    'print("expected refusal")'], 'false-negative',
                    'expected refusal')


if __name__ == '__main__':
    unittest.main()
