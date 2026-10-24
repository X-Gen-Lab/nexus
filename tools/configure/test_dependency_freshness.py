"""Observe fresh Git state with a bounded number of child processes."""
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

    def git(self, directory, *args):
        return subprocess.check_output(
            ['git', '-C', str(directory), *args], text=True).strip()

    def lock(self, commit):
        self.git(self.root, 'update-index', '--add', '--cacheinfo',
                 '160000,' + commit + ',vendor')
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
