"""A real nested Git checkout must retain its own identity inside a hook."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from tools.configure import configure

class DependencyHookEnvironmentTests(unittest.TestCase):
    def test_nested_git_identity_ignores_parent_hook_repository(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            child = root / 'vendor'
            def git(path, *args):
                return subprocess.check_output(['git', '-C', str(path), *args], text=True).strip()
            for path in (root, child):
                path.mkdir(exist_ok=True)
                git(path, 'init', '-q')
                git(path, 'config', 'user.name', 'Fixture')
                git(path, 'config', 'user.email', 'fixture@local.invalid')
            (child / 'vendor.c').write_text('/* committed vendor bytes */\n')
            git(child, 'add', 'vendor.c')
            git(child, 'commit', '-qm', 'fixture: nested checkout')
            commit = git(child, 'rev-parse', 'HEAD')
            git(root, 'update-index', '--add', '--cacheinfo', '160000,' + commit + ',vendor')
            git(root, 'commit', '-qm', 'fixture: parent gitlink')
            inherited = {'GIT_DIR': str(root / '.git'),
                         'GIT_WORK_TREE': str(root),
                         'GIT_INDEX_FILE': str(root / '.git/index')}
            with patch.dict(os.environ, inherited):
                actual = configure.dependency_identity(root, 'vendor', commit)
                self.assertEqual(actual, {'path': 'vendor', 'commit': commit})
                self.assertEqual({key: os.environ[key] for key in inherited}, inherited)
            (child / 'vendor.c').write_text('/* dirty vendor bytes */\n')
            with patch.dict(os.environ, inherited):
                with self.assertRaisesRegex(configure.ConfigurationError, 'unrecorded'):
                    configure.dependency_identity(root, 'vendor', commit)

if __name__ == '__main__':
    unittest.main(verbosity=2)
