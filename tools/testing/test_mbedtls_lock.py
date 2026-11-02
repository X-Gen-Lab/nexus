"""TLS dependency admission uses real Git pins and a closed SDK payload."""
import importlib
import copy
from contextlib import redirect_stderr
import hashlib
import io
import json
import os
import shutil
import subprocess
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.maintenance import source_catalogue as catalogue


ROOT = Path(__file__).resolve().parents[2]
CURATED = ROOT / 'ext/mbedtls'
LOCK_NAME = 'dependencies/mbedtls.lock.json'


class MbedTLSLockTests(unittest.TestCase):
    def test_real_curated_parent_declaration_and_gitlinks_are_selected(self):
        # Read actual immutable committed bytes, not mocked Git return values.
        def git(*arguments):
            return subprocess.check_output(
                ['git', '-C', str(CURATED), *arguments],
                env={key: value for key, value in os.environ.items()
                     if not key.startswith('GIT_')})
        raw = git('show', 'HEAD:.gitmodules')
        records = git('ls-tree', '-r', 'HEAD').decode().splitlines()
        links = {row.split('\t')[1]: row.split()[2] for row in records
                 if row.startswith('160000 ')}
        actual = catalogue.declared_modules(raw, parent='ext/mbedtls')
        self.assertEqual(set(actual), {'framework', 'tf-psa-crypto'})
        self.assertEqual(catalogue.verify_gitlinks(links, 'ext/mbedtls'), links)

    def test_lock_preserves_curated_versions_generated_material_and_licenses(self):
        module = importlib.import_module('tools.maintenance.mbedtls_lock')
        lock = module.load_lock()
        self.assertEqual(lock['version'], '4.1.1')
        self.assertEqual(module.record(lock, 'mbedtls')['commit'],
                         'd840a13e087a8742d5a060a4a4a42a33ac41e3ab')
        self.assertEqual(module.record(lock, 'tf_psa_crypto')['version'], '1.1.1')
        generated = sum(len(value['generated_files'])
                        for value in lock['repositories'].values())
        self.assertEqual(generated, 11)
        for value in lock['repositories'].values():
            self.assertEqual(value['license'],
                             'Apache-2.0 OR GPL-2.0-or-later')
            self.assertIn('LICENSE', value['files'])

    def test_framework_alias_and_selected_only_graph_are_exact_catalogue_data(self):
        mounts = [item for item in catalogue.ALL_DEPENDENCIES
                  if item.source_lock == LOCK_NAME]
        self.assertEqual(len(mounts), 4)
        alias = next(item for item in mounts if item.alias_of is not None)
        self.assertEqual(alias.path, 'ext/mbedtls/tf-psa-crypto/framework')
        raw = subprocess.check_output(['git', '-C',
            str(CURATED / 'tf-psa-crypto'), 'show', 'HEAD:.gitmodules'],
            env={key: value for key, value in os.environ.items()
                 if not key.startswith('GIT_')})
        self.assertEqual(set(catalogue.declared_modules(
            raw, 'ext/mbedtls/tf-psa-crypto')), {'framework'})

    def test_rehashed_omissions_and_changed_generated_material_are_rejected(self):
        module = importlib.import_module('tools.maintenance.mbedtls_lock')
        original = module.load_lock()
        for change in ('license', 'generated', 'commit'):
            with self.subTest(change=change):
                lock = copy.deepcopy(original)
                record = lock['repositories']['tf_psa_crypto']
                if change == 'license':
                    del record['files']['LICENSE']
                elif change == 'generated':
                    record['files']['core/psa_crypto_driver_wrappers.h'] = '0' * 64
                else:
                    record['commit'] = '0' * 40
                with self.assertRaises(ValueError):
                    module.contract(lock)


class MbedTLSActualGitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.module = importlib.import_module('tools.maintenance.mbedtls_lock')
        cls.lock = cls.module.load_lock()

    def setUp(self):
        isolated = patch.dict(os.environ,
            {key: value for key, value in os.environ.items()
             if not key.startswith('GIT_')}, clear=True)
        isolated.start()
        self.addCleanup(isolated.stop)
        self.temporary = tempfile.TemporaryDirectory(prefix='TLS actual Git ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'ext/mbedtls'
        self.lock_path = self.root / LOCK_NAME
        self.lock_path.parent.mkdir()
        self.lock_path.write_text(json.dumps(self.lock))
        for item in self.module.mounts():
            destination = self.root / item.path
            destination.parent.mkdir(parents=True, exist_ok=True)
            self.git(self.root, 'clone', '--shared', '--no-checkout',
                     str(ROOT / item.path), str(destination))
            self.git(destination, 'config', 'remote.origin.url', item.fetch_url)
            self.git(destination, 'sparse-checkout', 'init', '--no-cone')
            record = self.module.record(self.lock, item.identifier)
            names = list(record['files'])
            names += [catalogue.relative_path(child) + '/'
                      for child in catalogue.children(item.path)]
            self.git(destination, 'sparse-checkout', 'set', '--no-cone',
                     '--stdin', input='\n'.join('/' + name for name in names))
            self.git(destination, 'checkout', '--detach', record['commit'])
        self.git(self.root, 'init', '--quiet')
        self.git(self.root, 'config', 'user.name', 'TLS Git fixture')
        self.git(self.root, 'config', 'user.email', 'fixture@example.invalid')
        (self.root / '.gitmodules').write_text(
            '[submodule "ext/mbedtls"]\npath = ext/mbedtls\n'
            'url = https://github.com/X-Gen-Lab/mbedtls.git\n')
        self.git(self.root, 'add', '.gitmodules')
        self.git(self.root, 'update-index', '--add', '--cacheinfo', '160000',
                 self.module.COMMIT, 'ext/mbedtls')
        self.git(self.root, 'commit', '--quiet', '-m', 'Pinned TLS fixture')

    def git(self, directory, *arguments, input=None):
        result = subprocess.run(['git', '-C', str(directory), *arguments],
            input=input, text=True, capture_output=True, check=False, timeout=30,
            env={key: value for key, value in os.environ.items()
                 if not key.startswith('GIT_')})
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def verify(self, **kwargs):
        return self.module.verify(self.source, self.lock_path, **kwargs)

    def test_actual_parent_index_origins_pins_and_aliases_admit_only_source(self):
        result = self.verify()
        self.assertEqual(result['scope'], 'organization_gitlink')
        self.assertEqual(result['verified_mounts'], 4)
        self.assertEqual(result['verified_files'], 386)
        self.assertFalse(result['software_qualified'])

    def test_inventory_observes_all_four_real_mounts_through_one_source_lock(self):
        from tools.maintenance import dependency_proposals as proposals
        auxiliary = ('dependencies/toolchains.lock.json',
            'dependencies/actions.lock.json', 'dependencies/environment.lock.json',
            'dependencies/development-tools.txt', 'dependencies/environment-tools.txt',
            'vendors/gigadevice/gd32f4xx/source.lock.json')
        for name in auxiliary:
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        shutil.copytree(ROOT / 'vendors/gigadevice/gd32f4xx/LICENSES',
                        self.root / 'vendors/gigadevice/gd32f4xx/LICENSES')
        self.git(self.root, 'add', 'dependencies', 'vendors')
        self.git(self.root, 'commit', '--quiet', '-m', 'Inventory inputs')
        # Scope this real Git fixture to TLS, without replacing any verifier.
        mounts = self.module.mounts()
        with patch.object(catalogue, 'ALL_DEPENDENCIES', mounts), \
                patch.object(proposals, 'ALL_DEPENDENCIES', mounts):
            result = proposals.inventory(self.root)
        observed = [row for row in result['dependencies'] if 'path' in row]
        self.assertEqual({row['path'] for row in observed},
                         {item.path for item in mounts})
        for row in observed:
            record = self.module.record(self.lock, row['path'])
            self.assertEqual(row['revision'], record['commit'])
            self.assertEqual(row['upstream_revision'], record['upstream_commit'])
            identities = {item['path'] for item in row['identities']}
            self.assertIn(str(self.lock_path), identities)
            self.assertTrue({str(self.root / row['path'] / name)
                             for name in record['files']}.issubset(identities))

    def test_changed_parent_index_and_wrong_org_origin_reject(self):
        self.git(self.root, 'update-index', '--cacheinfo', '160000',
                 '0a8fda272a5a0abef3b47c91bed37185d5a726b1', 'ext/mbedtls')
        with self.assertRaisesRegex(ValueError, 'Gitlink'):
            self.verify()
        self.git(self.root, 'update-index', '--cacheinfo', '160000',
                 self.module.COMMIT, 'ext/mbedtls')
        self.git(self.source, 'config', 'remote.origin.url',
                 'https://github.com/Mbed-TLS/mbedtls.git')
        with self.assertRaisesRegex(ValueError, 'origin'):
            self.verify()
        result = self.verify(development=True)
        self.assertFalse(result['production_admitted'])

    def test_changed_selected_alias_and_dirty_material_reject(self):
        alias = self.source / 'tf-psa-crypto/framework'
        self.git(alias, '-c', 'user.name=TLS alias fixture', '-c',
                 'user.email=fixture@example.invalid', 'commit', '--allow-empty',
                 '--quiet', '-m', 'Unexpected selected alias revision')
        with self.assertRaisesRegex(ValueError, 'dirty|commit|tree'):
            self.verify()
        pin = self.module.record(self.lock, 'mbedtls_framework')['commit']
        self.git(alias, 'checkout', '--detach', pin)
        material = self.source / 'library/error.c'
        material.write_bytes(material.read_bytes() + b'changed\n')
        with self.assertRaisesRegex(ValueError, 'dirty|bytes'):
            self.verify()

    def test_symlink_source_or_git_marker_is_never_an_own_checkout(self):
        alias = self.root / 'alias'
        alias.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'canonical|regular|symlink'):
            self.module.verify(alias, self.lock_path, development=True)
        marker = self.source / '.git'
        backing = self.root / 'backing-git'
        marker.rename(backing)
        marker.symlink_to(backing, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'regular|symlink|checkout'):
            self.verify()

    def test_git_timeout_retains_a_cli_rejection_without_an_unhandled_traceback(self):
        error = io.StringIO()
        with patch.object(self.module, 'git', side_effect=
                subprocess.TimeoutExpired(['git'], 20)), redirect_stderr(error):
            status = self.module.main([
                '--source', str(self.source), '--lock', str(self.lock_path)])
        self.assertEqual(status, 1)
        self.assertIn('TLS source rejected:', error.getvalue())


class MbedTLSSDKPayloadTests(unittest.TestCase):
    def setUp(self):
        self.module = importlib.import_module('tools.maintenance.mbedtls_lock')
        self.lock = self.module.load_lock()
        self.temporary = tempfile.TemporaryDirectory(prefix='TLS SDK bytes ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'ext/mbedtls'
        self.lock_path = self.root / LOCK_NAME
        self.lock_path.parent.mkdir()
        self.lock_path.write_text(json.dumps(self.lock))
        self.records = []
        for item in self.module.mounts():
            record = self.module.record(self.lock, item.identifier)
            self.records.append({'path': item.path, 'commit': record['commit'],
                                 'tree': record['tree']})
            for name in record['files']:
                destination = self.root / item.path / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / item.path / name, destination)

    def verify(self):
        return self.module.verify(self.source, self.lock_path, source_sdk=True,
                                  dependency_records=self.records)

    def test_source_sdk_is_exact_closed_bytes_with_bound_four_mount_records(self):
        result = self.verify()
        self.assertEqual(result['scope'], 'source_sdk_payload')
        self.assertEqual(result['verified_files'], 386)
        self.records[-1]['tree'] = '0' * 40
        with self.assertRaisesRegex(ValueError, 'identity'):
            self.verify()

    def test_no_unselected_driver_or_rehashed_missing_material_can_enter_sdk(self):
        unexpected = self.source / 'tf-psa-crypto/drivers/pqcp/mldsa-native/code.c'
        unexpected.parent.mkdir(parents=True)
        unexpected.write_text('unselected material\n')
        with self.assertRaisesRegex(ValueError, 'closure'):
            self.verify()
        shutil.rmtree(unexpected.parent)
        missing = self.source / 'tf-psa-crypto/core/psa_crypto_driver_wrappers.h'
        missing.unlink()
        with self.assertRaises(ValueError):
            self.verify()

    def test_sdk_packager_derives_required_files_and_selection_from_same_lock(self):
        path = ROOT / 'cmake/package/package_source_sdk.py'
        specification = importlib.util.spec_from_file_location('tls_sdk', path)
        sdk = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(sdk)
        for item in self.module.mounts():
            files = set(self.module.record(self.lock, item.identifier)['files'])
            entries = sdk.dependency_entries(ROOT, item.path)
            self.assertEqual(set(entries), files)
            self.assertTrue({item.path + '/' + name for name in files}.issubset(
                sdk.REQUIRED_FILES))


if __name__ == '__main__':
    unittest.main()
