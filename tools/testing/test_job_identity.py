"""Exercise CI identity against real Git, executables and artifact bytes."""

import json
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.evidence.common import EvidenceError
from tools.evidence.identity import canonical_digest
from tools.maintenance import job_identity as identity
from tools.configure import cpu, metadata, runtime


class JobIdentityTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Identity fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        (self.root / '.gitignore').write_text('build/\n')
        (self.root / 'dependencies').mkdir()
        (self.root / 'dependencies/toolchains.lock.json').write_text('{}\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'test fixture')
        self.commit = self.git('rev-parse', 'HEAD')
        self.artifacts = self.root / 'build/software'
        self.artifacts.mkdir(parents=True)
        (self.artifacts / 'image.elf').write_bytes(b'fixture ELF bytes')
        (self.artifacts / 'empty-stderr.log').touch()
        self.output = self.root / 'build/identity'

    def git(self, *args):
        return subprocess.check_output(
            ['git', '-C', str(self.root), *args], text=True).strip()

    def capture(self, **overrides):
        arguments = dict(root=self.root, job='arm.fixture', result='success',
                         expected_commit=self.commit,
                         artifacts=[self.artifacts],
                         tools={'python': sys.executable},
                         output=self.output, require_dependencies=True)
        arguments.update(overrides)
        return identity.capture(**arguments)

    def test_actual_checkout_tree_tools_and_even_empty_logs_are_bound(self):
        record = self.capture()
        self.assertEqual(record['source']['commit'], self.commit)
        self.assertEqual(record['source']['tree'],
                         self.git('rev-parse', 'HEAD^{tree}'))
        self.assertEqual(record['kind'], 'ci_job_identity')
        self.assertEqual(record['job_result'], 'success')
        self.assertEqual(record['qualification'], 'not_a_software_candidate')
        self.assertTrue(record['dependencies_complete'])
        self.assertEqual(len(record['artifacts']), 2)
        self.assertEqual(record['tools'][0]['execution']['exit_code'], 0)
        self.assertIn('Python', record['tools'][0]['version'])
        empty = next(item for item in record['artifacts']
                     if item['relative_path'].endswith('empty-stderr.log'))
        self.assertEqual(empty['size'], 0)
        self.assertEqual(len(empty['sha256']), 64)
        published = json.loads((self.output / 'identity.json').read_text())
        self.assertEqual(published, record)
        self.assertEqual(record['record_sha256'], canonical_digest({
            key: value for key, value in record.items()
            if key != 'record_sha256'}))

    def test_configuration_identity_binds_actual_declared_input_bytes(self):
        source_input = self.root / 'dependencies/toolchains.lock.json'
        resolved = {'schema_version': 1, 'inputs': [{
            'path': 'toolchain-lock',
            'sha256': identity.file_identity(source_input)['sha256']}]}
        resolved['configuration_sha256'] = canonical_digest(resolved)
        (self.artifacts / 'resolved.json').write_text(json.dumps(resolved))
        (self.artifacts / 'input_paths.json').write_text(json.dumps({
            'toolchain-lock': str(source_input)}))
        record = self.capture()
        configuration = record['configurations'][0]
        self.assertEqual(configuration['configuration_sha256'],
                         resolved['configuration_sha256'])
        self.assertEqual(configuration['inputs'][0]['file'],
                         identity.file_identity(source_input))
        self.output = self.root / 'build/tampered-identity'
        resolved['inputs'][0]['sha256'] = '0' * 64
        resolved.pop('configuration_sha256')
        resolved['configuration_sha256'] = canonical_digest(resolved)
        (self.artifacts / 'resolved.json').write_text(json.dumps(resolved))
        with self.assertRaisesRegex(EvidenceError, 'input changed'):
            self.capture()

    def test_missing_configuration_navigation_cannot_seal_success(self):
        (self.artifacts / 'resolved.json').write_text('{}')
        with self.assertRaisesRegex(EvidenceError, 'configuration|resolved'):
            self.capture()

    def test_failed_job_retains_invalid_configuration_bytes(self):
        (self.artifacts / 'resolved.json').write_text('partial invalid JSON')
        record = self.capture(result='failure')
        self.assertEqual(record['configurations'][0]['validation'], 'invalid')
        self.assertTrue(record['configurations'][0]['error'])
        self.assertEqual(record['job_result'], 'failure')

    def test_cpu_runtime_inputs_use_their_actual_length_framed_identity(self):
        assembly = self.artifacts / 'runtime.toml'
        facts = self.artifacts / 'cpu.json'
        assembly.write_text('schema_version = 1\n')
        facts.write_text('{"fixture": true}\n')
        policy = self.artifacts / 'platform-compatibility.json'
        policy.write_bytes((metadata.ROOT / metadata.POLICY).read_bytes())
        checksum = hashlib.sha256()
        for label, path in ((b'assembly', assembly), (b'cpu-facts', facts),
                            (b'platform-metadata', policy)):
            data = path.read_bytes()
            checksum.update(label + b'\0' + len(data).to_bytes(8, 'big') + data)
        resolved = {'schema_version': 1, 'cpu_profile': {'fixture': True},
                    'platform_metadata': metadata.policy_contract(
                        json.loads(policy.read_text())),
                    'input_sha256': checksum.hexdigest()}
        resolved['configuration_sha256'] = canonical_digest(resolved)
        (self.artifacts / 'resolved-cpu.json').write_text(json.dumps(resolved))
        (self.artifacts / 'input_paths.json').write_text(json.dumps({
            'assembly': str(assembly), 'cpu_facts': str(facts),
            'platform_metadata': str(policy)}))
        record = self.capture()
        self.assertEqual(record['configurations'][0]['validation'], 'valid')
        self.assertEqual(len(record['configurations'][0]['inputs']), 3)
        self.output = self.root / 'build/tampered-cpu'
        facts.write_text('changed generated facts')
        with self.assertRaisesRegex(EvidenceError, 'input changed'):
            self.capture()

    def prepare_actual_cpu_inputs(self):
        policy = self.root / metadata.POLICY
        policy.write_bytes((metadata.ROOT / metadata.POLICY).read_bytes())
        self.git('add', str(policy))
        self.git('commit', '-qm', 'declare actual compatibility policy')
        self.commit = self.git('rev-parse', 'HEAD')
        native = json.loads((metadata.ROOT / 'soc/native/soc.json').read_text())
        facts = self.artifacts / 'cpu.json'
        facts.write_text(json.dumps({'schema_version': 1, 'cpu': native['cpu'],
                                     'irq': native['irq'], 'clock_hz': 1000000}))
        return facts

    def test_actual_runtime_generated_metadata_is_bound(self):
        self.prepare_actual_cpu_inputs()
        assembly = self.artifacts / 'runtime.toml'
        assembly.write_text('schema_version = 1\ncpu_facts = "cpu.json"\n'
                            'backend = "baremetal"\noptimization = "Os"\n')
        output = self.artifacts / 'runtime-generated'
        with patch.object(runtime, 'ROOT', self.root):
            runtime.configure(assembly, output)
        record = self.capture()
        configuration = record['configurations'][0]
        self.assertEqual(configuration['validation'], 'valid')
        self.assertEqual({item['label'] for item in configuration['inputs']},
                         {'assembly', 'cpu_facts', 'platform_metadata'})
        navigation = output / 'input_paths.json'
        paths = json.loads(navigation.read_text())
        paths.pop('platform_metadata')
        navigation.write_text(json.dumps(paths))
        with self.assertRaisesRegex(EvidenceError, 'navigation'):
            identity._configuration(output / 'resolved-cpu.json')
        paths['platform_metadata'] = str(self.root / metadata.POLICY)
        navigation.write_text(json.dumps(paths))
        resolved_path = output / 'resolved-cpu.json'
        resolved = json.loads(resolved_path.read_text())
        resolved['platform_metadata']['semantic_version'] = '9.9.9'
        resolved.pop('configuration_sha256')
        resolved['configuration_sha256'] = canonical_digest(resolved)
        resolved_path.write_text(json.dumps(resolved))
        with self.assertRaisesRegex(EvidenceError, 'metadata facts'):
            identity._configuration(resolved_path)

    def test_actual_cpu_cli_metadata_is_bound_without_an_assembly(self):
        facts = self.prepare_actual_cpu_inputs()
        output = self.artifacts / 'cpu-generated'
        with patch.object(cpu, 'ROOT', self.root):
            self.assertEqual(cpu.main(['--facts', str(facts), '--output',
                                      str(output), '--backend', 'baremetal']), 0)
        record = self.capture()
        self.assertEqual({item['label'] for item in
                          record['configurations'][0]['inputs']},
                         {'cpu_facts', 'platform_metadata'})

    def test_artifact_mutation_during_version_execution_is_rejected(self):
        script = self.root / 'build/artifact-mutating-tool'
        script.write_text(
            '#!/bin/sh\nprintf changed > build/software/image.elf\n'
            'printf fixture-version\n')
        script.chmod(0o700)
        with self.assertRaisesRegex(EvidenceError, 'artifact changed'):
            self.capture(tools={'compiler': str(script)})

    def test_new_file_in_published_directory_cannot_escape_its_inventory(self):
        script = self.root / 'build/artifact-adding-tool'
        script.write_text(
            '#!/bin/sh\nprintf late-output > build/software/unobserved.bin\n'
            'printf fixture-version\n')
        script.chmod(0o700)
        with self.assertRaisesRegex(EvidenceError, 'artifact.*changed'):
            self.capture(tools={'compiler': str(script)})

    def test_failed_job_missing_path_cannot_appear_after_inventory(self):
        script = self.root / 'build/artifact-creating-tool'
        script.write_text(
            '#!/bin/sh\nprintf late-output > build/new-output\n'
            'printf fixture-version\n')
        script.chmod(0o700)
        with self.assertRaisesRegex(EvidenceError, 'artifact.*changed'):
            self.capture(result='failure', tools={'compiler': str(script)},
                         artifacts=[self.root / 'build/new-output'])

    def test_identity_inside_published_tree_binds_only_its_own_known_outputs(self):
        record = self.capture(output=self.artifacts / 'job-identity')
        self.assertEqual(len(record['artifacts']), 2)
        self.assertEqual(record['identity_output'], 'build/software/job-identity')

    def test_tool_cannot_add_an_unbound_file_to_identity_outputs(self):
        script = self.root / 'build/identity-adding-tool'
        script.write_text(
            '#!/bin/sh\nprintf late-output > build/identity/unobserved.bin\n'
            'printf fixture-version\n')
        script.chmod(0o700)
        with self.assertRaisesRegex(EvidenceError, 'identity output changed'):
            self.capture(tools={'compiler': str(script)})

    def test_claimed_event_sha_cannot_replace_actual_checkout(self):
        with self.assertRaisesRegex(EvidenceError, 'checkout'):
            self.capture(expected_commit='a' * 40)
        self.assertFalse((self.output / 'identity.json').exists())

    def test_success_rejects_dirty_source(self):
        (self.root / 'dependencies/toolchains.lock.json').write_text('changed')
        with self.assertRaisesRegex(EvidenceError, 'dirty'):
            self.capture()

    def test_failure_keeps_actual_dirty_source_and_missing_outputs(self):
        (self.root / 'new-source.c').write_text('fixture')
        record = self.capture(result='failure', artifacts=[
            self.root / 'build/never-created'])
        self.assertTrue(record['source']['dirty'])
        self.assertEqual(record['job_result'], 'failure')
        self.assertEqual(record['artifacts'], [])
        self.assertEqual(len(record['missing_artifacts']), 1)

    def test_missing_tool_cannot_seal_a_success(self):
        with self.assertRaisesRegex(EvidenceError, 'tool'):
            self.capture(tools={'missing': '/not/a/tool'})

    def test_real_failed_version_process_cannot_seal_success(self):
        script = self.root / 'build/failing-tool'
        script.write_text('#!/bin/sh\nprintf failed-version\nexit 7\n')
        script.chmod(0o700)
        with self.assertRaisesRegex(EvidenceError, 'tool'):
            self.capture(tools={'compiler': str(script)})
        self.assertIn('failed-version',
                      (self.output / 'compiler-version.log').read_text())

    def test_mutation_during_actual_version_process_is_detected(self):
        script = self.root / 'build/mutating-tool'
        script.write_text(
            '#!/bin/sh\nprintf changed > dependencies/toolchains.lock.json\n'
            'printf fixture-version\n')
        script.chmod(0o700)
        with self.assertRaisesRegex(EvidenceError, 'changed'):
            self.capture(tools={'compiler': str(script)})

    def test_tool_cannot_rewrite_its_own_observed_binary(self):
        script = self.root / 'build/self-mutating-tool'
        script.write_text(
            '#!/bin/sh\nprintf "\\n# changed\\n" >> "$0"\n'
            'printf fixture-version\n')
        script.chmod(0o700)
        with self.assertRaisesRegex(EvidenceError, 'changed|identity'):
            self.capture(tools={'compiler': str(script)})

    def test_artifacts_cannot_escape_checkout_or_follow_symlinks(self):
        with self.assertRaisesRegex(EvidenceError, 'checkout'):
            self.capture(artifacts=[Path(sys.executable)])
        link = self.artifacts / 'alias'
        link.symlink_to(self.artifacts / 'image.elf')
        with self.assertRaisesRegex(EvidenceError, 'symlink'):
            self.capture()

    def test_success_requires_actual_nonempty_artifact_set(self):
        with self.assertRaisesRegex(EvidenceError, 'artifact'):
            self.capture(artifacts=[self.root / 'build/missing'])

    def test_parent_traversal_cannot_hash_a_file_outside_checkout(self):
        outside = self.root.parent / ('identity-outside-' + self.root.name)
        outside.write_text('outside fixture bytes')
        self.addCleanup(outside.unlink)
        disguised = self.root / '..' / outside.name
        with self.assertRaisesRegex(EvidenceError, 'checkout'):
            self.capture(artifacts=[disguised])

    def test_partial_dependency_observation_cannot_claim_completion(self):
        (self.root / '.gitmodules').write_text(
            '[submodule "missing"]\n path = vendor/missing\n'
            ' url = https://example.invalid/missing.git\n')
        self.git('add', '.gitmodules')
        self.git('update-index', '--add', '--cacheinfo',
                 '160000', self.commit, 'vendor/missing')
        self.git('commit', '-qm', 'declare uninitialized dependency')
        self.commit = self.git('rev-parse', 'HEAD')
        (self.root / 'vendor/missing').mkdir(parents=True)
        record = self.capture(require_dependencies=False)
        self.assertFalse(record['dependencies_complete'])
        self.output = self.root / 'build/complete-identity'
        with self.assertRaisesRegex(EvidenceError, 'dependenc'):
            self.capture()

    def test_unknown_result_and_duplicate_artifact_selection_are_rejected(self):
        for arguments in (dict(result='passed'), dict(artifacts=[
                self.artifacts, self.artifacts / 'image.elf'])):
            with self.subTest(arguments=arguments):
                with self.assertRaises(EvidenceError):
                    self.capture(**arguments)


if __name__ == '__main__':
    unittest.main()
