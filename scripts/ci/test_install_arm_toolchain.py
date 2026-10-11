"""Reject corrupted archives, escaping paths and unrelated installations."""

import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest

try:
    from . import install_arm_toolchain as installer
except ImportError:
    import install_arm_toolchain as installer


class ArmToolchainTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)
        self.archive = self.work / 'archive.tar'
        self.root = 'locked-toolchain'

    def package(self, name, link=None):
        with tarfile.open(self.archive, 'w') as archive:
            info = tarfile.TarInfo(name)
            if link is None:
                content = b'locked compiler data'
                info.size = len(content)
                archive.addfile(info, io.BytesIO(content))
            else:
                info.type = tarfile.SYMTYPE
                info.linkname = link
                archive.addfile(info)

    def test_exact_digest_and_normal_file_are_accepted(self):
        self.package(self.root + '/bin/compiler')
        digest = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        installer.verify_archive(self.archive, digest)
        output = self.work / 'extracted'
        output.mkdir()
        installer.extract_archive(self.archive, output, self.root)
        self.assertEqual((output / self.root / 'bin/compiler').read_bytes(),
                         b'locked compiler data')

    def test_modified_archive_is_rejected_before_extraction(self):
        self.package(self.root + '/bin/compiler')
        digest = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.archive.write_bytes(self.archive.read_bytes() + b'tampered')
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            installer.verify_archive(self.archive, digest)

    def test_archive_paths_cannot_escape_or_change_the_locked_root(self):
        for name in ('../outside', '/absolute', self.root + '/../outside',
                     'different-root/bin/compiler'):
            with self.subTest(name=name):
                self.package(name)
                with self.assertRaises(ValueError):
                    installer.extract_archive(self.archive, self.work, self.root)
        self.assertFalse((self.work.parent / 'outside').exists())

    def test_symbolic_link_cannot_escape_the_root(self):
        self.package(self.root + '/bin/compiler', '../../outside')
        with self.assertRaisesRegex(ValueError, 'escapes'):
            installer.extract_archive(self.archive, self.work, self.root)

    def test_unrelated_directory_is_never_overwritten(self):
        destination = self.work / 'existing'
        destination.mkdir()
        sentinel = destination / 'keep.txt'
        sentinel.write_text('preserve')
        with self.assertRaisesRegex(ValueError, 'Refusing to overwrite'):
            installer.install(destination, {'sha256': '0' * 64})
        self.assertEqual(sentinel.read_text(), 'preserve')

    def test_lock_rejects_invalid_identity(self):
        path = self.work / 'lock.json'
        path.write_text(json.dumps({'schema_version': 1, 'arm_gnu': {
            'host': 'x86_64-linux', 'target': 'arm-none-eabi', 'sha256': 'floating'}}))
        with self.assertRaisesRegex(ValueError, 'identity'):
            installer.load_lock(path)


if __name__ == '__main__':
    unittest.main()
