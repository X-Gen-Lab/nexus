"""Behavioral regressions for the tracked Python compilation gate."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import check_python_syntax as gate


class PythonSyntaxTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / 'module.py'
        self.source.write_text('value = 1\n')

    def index(self, data):
        with patch.object(gate.subprocess, 'run', return_value=subprocess.CompletedProcess(
                ['git'], 0, stdout=data, stderr=b'')):
            return gate.tracked_python(self.root)

    def test_owned_source_compiles_without_executing(self):
        self.source.write_text('raise RuntimeError("must not execute")\n')
        self.assertEqual(gate.check_sources(self.root, [Path('module.py')]), [])

    def test_parse_and_compile_errors_are_rejected(self):
        for text in ('/* invalid Python */\n', 'return 1\n'):
            self.source.write_text(text)
            self.assertEqual(len(gate.check_sources(self.root, [Path('module.py')])), 1)

    def test_zero_scope_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'zero owned'):
            self.index(b'')
        with self.assertRaisesRegex(ValueError, 'zero owned'):
            gate.check_sources(self.root, [])

    def test_submodule_and_untracked_sources_are_not_traversed(self):
        dependency = self.root / 'vendor.py'
        dependency.mkdir()
        (dependency / 'broken.py').write_text('/* dependency */\n')
        (self.root / 'untracked.py').write_text('/* untracked */\n')
        paths = self.index(b'160000 abc 0\tvendor.py\0'
                           b'100644 def 0\tmodule.py\0')
        self.assertEqual(paths, [Path('module.py')])
        self.assertEqual(gate.check_sources(self.root, paths), [])

    def test_missing_source_and_symlinks_are_rejected(self):
        self.source.unlink()
        self.assertEqual(len(gate.check_sources(self.root, [Path('module.py')])), 1)
        outside = self.root / 'outside'
        outside.write_text('value = 1\n')
        self.source.symlink_to(outside)
        self.assertEqual(len(gate.check_sources(self.root, [Path('module.py')])), 1)

    def test_invalid_index_scope_is_rejected(self):
        for record in (b'broken\0', b'120000 abc 0\tmodule.py\0',
                       b'100644 abc 2\tmodule.py\0', b'100644 abc 0\t../module.py\0'):
            with self.assertRaises(ValueError):
                self.index(record)


if __name__ == '__main__':
    unittest.main()
