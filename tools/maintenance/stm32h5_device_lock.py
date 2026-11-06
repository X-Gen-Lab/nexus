#!/usr/bin/env python3
"""Prove the fixed H563 official device compile-reference source closure.

STM32H563xx is the vendor header selector, not a Board or ordering-code claim.
The shared snapshot and Git proof rules apply without adding a supported SoC.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __name__ == '__main__':
    sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import fields, parse_json  # noqa: E402
from tools.maintenance import cmsis_device_lock as shared  # noqa: E402

COMMIT = '884b8dc78e41cbfca008363342b17f4a9e8641f7'
DIRECTORY = 'vendors/st/cmsis_device_h5'
LOCK_NAME = 'dependencies/stm32h5.lock.json'
FILES = frozenset({
    'Include/stm32h563xx.h', 'Include/stm32h5xx.h',
    'Include/system_stm32h5xx.h', 'LICENSE.md',
    'Source/Templates/gcc/startup_stm32h563xx.s',
    'Source/Templates/system_stm32h5xx.c',
})
REVIEW = {
    'origin': 'https://github.com/STMicroelectronics/cmsis-device-h5.git',
    'commit': COMMIT, 'directory': DIRECTORY, 'lock_name': LOCK_NAME,
    'files': FILES,
}


def contract(lock):
    """Require the exact header selector, original notice and reviewed graph."""
    fields(lock, {'schema_version', 'kind', 'repository', 'commit',
                  'selected_part', 'files', 'license'}, 'H563 device lock')
    if (type(lock['schema_version']) is not int or
            lock['schema_version'] != 1 or
            lock['kind'] != 'nexus-reviewed-device-source' or
            lock['repository'] != REVIEW['origin'] or
            lock['commit'] != COMMIT or
            lock['selected_part'] != 'STM32H563xx'):
        raise ValueError('Exact reviewed H563 device compile source required')
    if not isinstance(lock['files'], dict) or set(lock['files']) != FILES:
        raise ValueError('H563 device identities must close the reviewed graph')
    for identity in lock['files'].values():
        fields(identity, {'bytes', 'sha256'}, 'H563 file identity')
        if (type(identity['bytes']) is not int or identity['bytes'] <= 0 or
                not isinstance(identity['sha256'], str) or
                not re.fullmatch(r'[0-9a-f]{64}', identity['sha256'])):
            raise ValueError('Nonempty exact H563 device identity required')
    if lock['license'] != {'path': 'LICENSE.md',
                           'expression': 'Apache-2.0'}:
        raise ValueError('Original H563 Apache-2.0 notice required')
    return FILES


def verify(source, lock_path, *, packaged_commit=None):
    """Verify clean Git or the exact commit from a fully verified source SDK.

    A package caller must first verify its entire installed SDK manifest. A
    missing Git marker alone never authorizes byte-only source acceptance.
    """
    source = Path(source).absolute()
    if (source.resolve() != source or source.is_symlink() or
            not source.is_dir()):
        raise ValueError('H563 device source requires a canonical directory')
    raw, lock_identity = shared.stable_snapshot(lock_path)
    lock = parse_json(raw)
    selected = contract(lock)
    if packaged_commit is None:
        shared.verify_git(source, lock, REVIEW)
    elif (packaged_commit != COMMIT or (source / '.git').exists() or
          (source / '.git').is_symlink()):
        raise ValueError('Packaged H563 source needs the exact SDK commit')
    observed = {}
    for name in sorted(selected):
        data, identity = shared.stable_snapshot(source / name)
        expected = lock['files'][name]
        if (len(data) != expected['bytes'] or
                hashlib.sha256(data).hexdigest() != expected['sha256']):
            raise ValueError('H563 source/license identity changed: ' + name)
        observed[name] = identity
    if packaged_commit is None:
        shared.verify_git(source, lock, REVIEW)
    if (shared.stable_snapshot(lock_path)[1] != lock_identity or any(
            shared.stable_snapshot(source / name)[1] != identity
            for name, identity in observed.items())):
        raise ValueError('H563 source or lock changed during verification')
    return {'kind': 'nexus-cmsis-device-source-verification', 'commit': COMMIT,
            'selected_part': 'STM32H563xx', 'verified_files': len(selected),
            'license': 'Apache-2.0', 'software_qualified': False,
            'source_kind': 'installed_source_sdk' if packaged_commit is not None
            else 'git_checkout'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--lock', required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        source, lock = args.source.absolute(), args.lock.absolute()
        _, lock_identity = shared.stable_snapshot(lock)
        sdk = source.parents[2]
        manifest_path = sdk / '.nexus-source-sdk.json'
        manifest_identity, packaged = None, None
        if manifest_path.exists() or manifest_path.is_symlink():
            if source != sdk / DIRECTORY or lock != sdk / LOCK_NAME:
                raise ValueError('H563 device requires exact SDK source paths')
            _, manifest_identity = shared.stable_snapshot(manifest_path)
            sys.path.insert(0, str(ROOT / 'cmake/package'))
            import package_source_sdk
            manifest = package_source_sdk.verify(sdk)
            records = [row for row in manifest['dependencies']
                       if row['path'] == DIRECTORY]
            if len(records) != 1:
                raise ValueError('SDK lacks the exact H563 device dependency')
            packaged = records[0]['commit']
        result = verify(source, lock, packaged_commit=packaged)
        if (shared.stable_snapshot(lock)[1] != lock_identity or
                (manifest_identity is not None and
                 shared.stable_snapshot(manifest_path)[1] != manifest_identity)):
            raise ValueError('H563 lock or SDK manifest changed during proof')
    except (OSError, ValueError, TypeError, KeyError, IndexError, ImportError,
            subprocess.SubprocessError) as error:
        print('H563 device source rejected: ' + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
