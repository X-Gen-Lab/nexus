"""Install the exact official ARM GNU distribution from its reviewed lock.

Explicit dependency preparation only: CMake never downloads toolchains.
An existing installation is verified, never overwritten. Only the original
archive is cached; its digest is checked before every extraction. Paths and
links must stay within its expected root.
"""

import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import stat
import subprocess
import tarfile
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def load_lock(path):
    document = json.loads(path.read_text())
    if document.get('schema_version') != 1:
        raise ValueError('Unsupported toolchain lock schema')
    entry = document['arm_gnu']
    if (entry['host'] != 'x86_64-linux' or entry['target'] != 'arm-none-eabi' or
            len(entry['sha256']) != 64 or
            any(c not in '0123456789abcdef' for c in entry['sha256'])):
        raise ValueError('Invalid ARM toolchain identity')
    return entry


def verify_archive(archive, expected):
    digest = hashlib.sha256()
    with archive.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    if digest.hexdigest() != expected:
        raise ValueError('ARM toolchain SHA-256 does not match the lock')


def extract_archive(archive, directory, expected_root):
    def checked_path(name):
        path = PurePosixPath(name)
        if (path.is_absolute() or '..' in path.parts or not path.parts or
                path.parts[0] != expected_root):
            raise ValueError(f'Unsafe toolchain archive path: {name}')
        return path

    with tarfile.open(archive, 'r:*') as package:
        members = package.getmembers()
        for member in members:
            path = checked_path(member.name)
            if member.issym() or member.islnk():
                target = PurePosixPath(member.linkname)
                base = path.parent if member.issym() else PurePosixPath()
                if target.is_absolute():
                    raise ValueError('Absolute toolchain archive link')
                resolved = (directory / str(base / target)).resolve()
                root = (directory / expected_root).resolve()
                if not resolved.is_relative_to(root):
                    raise ValueError('Toolchain archive link escapes its root')
            elif not (member.isfile() or member.isdir()):
                raise ValueError('Unsupported toolchain archive member')
        # data filtering also rejects paths that traverse a previously created
        # symlink. It is available in the maintained Python 3.11 security update.
        package.extractall(directory, members=members, filter='data')


def check_compiler(directory, version):
    compiler = directory / 'bin/arm-none-eabi-gcc'
    result = subprocess.run([str(compiler), '--version'], text=True,
                            capture_output=True, timeout=15, check=True)
    if f'Arm GNU Toolchain {version}'.casefold() not in result.stdout.casefold():
        raise ValueError('Installed compiler version does not match the lock')


def prepare_archive(archive, entry):
    """Recover an untrusted cache, publishing only complete verified bytes."""
    archive.parent.mkdir(parents=True, exist_ok=True)
    if archive.is_symlink():
        archive.unlink()
    if archive.exists():
        if not archive.is_file():
            raise ValueError('ARM toolchain cache entry is not a regular file')
        try:
            verify_archive(archive, entry['sha256'])
            return
        except ValueError:
            archive.unlink()
    for attempt in range(3):
        try:
            with tempfile.TemporaryDirectory(prefix='.nexus-download-',
                                             dir=archive.parent) as temporary:
                pending = Path(temporary) / 'toolchain.tar.xz'
                with urllib.request.urlopen(entry['url'], timeout=60) as source:
                    with pending.open('wb') as output:
                        shutil.copyfileobj(source, output, length=1024 * 1024)
                verify_archive(pending, entry['sha256'])
                pending.replace(archive)
            return
        except (OSError, ValueError, http.client.HTTPException):
            if attempt == 2:
                raise
            time.sleep(2 ** attempt)


def verify_installation(destination, expected):
    """Compare an installation with freshly extracted, hash-verified input."""
    actual_paths = {path.relative_to(destination): path
                    for path in destination.rglob('*')}
    expected_paths = {path.relative_to(expected): path
                      for path in expected.rglob('*')}
    if actual_paths.keys() != expected_paths.keys():
        raise ValueError('Changed installed toolchain member set')
    for relative, source in expected_paths.items():
        target = actual_paths[relative]
        source_stat, target_stat = source.lstat(), target.lstat()
        if (stat.S_IFMT(source_stat.st_mode) != stat.S_IFMT(target_stat.st_mode)
                or stat.S_IMODE(source_stat.st_mode) !=
                stat.S_IMODE(target_stat.st_mode)):
            raise ValueError(f'Changed installed toolchain member: {relative}')
        if source.is_symlink():
            same = os.readlink(source) == os.readlink(target)
        elif source.is_file():
            same = source_stat.st_size == target_stat.st_size
            if same:
                with source.open('rb') as left, target.open('rb') as right:
                    while True:
                        chunk = left.read(1024 * 1024)
                        if chunk != right.read(1024 * 1024):
                            same = False
                            break
                        if not chunk:
                            break
        else:
            same = source.is_dir()
        if not same:
            raise ValueError(f'Changed installed toolchain member: {relative}')


def install(destination, entry, cache_dir=None):
    marker = destination / '.nexus-toolchain-sha256'
    if destination.exists() or destination.is_symlink():
        if (destination.is_symlink() or marker.is_symlink() or
                not marker.is_file() or
                marker.read_text().strip() != entry['sha256']):
            raise ValueError(
                'Refusing to overwrite an unrelated toolchain directory')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.nexus-toolchain-',
                                     dir=destination.parent) as temporary:
        work = Path(temporary)
        cache = cache_dir if cache_dir is not None else work
        archive = cache / (entry['sha256'] + '.tar.xz')
        prepare_archive(archive, entry)
        extract_archive(archive, work, entry['archive_root'])
        extracted = work / entry['archive_root']
        check_compiler(extracted, entry['version'])
        (extracted / '.nexus-toolchain-sha256').write_text(entry['sha256'] + '\n')
        if destination.exists():
            verify_installation(destination, extracted)
            check_compiler(destination, entry['version'])
        else:
            extracted.rename(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install-dir', type=Path, required=True)
    parser.add_argument('--cache-dir', type=Path,
                        help='Cache original archives; every hit is reverified')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'amd64'):
        parser.error('The maintained toolchain lock requires x86_64 Linux')
    try:
        install(args.install_dir.absolute(),
                load_lock(ROOT / 'dependencies/toolchains.lock.json'),
                cache_dir=args.cache_dir)
    except (OSError, ValueError, KeyError, tarfile.TarError,
            http.client.HTTPException, subprocess.SubprocessError) as error:
        parser.exit(1, f'ARM toolchain preparation failed: {error}\n')
    print(args.install_dir.resolve() / 'bin')


if __name__ == '__main__':
    main()
