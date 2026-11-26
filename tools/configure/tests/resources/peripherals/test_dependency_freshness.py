"""Observe fresh Git state with a bounded number of child processes."""
import os
import subprocess
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.configure import configure


class DependencyIdentityFreshnessTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.child = self.root / 'vendor'
        for path in (self.root, self.child):
            path.mkdir(exist_ok=True)
            self.git(path, 'init', '-q')
            self.git(path, 'config', 'user.name', 'Fixture')
            self.git(path, 'config', 'user.email', 'fixture@local.invalid')
        self.source = self.child / 'vendor.c'
        self.source.write_text('/* reviewed bytes */\n')
        self.git(self.child, 'add', 'vendor.c')
        self.git(self.child, 'commit', '-qm', 'fixture: child')
        self.commit = self.git(self.child, 'rev-parse', 'HEAD')
        self.lock(self.commit)

    def git(self, directory, *args, input=None):
        return subprocess.check_output(
            ['git', '-C', str(directory), *args], text=True,
            input=input).strip()

    def stage(self, commit):
        self.git(self.root, 'update-index', '--add', '--cacheinfo',
                 '160000,' + commit + ',vendor')

    def lock(self, commit):
        self.stage(commit)
        self.git(self.root, 'commit', '-qm', 'fixture: locked child')

    def observe(self, expected=None):
        return configure.dependency_identity(self.root, 'vendor', expected)

    def test_one_observation_uses_at_most_three_git_processes(self):
        with patch.object(configure.subprocess, 'run',
                          wraps=subprocess.run) as calls:
            result = self.observe(self.commit)
        self.assertEqual(result['commit'], self.commit)
        self.assertLessEqual(calls.call_count, 3)

    def test_dirty_and_restored_bytes_are_reobserved_on_every_call(self):
        self.observe()
        self.source.write_text('/* dirty bytes */\n')
        with self.assertRaisesRegex(configure.ConfigurationError, 'unrecorded'):
            self.observe()
        self.source.write_text('/* reviewed bytes */\n')
        self.assertEqual(self.observe()['commit'], self.commit)
        (self.child / 'untracked.c').write_text('/* unrecorded */\n')
        with self.assertRaisesRegex(configure.ConfigurationError, 'unrecorded'):
            self.observe()

    def test_head_and_parent_gitlink_changes_are_reobserved(self):
        self.observe()
        self.source.write_text('/* next reviewed bytes */\n')
        self.git(self.child, 'commit', '-qam', 'fixture: next child')
        next_commit = self.git(self.child, 'rev-parse', 'HEAD')
        with self.assertRaisesRegex(configure.ConfigurationError, 'Gitlink'):
            self.observe()
        self.lock(next_commit)
        self.assertEqual(self.observe(next_commit)['commit'], next_commit)
        with self.assertRaisesRegex(configure.ConfigurationError,
                                    'contradicts'):
            self.observe(self.commit)

    def test_explicit_staged_gitlink_update_needs_no_parent_commit(self):
        head = self.git(self.root, 'rev-parse', 'HEAD')
        self.source.write_text('/* next reviewed bytes */\n')
        self.git(self.child, 'commit', '-qam', 'fixture: staged child')
        next_commit = self.git(self.child, 'rev-parse', 'HEAD')
        with self.assertRaisesRegex(configure.ConfigurationError, 'Gitlink'):
            self.observe()
        self.stage(next_commit)
        index = self.git(self.root, 'ls-files', '--stage')
        self.assertEqual(self.observe(next_commit)['commit'], next_commit)
        self.assertEqual(self.git(self.root, 'rev-parse', 'HEAD'), head)
        self.assertEqual(self.git(self.root, 'ls-files', '--stage'), index)
        with self.assertRaisesRegex(configure.ConfigurationError,
                                    'contradicts'):
            self.observe(self.commit)
        self.source.write_text('/* unrecorded staged dependency bytes */\n')
        with self.assertRaisesRegex(configure.ConfigurationError, 'unrecorded'):
            self.observe(next_commit)

    def test_explicit_staged_gitlink_addition_needs_no_parent_commit(self):
        self.git(self.root, 'update-index', '--force-remove', 'vendor')
        self.git(self.root, 'commit', '-qm', 'fixture: no locked child')
        head = self.git(self.root, 'rev-parse', 'HEAD')
        with self.assertRaisesRegex(configure.ConfigurationError, 'Gitlink'):
            self.observe()
        self.stage(self.commit)
        self.assertEqual(self.observe(self.commit)['commit'], self.commit)
        self.assertEqual(self.git(self.root, 'rev-parse', 'HEAD'), head)

    def test_unmerged_index_cannot_authorize_dependency(self):
        self.git(self.root, 'update-index', '--index-info', input=(
            f'0 {"0" * len(self.commit)}\tvendor\n'
            f'160000 {self.commit} 1\tvendor\n'
            f'160000 {self.commit} 2\tvendor\n'))
        index = self.git(self.root, 'ls-files', '--stage')
        with self.assertRaisesRegex(configure.ConfigurationError, 'Gitlink'):
            self.observe()
        self.assertEqual(self.git(self.root, 'ls-files', '--stage'), index)

    def test_staged_gitlink_path_is_literal_and_nul_delimited(self):
        relative = 'vendor [reviewed]\tpath'
        self.child.rename(self.root / relative)
        self.git(self.root, 'update-index', '--add', '--cacheinfo',
                 '160000,' + self.commit + ',' + relative)
        actual = configure.dependency_identity(self.root, relative, self.commit)
        self.assertEqual(actual, {'path': relative, 'commit': self.commit})

    def test_regular_index_entry_cannot_authorize_dependency(self):
        blob = self.git(self.root, 'hash-object', '-w', '--stdin',
                        input='not a Gitlink\n')
        self.git(self.root, 'update-index', '--cacheinfo',
                 '100644,' + blob + ',vendor')
        with self.assertRaisesRegex(configure.ConfigurationError, 'Gitlink'):
            self.observe()

    def test_inherited_alternate_index_cannot_authorize_unstaged_pin(self):
        inherited_index = self.root / 'hook-index'
        inherited_index.write_bytes((self.root / '.git/index').read_bytes())
        self.source.write_text('/* next reviewed bytes */\n')
        self.git(self.child, 'commit', '-qam', 'fixture: alternate index child')
        next_commit = self.git(self.child, 'rev-parse', 'HEAD')
        inherited = {'GIT_DIR': str(self.root / '.git'),
                     'GIT_WORK_TREE': str(self.root),
                     'GIT_INDEX_FILE': str(inherited_index)}
        subprocess.check_output(
            ['git', '-C', str(self.root), 'update-index', '--cacheinfo',
             '160000,' + next_commit + ',vendor'],
            env={**os.environ, **inherited})
        with patch.dict(os.environ, inherited):
            with self.assertRaisesRegex(configure.ConfigurationError, 'Gitlink'):
                self.observe()
        self.stage(next_commit)
        with patch.dict(os.environ, inherited):
            self.assertEqual(self.observe(next_commit)['commit'], next_commit)
            self.assertEqual({key: os.environ[key] for key in inherited},
                             inherited)

    def test_failed_query_cannot_reuse_a_previous_observation(self):
        self.observe()
        real = subprocess.run
        def run(command, **kwargs):
            if 'status' in command:
                return subprocess.CompletedProcess(command, 128, '', 'failed')
            return real(command, **kwargs)
        with patch.object(configure.subprocess, 'run', side_effect=run):
            with self.assertRaisesRegex(configure.ConfigurationError,
                                        'Cannot verify'):
                self.observe()


if __name__ == '__main__':
    unittest.main(verbosity=2)
