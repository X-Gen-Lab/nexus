"""The optional MQTT source has one exact, selected-only dependency authority."""
import importlib
import importlib.util
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.maintenance import source_catalogue as catalogue


ROOT = Path(__file__).resolve().parents[2]


class CoreMQTTLockTests(unittest.TestCase):
    def test_runtime_record_admits_actual_selected_only_curated_leaf(self):
        source = ROOT / 'ext/coremqtt'
        raw = subprocess.check_output(['git', '-C', str(source),
                                      'ls-tree', '-r', 'HEAD'],
            env={key: value for key, value in os.environ.items()
                 if not key.startswith('GIT_')}).decode()
        links = {row.split('\t')[1]: row.split()[2] for row in raw.splitlines()
                 if row.startswith('160000 ')}
        selected = catalogue.dependency('coremqtt')
        self.assertEqual(selected.path, 'ext/coremqtt')
        self.assertEqual(selected.scope, 'runtime')
        self.assertEqual(catalogue.verify_gitlinks(links, selected.path), {})

    def test_exact_lock_has_unchanged_c11_sources_mit_and_curation_notice(self):
        module = importlib.import_module('tools.maintenance.coremqtt_lock')
        lock = module.contract(json.loads(module.LOCK.read_text()))
        self.assertEqual(lock['version'], '2.3.1')
        self.assertEqual(lock['commit'],
                         'f4445aa70cb211504bca2c9e4177cd74609f5fba')
        self.assertEqual(lock['upstream_commit'],
                         '2beef04725328923e05e576b884212d53ec97af7')
        self.assertEqual(lock['license'], 'MIT')
        self.assertEqual(len(lock['files']), 10)
        self.assertIn('LICENSE', lock['files'])
        self.assertIn('NEXUS-SOURCE-CURATION.md', lock['files'])

    def test_rehashed_omission_and_wrong_curated_metadata_are_rejected(self):
        module = importlib.import_module('tools.maintenance.coremqtt_lock')
        original = json.loads(module.LOCK.read_text())
        for key in ('source/core_mqtt.c', 'LICENSE', 'NEXUS-SOURCE-CURATION.md'):
            changed = copy.deepcopy(original)
            del changed['files'][key]
            with self.subTest(key=key), self.assertRaises(ValueError):
                module.contract(changed)
        changed = copy.deepcopy(original)
        changed['commit'] = changed['upstream_commit']
        with self.assertRaises(ValueError):
            module.contract(changed)


class CoreMQTTGitAndSDKTests(unittest.TestCase):
    def setUp(self):
        isolated = patch.dict(os.environ,
            {key: value for key, value in os.environ.items()
             if not key.startswith('GIT_')}, clear=True)
        isolated.start()
        self.addCleanup(isolated.stop)
        self.module = importlib.import_module('tools.maintenance.coremqtt_lock')
        self.lock = self.module.contract(json.loads(self.module.LOCK.read_text()))
        self.temporary = tempfile.TemporaryDirectory(prefix='coreMQTT Git ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'ext/coremqtt'
        self.lock_path = self.root / 'dependencies/coremqtt.lock.json'
        self.lock_path.parent.mkdir(parents=True)
        self.lock_path.write_text(json.dumps(self.lock))
        self.source.parent.mkdir()
        self.git(self.root, 'clone', '--shared', '--no-checkout',
                 str(ROOT / 'ext/coremqtt'), str(self.source))
        self.git(self.source, 'sparse-checkout', 'init', '--no-cone')
        self.git(self.source, 'sparse-checkout', 'set', '--no-cone', '--stdin',
                 input='\n'.join('/' + name for name in self.lock['files']))
        self.git(self.source, 'checkout', '--detach', self.module.COMMIT)
        self.git(self.source, 'config', 'remote.origin.url', self.module.FETCH)
        self.git(self.root, 'init', '--quiet')
        self.git(self.root, 'config', 'user.name', 'coreMQTT fixture')
        self.git(self.root, 'config', 'user.email', 'fixture@example.invalid')
        (self.root / '.gitmodules').write_text(
            '[submodule "ext/coremqtt"]\npath = ext/coremqtt\nurl = '
            + self.module.FETCH + '\n')
        self.git(self.root, 'add', '.gitmodules')
        self.git(self.root, 'update-index', '--add', '--cacheinfo', '160000',
                 self.module.COMMIT, 'ext/coremqtt')
        self.git(self.root, 'commit', '--quiet', '-m', 'Pinned MQTT fixture')

    def git(self, directory, *arguments, input=None):
        result = subprocess.run(['git', '-C', str(directory), *arguments],
            input=input, capture_output=True, text=True, check=False, timeout=30,
            env={key: value for key, value in os.environ.items()
                 if not key.startswith('GIT_')})
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def verify(self, **kwargs):
        return self.module.verify(self.source, self.lock_path, **kwargs)

    def test_actual_git_admission_is_exact_source_not_protocol_qualification(self):
        report = self.verify()
        self.assertEqual(report['scope'], 'organization_gitlink')
        self.assertEqual(report['verified_files'], 10)
        self.assertFalse(report['software_qualified'])
        self.git(self.root, 'update-index', '--cacheinfo', '160000',
                 self.lock['upstream_commit'], 'ext/coremqtt')
        with self.assertRaisesRegex(ValueError, 'gitlink'):
            self.verify()

    def test_foreign_origin_dirty_material_and_source_alias_are_rejected(self):
        self.git(self.source, 'config', 'remote.origin.url', self.module.UPSTREAM)
        with self.assertRaisesRegex(ValueError, 'origin'):
            self.verify()
        self.assertFalse(self.verify(development=True)['production_admitted'])
        self.git(self.source, 'config', 'remote.origin.url', self.module.FETCH)
        source = self.source / 'source/core_mqtt.c'
        self.git(self.source, 'update-index', '--assume-unchanged',
                 'source/core_mqtt.c')
        source.write_bytes(source.read_bytes() + b'changed\n')
        self.assertEqual(self.git(self.source, 'status', '--porcelain'), '')
        with self.assertRaisesRegex(ValueError, 'bytes'):
            self.verify()
        alias = self.root / 'alias'
        alias.symlink_to(self.source, target_is_directory=True)
        with self.assertRaises(ValueError):
            self.module.verify(alias, self.lock_path, development=True)

    def sdk_payload(self):
        root = self.root / 'installed'
        source = root / 'ext/coremqtt'
        lock = root / 'dependencies/coremqtt.lock.json'
        lock.parent.mkdir(parents=True)
        lock.write_text(json.dumps(self.lock))
        for name in self.lock['files']:
            destination = source / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / 'ext/coremqtt' / name, destination)
        return source, lock

    def test_sdk_closure_cannot_admit_cmock_or_missing_algorithm_and_notice(self):
        source, lock = self.sdk_payload()
        self.assertEqual(self.module.verify(source, lock, source_sdk=True)[
                         'verified_files'], 10)
        extra = source / 'test/unit-test/CMock/code.c'
        extra.parent.mkdir(parents=True)
        extra.write_text('Excluded upstream test dependency\n')
        with self.assertRaisesRegex(ValueError, 'closure'):
            self.module.verify(source, lock, source_sdk=True)
        shutil.rmtree(source / 'test')
        (source / 'source/core_mqtt_serializer.c').unlink()
        with self.assertRaises(ValueError):
            self.module.verify(source, lock, source_sdk=True)

    def test_sdk_selection_and_required_sources_share_exact_locked_files(self):
        specification = importlib.util.spec_from_file_location('mqtt_sdk',
            ROOT / 'cmake/package/package_source_sdk.py')
        sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(sdk)
        self.assertEqual(set(sdk.dependency_entries(ROOT, 'ext/coremqtt')),
                         set(self.lock['files']))
        self.assertTrue({'ext/coremqtt/' + name for name in self.lock['files']}
                        .issubset(sdk.required_files(ROOT, sdk.source_profile(
                            components=("mqtt",)))))

    def test_inventory_retains_the_official_parent_and_current_closed_sources(self):
        from tools.maintenance import dependency_proposals as proposals
        names = ('dependencies/toolchains.lock.json',
            'dependencies/actions.lock.json', 'dependencies/environment.lock.json',
            'dependencies/development-tools.txt', 'dependencies/environment-tools.txt',
            'vendors/gigadevice/gd32f4xx/source.lock.json')
        for name in names:
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        shutil.copytree(ROOT / 'vendors/gigadevice/gd32f4xx/LICENSES',
                        self.root / 'vendors/gigadevice/gd32f4xx/LICENSES')
        self.git(self.root, 'add', 'dependencies', 'vendors')
        self.git(self.root, 'commit', '--quiet', '-m', 'Inventory observations')
        item = catalogue.dependency('coremqtt')
        with patch.object(catalogue, 'ALL_DEPENDENCIES', (item,)), \
                patch.object(proposals, 'ALL_DEPENDENCIES', (item,)):
            report = proposals.inventory(self.root)
        actual = next(row for row in report['dependencies']
                      if row['id'] == 'coremqtt')
        self.assertEqual(actual['revision'], self.module.COMMIT)
        self.assertEqual(actual['upstream_revision'], self.lock['upstream_commit'])
        paths = {row['path'] for row in actual['identities']}
        self.assertTrue({str(self.source / name) for name in self.lock['files']}
                        .issubset(paths))


if __name__ == '__main__':
    unittest.main()
