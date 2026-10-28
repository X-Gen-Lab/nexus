"""Bind selected CI scopes to real recursive Git and configuration closures."""

from dataclasses import replace
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.dev.dependencies import prepare_preset
from tools.evidence.common import EvidenceError, file_identity
from tools.evidence.identity import canonical_digest
from tools.maintenance.job_identity import capture
from tools.maintenance import source_catalogue
from tools.maintenance.source_catalogue import (
    ALL_SOURCE_DEPENDENCIES, SOURCE_DEPENDENCIES, children, dependency,
    module_name, relative_path,
)


class SelectedIdentityTests(unittest.TestCase):
    def setUp(self):
        baseline = tuple(replace(item, fetch_url=item.upstream_url)
                         for item in source_catalogue.ALL_DEPENDENCIES)
        policy = patch.object(source_catalogue, "ALL_DEPENDENCIES", baseline)
        policy.start()
        self.addCleanup(policy.stop)
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.area = Path(self.directory.name)
        self.root = self.area / 'source'
        self.root.mkdir()
        self.environment = {key: value for key, value in os.environ.items()
                            if not key.startswith("GIT_")}
        self.environment.update(GIT_AUTHOR_NAME='Fixture',
            GIT_AUTHOR_EMAIL='fixture@example.invalid',
            GIT_COMMITTER_NAME='Fixture',
            GIT_COMMITTER_EMAIL='fixture@example.invalid')
        self.module = self.area / 'module'
        self.module.mkdir()
        self.git(self.module, 'init', '-q')
        (self.module / 'source.txt').write_text('fixture source\n')
        self.git(self.module, 'add', '.')
        self.git(self.module, 'commit', '-qm', 'fixture module')
        self.module_sha = self.git(self.module, 'rev-parse', 'HEAD')
        self.freertos = self.area / 'freertos'
        self.freertos.mkdir()
        self.git(self.freertos, 'init', '-q')
        nested = children('ext/freertos')
        (self.freertos / '.gitmodules').write_text(''.join(
            '[submodule "' + module_name(item) + '"]\n path = '
            + relative_path(item) + '\n url = ' + item.fetch_url + '\n'
            for item in nested))
        self.git(self.freertos, 'add', '.')
        for item in nested:
            self.git(self.freertos, 'update-index', '--add', '--cacheinfo',
                     '160000', self.module_sha, relative_path(item))
        self.git(self.freertos, 'commit', '-qm', 'fixture recursive module')
        self.freertos_sha = self.git(self.freertos, 'rev-parse', 'HEAD')
        self.git(self.root, 'init', '-q')
        (self.root / '.gitignore').write_text('build/\n')
        (self.root / 'dependencies').mkdir()
        (self.root / 'dependencies/fixture.lock.json').write_text('{}\n')
        declarations = []
        for item in ALL_SOURCE_DEPENDENCIES:
            declarations.append('[submodule "' + item.path + '"]\n path = '
                                + item.path + '\n url = ' + item.upstream_url + '\n')
            (self.root / item.path).mkdir(parents=True)
        (self.root / '.gitmodules').write_text(''.join(declarations))
        (self.root / 'board').mkdir()
        (self.root / 'board/board.json').write_text(
            '{"soc_family":"stm32f407"}')
        self.assembly = self.root / 'assembly.toml'
        self.assembly.write_text('schema = 2\nboard_package = "board"\n'
                                 'backend = "baremetal"\n')
        (self.root / 'CMakePresets.json').write_text(json.dumps({
            'version': 6, 'configurePresets': [{'name': 'fixture',
                'generator': 'Ninja', 'binaryDir': '${sourceDir}/build/fixture',
                'cacheVariables': {'NEXUS_ASSEMBLY_FILE':
                                   '${sourceDir}/assembly.toml'}}]}))
        self.git(self.root, 'add', '.')
        for item in ALL_SOURCE_DEPENDENCIES:
            sha = self.freertos_sha if item.path == 'ext/freertos' else self.module_sha
            self.git(self.root, 'update-index', '--add', '--cacheinfo',
                     '160000', sha, item.path)
        self.commit_source()
        # These synthetic modules exercise selected recursive identity only;
        # they never qualify official vendor bytes or physical firmware.
        self.initialize('vendors/arm/CMSIS_6', 'vendors/st/cmsis_device_f4',
                        'vendors/st/stm32f4_hal')
        self.artifacts = self.root / 'build/arm'
        self.artifacts.mkdir(parents=True)
        (self.artifacts / 'image.elf').write_bytes(b'fixture image bytes')
        self.receipt_path = self.root / 'build/setup-dependencies.json'
        self.refresh_receipt()
        self.write_configuration()

    def git(self, root, *arguments):
        return subprocess.check_output(['git', '-C', str(root), *arguments],
            env=self.environment, text=True, stderr=subprocess.STDOUT,
            timeout=15).strip()

    def commit_source(self):
        self.git(self.root, 'add', '.')
        self.git(self.root, 'commit', '-qm', 'fixture platform')
        self.commit = self.git(self.root, 'rev-parse', 'HEAD')

    def initialize(self, *modules):
        for module in modules:
            source = self.freertos if module == 'ext/freertos' else self.module
            self.git(self.root, 'config', 'submodule.' + module + '.url', str(source))
            self.git(self.root, '-c', 'protocol.file.allow=always', 'submodule',
                     'update', '--init', '--', module)
            checkout = self.root / module
            item = dependency(module)
            self.git(checkout, 'remote', 'set-url', 'origin', item.fetch_url)
            self.git(self.root, 'config', 'submodule.' + module + '.url',
                     item.fetch_url)
            for child in children(module):
                key = 'submodule.' + module_name(child) + '.url'
                self.git(checkout, 'config', key, str(self.module))
                self.git(checkout, '-c', 'protocol.file.allow=always', 'submodule',
                         'update', '--init', '--', relative_path(child))
                self.git(self.root / child.path, 'remote', 'set-url', 'origin',
                         child.fetch_url)
                self.git(checkout, 'config', key, child.fetch_url)

    def refresh_receipt(self):
        self.receipt = prepare_preset(self.root, 'fixture')
        self.receipt_path.write_text(json.dumps(self.receipt))

    def write_configuration(self, assembly=None):
        paths = [assembly or self.assembly, self.root / 'board/board.json']
        inputs = [{'path': path.name, 'sha256': file_identity(path)['sha256']}
                  for path in paths]
        resolved = {'schema_version': 1, 'inputs': inputs}
        resolved['configuration_sha256'] = canonical_digest(resolved)
        (self.artifacts / 'resolved.json').write_text(json.dumps(resolved))
        (self.artifacts / 'input_paths.json').write_text(json.dumps({
            path.name: str(path) for path in paths}))

    def capture(self, **changes):
        arguments = dict(root=self.root, job='arm.fixture', result='success',
            expected_commit=self.commit, artifacts=[self.artifacts],
            tools={'python': sys.executable}, output=self.root / 'build/identity',
            require_dependencies=True, dependency_preparation=self.receipt_path,
            expected_preset='fixture')
        arguments.update(changes)
        return capture(**arguments)

    def test_complete_selected_arm_with_missing_unselected_modules_is_bound(self):
        record = self.capture()
        self.assertFalse(record['dependencies_complete'])
        self.assertTrue(record['selected_dependencies_complete'])
        self.assertTrue(record['dependencies_required'])
        self.assertEqual(len(record['dependency_scope']['recursive_submodules']), 3)
        self.assertEqual({item['path'] for item in
                          record['dependency_scope']['recursive_submodules']},
                         {'vendors/arm/CMSIS_6', 'vendors/st/cmsis_device_f4',
                          'vendors/st/stm32f4_hal'})
        self.assertEqual(record['dependency_scope']['assembly'],
                         file_identity(self.assembly))

    def test_default_all_required_does_not_accept_the_arm_subset(self):
        with self.assertRaises(EvidenceError):
            self.capture(dependency_preparation=None, expected_preset=None)

    def test_wrong_expected_preset_and_forged_receipt_are_rejected(self):
        with self.assertRaises(EvidenceError):
            self.capture(expected_preset='other')
        self.receipt['family'] = 'stm32h743'
        self.receipt_path.write_text(json.dumps(self.receipt))
        with self.assertRaises(EvidenceError):
            self.capture()

    def test_missing_or_modified_selected_module_is_rejected(self):
        self.git(self.root, 'submodule', 'deinit', '-f', '--',
                 'vendors/st/cmsis_device_f4')
        with self.assertRaises(EvidenceError):
            self.capture()
        self.initialize('vendors/st/cmsis_device_f4')
        (self.root / 'vendors/st/cmsis_device_f4/source.txt').write_text('changed')
        with self.assertRaises(EvidenceError):
            self.capture()

    def test_unknown_source_cannot_be_falsely_declared_verified(self):
        self.receipt['recursive_submodules'].append({
            'path': 'vendors/unknown', 'commit': self.module_sha})
        self.receipt_path.write_text(json.dumps(self.receipt))
        with self.assertRaises(EvidenceError):
            self.capture()

    def test_missing_resolved_and_other_valid_assembly_are_rejected(self):
        (self.artifacts / 'resolved.json').unlink()
        with self.assertRaises(EvidenceError):
            self.capture()
        other = self.root / 'build/other.toml'
        other.write_text('schema = 2\nbackend = "freertos"\n')
        self.write_configuration(other)
        with self.assertRaises(EvidenceError):
            self.capture(output=self.root / 'build/other-identity')

    def test_nested_selected_closure_is_required_and_recorded(self):
        self.assembly.write_text(self.assembly.read_text().replace(
            'baremetal', 'freertos'))
        self.commit_source()
        self.initialize('ext/freertos')
        self.refresh_receipt()
        self.write_configuration()
        self.git(self.root / 'ext/freertos', 'submodule', 'deinit', '-f', '--',
                 relative_path(children('ext/freertos')[0]))
        with self.assertRaises(EvidenceError):
            self.capture()
        self.initialize('ext/freertos')
        record = self.capture()
        paths = {item['path'] for item in
                 record['dependency_scope']['recursive_submodules']}
        for child in children('ext/freertos'):
            self.assertIn(child.path, paths)

    def test_failure_observation_never_claims_bad_scope_verified(self):
        self.receipt['scope'] = 'unknown'
        self.receipt_path.write_text(json.dumps(self.receipt))
        record = self.capture(result='failure')
        self.assertFalse(record['selected_dependencies_complete'])
        self.assertEqual(record['dependency_scope']['validation'], 'invalid')

    def test_native_requires_every_declared_recursive_source(self):
        (self.root / 'board/board.json').write_text('{"soc_family":"native"}')
        self.assembly.write_text(self.assembly.read_text().replace(
            'baremetal', 'native'))
        self.commit_source()
        with self.assertRaises(EvidenceError):
            prepare_preset(self.root, 'fixture')
        self.initialize(*(item.path for item in SOURCE_DEPENDENCIES))
        with self.assertRaisesRegex(EvidenceError, 'ext/googletest'):
            prepare_preset(self.root, 'fixture')
        self.initialize('ext/googletest')
        self.refresh_receipt()
        self.write_configuration()
        record = self.capture()
        self.assertTrue(record['dependencies_complete'])
        self.assertTrue(record['selected_dependencies_complete'])
        self.assertEqual(record['dependency_scope']['scope'], 'native')
        self.assertGreater(len(record['dependency_scope']['recursive_submodules']),
                           len(ALL_SOURCE_DEPENDENCIES))

    def test_valid_baremetal_receipt_cannot_bind_freertos_preset_configuration(self):
        (self.root / 'other.toml').write_text(
            self.assembly.read_text().replace('baremetal', 'freertos'))
        document = json.loads((self.root / 'CMakePresets.json').read_text())
        other = json.loads(json.dumps(document['configurePresets'][0]))
        other['name'] = 'freertos'
        other['cacheVariables']['NEXUS_ASSEMBLY_FILE'] = '${sourceDir}/other.toml'
        document['configurePresets'].append(other)
        (self.root / 'CMakePresets.json').write_text(json.dumps(document))
        self.commit_source()
        self.write_configuration(self.root / 'other.toml')
        with self.assertRaises(EvidenceError):
            self.capture(expected_preset='freertos')

    def test_old_board_schema_bool_and_receipt_mutation_cannot_be_verified(self):
        self.receipt['schema_version'] = True
        self.receipt_path.write_text(json.dumps(self.receipt))
        with self.assertRaises(EvidenceError):
            self.capture()
        self.refresh_receipt()
        (self.root / 'board/board.json').write_text(
            '{"soc_family":"stm32h743"}')
        self.commit_source()
        with self.assertRaises(EvidenceError):
            self.capture()

    def test_each_configuration_must_match_the_selected_scope(self):
        other = self.artifacts / 'other'
        other.mkdir()
        assembly = self.root / 'build/unselected.toml'
        assembly.write_text('schema = 2\nbackend = "freertos"\n')
        resolved = {'schema_version': 1, 'inputs': [{
            'path': 'unselected', 'sha256': file_identity(assembly)['sha256']}]}
        resolved['configuration_sha256'] = canonical_digest(resolved)
        (other / 'resolved.json').write_text(json.dumps(resolved))
        (other / 'input_paths.json').write_text(json.dumps({
            'unselected': str(assembly)}))
        with self.assertRaises(EvidenceError):
            self.capture()

    def test_arm_workflow_binds_and_uploads_its_actual_preparation_receipt(self):
        repository = Path(__file__).resolve().parents[2]
        workflow = (repository / '.github/workflows/build-matrix.yml').read_text()
        arm = workflow.split('  arm:\n', 1)[1].split('  controller-routes:', 1)[0]
        self.assertIn("require-dependencies: 'true'", arm)
        self.assertIn('dependency-preparation: build/setup-dependencies.json', arm)
        self.assertIn('dependency-preset: ${{ matrix.preset }}', arm)
        self.assertEqual(arm.count('            build/setup-dependencies.json'), 2)


if __name__ == '__main__':
    unittest.main()
