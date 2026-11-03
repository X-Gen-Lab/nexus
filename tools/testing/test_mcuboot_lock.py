"""Selected MCUboot bytes, ownership and whole-SDK trust stay separate."""
from pathlib import Path
import contextlib
import hashlib
import io
import importlib.util
import json
import os
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.maintenance import mcuboot_lock, source_catalogue


ROOT = Path(__file__).resolve().parents[2]
CURATED = Path(os.environ.get('NEXUS_MCUBOOT_TEST_SOURCE',
                              ROOT / 'ext/mcuboot'))


class MCUbootImportTests(unittest.TestCase):
    def test_library_import_preserves_each_callers_bytecode_policy(self):
        # Copy only the entry module: its imported authorities are already
        # loaded. A legitimate False policy may cache this temporary copy.
        original_flag, original_path = sys.dont_write_bytecode, sys.path[:]
        try:
            with tempfile.TemporaryDirectory() as area:
                source = Path(area) / 'tools/maintenance/mcuboot_probe.py'
                source.parent.mkdir(parents=True)
                shutil.copyfile(ROOT / 'tools/maintenance/mcuboot_lock.py', source)
                for flag in (False, True):
                    with self.subTest(flag=flag):
                        sys.dont_write_bytecode = flag
                        spec = importlib.util.spec_from_file_location(
                            'mcuboot_library_policy_probe', source)
                        module = importlib.util.module_from_spec(spec)
                        spec.loader.exec_module(module)
                        self.assertIs(sys.dont_write_bytecode, flag)
        finally:
            sys.dont_write_bytecode = original_flag
            sys.path[:] = original_path


class MCUbootLockTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='MCUboot admitted ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.item = source_catalogue.dependency('mcuboot')
        self.source = self.root / self.item.path
        self.lock_path = self.root / self.item.source_lock
        self.lock_path.parent.mkdir()
        self.lock = json.loads((ROOT / self.item.source_lock).read_text())
        self.lock_path.write_text(json.dumps(self.lock))
        for name in self.lock['files']:
            target = self.source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(CURATED / name, target)
        self.common = patch('tools.dev.dependencies.verify_selected_checkout',
                            return_value={'path': self.item.path,
                                          'commit': self.lock['commit'],
                                          'tree': self.lock['tree'],
                                          'scope': 'organization_gitlink'})
        self.checkout = self.common.start()
        self.addCleanup(self.common.stop)
        self.manifest = self.root / '.nexus-source-sdk.json'
        self.manifest.write_text('{"fixture": "owned SDK verifier is mocked"}')
        self.sdk = {'dependencies': [{'path': self.item.path,
                                     'commit': self.lock['commit'],
                                     'tree': self.lock['tree']}]}

    def verify(self, **flags):
        return mcuboot_lock.verify(self.source, self.lock_path, **flags)

    def main(self, verifier):
        with patch.object(mcuboot_lock, 'verify_sdk', verifier), \
                contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()):
            return mcuboot_lock.main([
                '--source', str(self.source), '--lock', str(self.lock_path),
                '--source-sdk'])

    def test_ordinary_git_admission_delegates_two_observations_to_common_boundary(self):
        result = self.verify()
        self.assertTrue(result['production_admitted'])
        self.assertEqual(result['verified_files'], 80)
        self.assertEqual(self.checkout.call_count, 2)
        for observed in self.checkout.call_args_list:
            self.assertTrue(observed.kwargs['parent_index'])
            self.assertFalse(observed.kwargs['development'])

    def test_rehashed_modified_file_map_cannot_redefine_reviewed_source(self):
        target = self.source / 'boot/bootutil/src/loader.c'
        target.write_bytes(target.read_bytes() + b'\n')
        self.lock['files']['boot/bootutil/src/loader.c'] = \
            hashlib.sha256(target.read_bytes()).hexdigest()
        self.lock_path.write_text(json.dumps(self.lock))
        with self.assertRaises(ValueError):
            self.verify(source_sdk=True)

    def test_changed_license_rejects_without_git_as_payload_admission(self):
        (self.source / 'ext/tinycrypt/LICENSE').write_text('different license')
        with self.assertRaises(ValueError):
            self.verify(source_sdk=True)

    def test_missing_selected_c_file_rejects(self):
        (self.source / 'boot/bootutil/src/loader.c').unlink()
        with self.assertRaises((ValueError, OSError)):
            self.verify(source_sdk=True)

    def test_selected_file_alias_rejects_even_if_bytes_match(self):
        target = self.source / 'NOTICE'
        target.unlink()
        target.symlink_to(CURATED / 'NOTICE')
        with self.assertRaises(ValueError):
            self.verify(source_sdk=True)

    def test_extra_sdk_file_rejects_exact_payload_closure(self):
        (self.source / 'unselected.c').write_text('int unselected;')
        with self.assertRaises(ValueError):
            self.verify(source_sdk=True)

    def test_extra_sdk_directory_alias_rejects(self):
        (self.source / 'unselected').symlink_to(CURATED,
                                                 target_is_directory=True)
        with self.assertRaises(ValueError):
            self.verify(source_sdk=True)

    def test_development_and_sdk_scopes_cannot_be_combined(self):
        with self.assertRaises(ValueError):
            self.verify(development=True, source_sdk=True)

    def test_payload_only_check_cannot_claim_whole_sdk_admission(self):
        result = self.verify(source_sdk=True)
        self.assertFalse(result['production_admitted'])
        self.checkout.assert_not_called()

    def test_sdk_cli_runs_own_verifier_before_git_free_payload_admission(self):
        order = []
        original = mcuboot_lock.verify

        def own_sdk(root):
            self.assertEqual(root, self.root)
            order.append('sdk')
            return self.sdk

        def payload(*args, **kwargs):
            order.append('payload')
            return original(*args, **kwargs)

        with patch.object(mcuboot_lock, 'verify', side_effect=payload):
            self.assertEqual(self.main(own_sdk), 0)
        self.assertEqual(order, ['sdk', 'payload'])
        self.checkout.assert_not_called()

    def test_failed_sdk_owner_stops_before_payload_verification(self):
        def reject(root):
            raise ValueError('own SDK verification failed')
        with patch.object(mcuboot_lock, 'verify') as payload:
            self.assertEqual(self.main(reject), 1)
        payload.assert_not_called()

    def test_sdk_manifest_must_select_the_exact_commit_and_tree(self):
        self.sdk['dependencies'][0]['tree'] = '1' * 40
        with patch.object(mcuboot_lock, 'verify') as payload:
            self.assertEqual(self.main(lambda root: self.sdk), 1)
        payload.assert_not_called()

    def test_changed_sdk_manifest_cannot_inherit_earlier_owner_proof(self):
        def change(root):
            self.manifest.write_text(self.manifest.read_text() + '\n')
            return self.sdk
        self.assertEqual(self.main(change), 1)

    def test_source_changed_during_observation_rejects(self):
        selected = self.source / 'boot/bootutil/src/loader.c'
        original = mcuboot_lock.snapshot
        changed = False

        def observe(path):
            nonlocal changed
            result = original(path)
            if Path(path) == selected and not changed:
                selected.write_bytes(selected.read_bytes() + b'\n')
                changed = True
            return result

        with patch.object(mcuboot_lock, 'snapshot', side_effect=observe), \
                self.assertRaises(ValueError):
            self.verify(source_sdk=True)


if __name__ == '__main__':
    unittest.main()
