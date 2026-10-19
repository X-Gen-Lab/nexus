"""Reject corrupted archives, escaping paths and unrelated installations."""

import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock
import urllib.error

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

    def compiler_package(self, version='14.3.rel1', links=False):
        with tarfile.open(self.archive, 'w') as archive:
            content = ('#!/bin/sh\nprintf "Arm GNU Toolchain ' + version +
                       '\\n"\n').encode()
            binary = '/bin/compiler' if links else '/bin/arm-none-eabi-gcc'
            info = tarfile.TarInfo(self.root + binary)
            info.size = len(content)
            info.mode = 0o755
            archive.addfile(info, io.BytesIO(content))
            if links:
                info = tarfile.TarInfo(self.root + '/bin/arm-none-eabi-gcc')
                info.type, info.linkname = tarfile.SYMTYPE, 'compiler'
                archive.addfile(info)
                info = tarfile.TarInfo(self.root + '/bin/compiler-hardlink')
                info.type = tarfile.LNKTYPE
                info.linkname, info.mode = self.root + binary, 0o755
                archive.addfile(info)
        return {
            'version': '14.3.rel1', 'url': self.archive.as_uri(),
            'archive_root': self.root,
            'sha256': hashlib.sha256(self.archive.read_bytes()).hexdigest(),
        }

    def cached_archive(self, entry):
        return self.work / 'cache' / (entry['sha256'] + '.tar.xz')

    def test_transient_download_failure_retries_before_atomic_install(self):
        entry = self.compiler_package()
        destination = self.work / 'installed'
        with self.archive.open('rb') as source:
            with mock.patch.object(installer.urllib.request, 'urlopen',
                                   side_effect=[urllib.error.URLError('temporary'),
                                                source]) as download:
                with mock.patch('time.sleep'):
                    installer.install(destination, entry,
                                      cache_dir=self.work / 'cache')
        self.assertEqual(download.call_count, 2)
        self.assertEqual(self.cached_archive(entry).read_bytes(),
                         self.archive.read_bytes())
        self.assertTrue((destination / 'bin/arm-none-eabi-gcc').is_file())

    def test_interrupted_download_exhausts_bound_without_partial_publication(self):
        entry = self.compiler_package()
        destination = self.work / 'installed'

        class InterruptedDownload(io.BytesIO):
            def read(self, size=-1):
                if self.tell():
                    raise OSError('connection interrupted')
                return super().read(7)

        def interrupted(*args, **kwargs):
            return InterruptedDownload(self.archive.read_bytes())

        with mock.patch.object(installer.urllib.request, 'urlopen',
                               side_effect=interrupted) as download:
            with mock.patch('time.sleep'):
                with self.assertRaisesRegex(OSError, 'interrupted'):
                    installer.install(destination, entry,
                                      cache_dir=self.work / 'cache')
        self.assertEqual(download.call_count, 3)
        self.assertFalse(destination.exists())
        self.assertEqual(list((self.work / 'cache').iterdir()), [])
        self.assertEqual(list(self.work.glob('.nexus-toolchain-*')), [])

    def test_keyboard_interrupt_cancels_once_without_partial_publication(self):
        entry = self.compiler_package()
        destination = self.work / 'installed'
        cache = self.work / 'cache'
        with mock.patch.object(installer.urllib.request, 'urlopen',
                               side_effect=KeyboardInterrupt) as download:
            with self.assertRaises(KeyboardInterrupt):
                installer.install(destination, entry, cache_dir=cache)
        self.assertEqual(download.call_count, 1)
        self.assertFalse(destination.exists())
        self.assertEqual(list(cache.iterdir()), [])
        self.assertEqual(list(self.work.glob('.nexus-toolchain-*')), [])

    def test_verified_archive_is_reused_without_downloading(self):
        entry = self.compiler_package()
        cache = self.work / 'cache'
        installer.install(self.work / 'first', entry, cache_dir=cache)
        with mock.patch.object(installer.urllib.request, 'urlopen',
                               side_effect=AssertionError('must use cache')):
            installer.install(self.work / 'second', entry, cache_dir=cache)
        self.assertEqual((self.work / 'second/bin/arm-none-eabi-gcc').read_bytes(),
                         (self.work / 'first/bin/arm-none-eabi-gcc').read_bytes())

    def test_corrupt_cached_archive_is_replaced_only_after_verified_download(self):
        entry = self.compiler_package()
        archive = self.cached_archive(entry)
        archive.parent.mkdir()
        archive.write_bytes(b'corrupt restored cache')
        installer.install(self.work / 'installed', entry,
                          cache_dir=archive.parent)
        self.assertEqual(archive.read_bytes(), self.archive.read_bytes())

    def test_modified_cache_hit_is_rehashed_and_never_executed(self):
        entry = self.compiler_package()
        cache = self.work / 'cache'
        installer.install(self.work / 'first', entry, cache_dir=cache)
        self.cached_archive(entry).write_bytes(b'modified after first install')
        with mock.patch.object(installer.urllib.request, 'urlopen',
                               side_effect=urllib.error.URLError('offline')):
            with mock.patch('time.sleep'):
                with self.assertRaises(urllib.error.URLError):
                    installer.install(self.work / 'second', entry,
                                      cache_dir=cache)
        self.assertFalse((self.work / 'second').exists())
        self.assertFalse(self.cached_archive(entry).exists())

    def test_wrong_download_digest_never_publishes_cache_or_destination(self):
        entry = self.compiler_package()
        entry['sha256'] = '0' * 64
        with mock.patch('time.sleep'):
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                installer.install(self.work / 'installed', entry,
                                  cache_dir=self.work / 'cache')
        self.assertFalse((self.work / 'installed').exists())
        self.assertEqual(list((self.work / 'cache').iterdir()), [])

    def test_matching_marker_cannot_hide_changed_installed_compiler(self):
        entry = self.compiler_package()
        destination = self.work / 'installed'
        installer.install(destination, entry, cache_dir=self.work / 'cache')
        compiler = destination / 'bin/arm-none-eabi-gcc'
        compiler.write_bytes(compiler.read_bytes() + b'# changed\n')
        modified = compiler.read_bytes()
        with self.assertRaisesRegex(ValueError, 'installed toolchain'):
            installer.install(destination, entry, cache_dir=self.work / 'cache')
        self.assertEqual(compiler.read_bytes(), modified)

    def test_existing_installation_rejects_added_members_without_overwriting(self):
        entry = self.compiler_package()
        destination = self.work / 'installed'
        installer.install(destination, entry, cache_dir=self.work / 'cache')
        added = destination / 'extra'
        added.write_text('preserve')
        with self.assertRaisesRegex(ValueError, 'installed toolchain'):
            installer.install(destination, entry, cache_dir=self.work / 'cache')
        self.assertEqual(added.read_text(), 'preserve')

    def test_existing_verified_installation_is_idempotent(self):
        entry = self.compiler_package()
        destination = self.work / 'installed'
        installer.install(destination, entry, cache_dir=self.work / 'cache')
        before = (destination / 'bin/arm-none-eabi-gcc').stat()
        installer.install(destination, entry, cache_dir=self.work / 'cache')
        after = (destination / 'bin/arm-none-eabi-gcc').stat()
        self.assertEqual((before.st_ino, before.st_mtime_ns),
                         (after.st_ino, after.st_mtime_ns))

    def test_safe_archive_links_survive_installation_and_reverification(self):
        entry = self.compiler_package(links=True)
        destination = self.work / 'installed'
        installer.install(destination, entry, cache_dir=self.work / 'cache')
        installer.install(destination, entry, cache_dir=self.work / 'cache')
        compiler = destination / 'bin/compiler'
        self.assertEqual((destination / 'bin/arm-none-eabi-gcc').readlink(),
                         Path('compiler'))
        self.assertEqual((destination / 'bin/compiler-hardlink').stat().st_ino,
                         compiler.stat().st_ino)

    def test_matching_marker_rejects_changed_member_type_or_permissions(self):
        entry = self.compiler_package()
        for change in ('symlink', 'permissions'):
            with self.subTest(change=change):
                destination = self.work / change
                installer.install(destination, entry,
                                  cache_dir=self.work / 'cache')
                compiler = destination / 'bin/arm-none-eabi-gcc'
                if change == 'symlink':
                    compiler.unlink()
                    compiler.symlink_to(self.work / 'outside')
                else:
                    compiler.chmod(0o777)
                with self.assertRaisesRegex(ValueError, 'installed toolchain'):
                    installer.install(destination, entry,
                                      cache_dir=self.work / 'cache')

    def test_wrong_real_compiler_version_is_not_installed_from_valid_cache(self):
        entry = self.compiler_package(version='incorrect')
        cache = self.work / 'cache'
        cache.mkdir()
        self.cached_archive(entry).write_bytes(self.archive.read_bytes())
        with self.assertRaisesRegex(ValueError, 'compiler version'):
            installer.install(self.work / 'installed', entry, cache_dir=cache)
        self.assertFalse((self.work / 'installed').exists())
        self.assertEqual(list(self.work.glob('.nexus-toolchain-*')), [])

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
