"""Original formal diagnostics and ordinary byte transport, not qualification."""

import json
from pathlib import Path
import shutil
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from tools.evidence import formal_diagnostics as diagnostics
from tools.evidence import store
from tools.evidence.common import EvidenceError, file_identity
from tools.evidence.identity import canonical_digest


class FormalDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.parent = Path(temporary.name)
        self.formal = self.parent / 'formal'
        self.build = self.parent / 'build'
        self.artifact = self.parent / 'artifact'
        self.output = self.parent / 'output'
        self.context = {'commit': 'a' * 40, 'run_id': '123', 'attempt': '2'}
        software = self.formal / 'software'
        software.mkdir(parents=True)
        (software / 'broken.raw').write_bytes(b'actual fixture failure\n')
        (software / 'passed.command.json').write_text('{}\n')
        (software / 'broken.command.json').write_text('{}\n')
        attempt = software / 'broken.attempts/original'
        attempt.mkdir(parents=True)
        (attempt / 'stdout.txt').write_bytes(b'original stdout\n')
        (attempt / 'stderr.txt').write_bytes(b'')
        (attempt / 'receipt.json').write_text('{}\n')
        (software / 'never-execute.py').write_text(
            'raise RuntimeError("Archived code must never execute")\n')
        self.report = {'status': 'failed', 'source_before': {
            'commit': self.context['commit'], 'dirty': False},
            'source_after': {'commit': self.context['commit'], 'dirty': False},
            'stages': [{'name': 'passed', 'status': 'passed'},
                       {'name': 'broken', 'status': 'failed'}]}
        (software / 'report.json').write_text(json.dumps(self.report))
        package = store.export_tree(
            self.formal, self.build / 'formal-transfer/store')
        envelope = self.build / 'formal-transfer/formal-package.json'
        envelope.write_text(json.dumps(package))
        self.job = {'schema_version': 1, 'kind': 'ci_job_identity',
            'job': 'formal-software', 'job_result': 'failure',
            'source': self.report['source_before'],
            'run': {'GITHUB_RUN_ID': '123', 'GITHUB_RUN_ATTEMPT': '2'},
            'qualification': 'not_a_software_candidate',
            'artifacts': [{**file_identity(envelope),
                'relative_path': 'build/formal-transfer/formal-package.json'}]}
        self.job_path = self.build / 'job-identities/formal-software/identity.json'
        self.job_path.parent.mkdir(parents=True)
        self.seal_fixture()

    def seal_fixture(self):
        self.job.pop('record_sha256', None)
        self.job['record_sha256'] = canonical_digest(self.job)
        self.job_path.write_text(json.dumps(self.job))
        if self.artifact.exists():
            shutil.rmtree(self.artifact)
        self.package = store.export_tree(
            self.build, self.artifact / 'formal-build-store')
        (self.artifact / 'formal-build-package.json').write_text(
            json.dumps(self.package))

    def prepare(self, context=None):
        return diagnostics.prepare(self.artifact, self.output,
                                   context or self.context)

    def update_formal(self):
        (self.formal / 'software/report.json').write_text(json.dumps(self.report))
        shutil.rmtree(self.build / 'formal-transfer')
        package = store.export_tree(
            self.formal, self.build / 'formal-transfer/store')
        envelope = self.build / 'formal-transfer/formal-package.json'
        envelope.write_text(json.dumps(package))
        self.job['artifacts'] = [{**file_identity(envelope),
            'relative_path': 'build/formal-transfer/formal-package.json'}]
        self.seal_fixture()

    def test_explicit_failure_report_and_contexts_are_preserved(self):
        detail = self.formal / 'software/tidy.json'
        detail.write_text('Actual analyzer diagnostic, despite JSON suffix\n')
        contexts = self.formal / 'software/tidy.contexts/observed'
        contexts.mkdir(parents=True)
        (contexts / 'stderr.txt').write_text('actual compiler diagnostic\n')
        stage = self.report['stages'][-1]
        stage.update(checker='quality_tools', argv=[
            'python', 'checker.py', '--report', str(detail)],
            raw={'path': str(self.formal / 'software/broken.raw')})
        self.update_formal()
        self.prepare()
        root = self.output / 'diagnostics/original-files/software'
        self.assertEqual((root / 'tidy.json').read_bytes(), detail.read_bytes())
        self.assertEqual((root / 'tidy.contexts/observed/stderr.txt').read_bytes(),
                         (contexts / 'stderr.txt').read_bytes())

    def test_explicit_report_outside_original_output_is_rejected(self):
        stage = self.report['stages'][-1]
        stage.update(checker='quality_tools', argv=[
            'python', 'checker.py', '--report', '/outside/secret'],
            raw={'path': str(self.formal / 'software/broken.raw')})
        self.update_formal()
        with self.assertRaises(EvidenceError):
            self.prepare()

    def test_native_failure_keeps_only_exact_preset_logs(self):
        tdd = self.build / 'native-asan/tdd'
        tdd.mkdir(parents=True)
        (tdd / 'failure.txt').write_text('real sanitizer stack\n')
        (self.build / 'native-asan/unrelated.bin').write_bytes(b'not selected')
        self.report['stages'][-1].update(checker='native_contracts',
            argv=['python', 'checker.py', '--preset', 'native-asan'])
        self.update_formal()
        result = self.prepare()
        root = self.output / 'diagnostics/original-files/build/native-asan'
        self.assertEqual((root / 'tdd/failure.txt').read_bytes(),
                         (tdd / 'failure.txt').read_bytes())
        self.assertFalse((root / 'unrelated.bin').exists())
        self.assertIn('build/native-asan/ctest-results.xml',
                      result['missing_diagnostics'])

    def test_original_failure_and_complete_ordered_bytes_are_preserved(self):
        before = store.inventory(self.artifact)
        with patch.object(diagnostics, 'PART_BYTES', 512):
            result = self.prepare()
        self.assertFalse(result['software_qualified'])
        self.assertEqual(result['source_status'], 'failed')
        self.assertGreater(len(result['parts']), 1)
        joined = self.parent / 'joined.tar.gz'
        joined.write_bytes(b''.join((self.output / 'parts' / item['name'])
                                   .read_bytes() for item in result['parts']))
        self.assertEqual(joined.read_bytes(),
                         Path(self.package['archive']['path']).read_bytes())
        store.verify_package(self.package, archive_override=joined)
        root = self.output / 'diagnostics'
        self.assertEqual(json.loads((root / 'original-report.json').read_text()),
                         self.report)
        for relative in ('broken.raw', 'passed.command.json',
                         'broken.command.json',
                         'broken.attempts/original/receipt.json',
                         'broken.attempts/original/stdout.txt',
                         'broken.attempts/original/stderr.txt'):
            self.assertEqual((root / 'original-files/software' / relative)
                             .read_bytes(), (self.formal / 'software' / relative)
                             .read_bytes())
        self.assertFalse((root / 'original-files/software/never-execute.py')
                         .exists())
        self.assertEqual(store.inventory(self.artifact), before)

    def test_requested_source_run_and_attempt_must_match_original(self):
        for key, value in (('commit', 'b' * 40), ('run_id', '124'),
                           ('attempt', '3')):
            with self.subTest(key=key):
                self.output = self.parent / ('reject-' + key)
                with self.assertRaises(EvidenceError):
                    self.prepare(dict(self.context, **{key: value}))

    def test_archive_tampering_is_rejected(self):
        with Path(self.package['archive']['path']).open('ab') as stream:
            stream.write(b'changed original')
        with self.assertRaisesRegex(EvidenceError, 'artifact identity changed'):
            self.prepare()

    def test_original_job_must_bind_nested_envelope(self):
        self.job['artifacts'][0]['sha256'] = '0' * 64
        self.seal_fixture()
        with self.assertRaisesRegex(EvidenceError, 'bind the nested envelope'):
            self.prepare()

    def test_unknown_input_file_and_alias_are_rejected(self):
        extra = self.artifact / 'unexpected'
        extra.write_text('unknown')
        with self.assertRaises(EvidenceError):
            self.prepare()
        extra.unlink()
        extra.symlink_to(self.artifact / 'formal-build-package.json')
        with self.assertRaises(EvidenceError):
            self.prepare()

    def test_concurrent_unknown_input_is_rejected(self):
        added = threading.Event()
        stop = threading.Event()

        def producer():
            deadline = time.monotonic() + 2
            while not stop.is_set() and time.monotonic() < deadline:
                if (self.output / 'diagnostics').is_dir():
                    (self.artifact / 'late-unknown').write_text('new input')
                    added.set()
                    return
                stop.wait(0.0001)

        thread = threading.Thread(target=producer)
        thread.start()
        try:
            with self.assertRaisesRegex(EvidenceError, 'Unknown or missing'):
                self.prepare()
        finally:
            stop.set()
            thread.join()
        self.assertTrue(added.is_set(), 'Producer must actually change input')

    def test_archive_and_diagnostic_limits_fail_without_truncation(self):
        with patch.object(diagnostics, 'MAX_PARTS', 1), \
                patch.object(diagnostics, 'PART_BYTES', 1):
            with self.assertRaises(EvidenceError):
                self.prepare()
        self.output = self.parent / 'small-diagnostic-bound'
        with patch.object(diagnostics, 'DIAGNOSTIC_BYTES', 1):
            with self.assertRaises(EvidenceError):
                self.prepare()
        self.assertEqual((self.formal / 'software/broken.raw').read_bytes(),
                         b'actual fixture failure\n')

    def test_diagnostic_limit_includes_metadata_and_raw_together(self):
        self.prepare()
        size = sum(path.stat().st_size for path in
                   (self.output / 'diagnostics').rglob('*') if path.is_file())
        self.output = self.parent / 'complete-bound'
        with patch.object(diagnostics, 'DIAGNOSTIC_BYTES', size - 1):
            with self.assertRaisesRegex(EvidenceError,
                                       'Complete diagnostic output'):
                self.prepare()

    def test_cli_preserves_failed_status_without_granting_qualification(self):
        self.assertEqual(diagnostics.main([
            '--artifact', str(self.artifact), '--output', str(self.output),
            '--expected-commit', self.context['commit'], '--run-id', '123',
            '--run-attempt', '2']), 0)
        result = json.loads((self.output / 'transport/manifest.json').read_text())
        self.assertEqual(result['source_status'], 'failed')
        self.assertFalse(result['software_qualified'])

    def test_unknown_context_existing_and_overlapping_outputs_rejected(self):
        with self.assertRaises(EvidenceError):
            self.prepare(dict(self.context, arbitrary='unknown'))
        self.output.mkdir()
        with self.assertRaises(EvidenceError):
            self.prepare()
        self.output = self.artifact / 'inside'
        with self.assertRaises(EvidenceError):
            self.prepare()


if __name__ == '__main__':
    unittest.main()
