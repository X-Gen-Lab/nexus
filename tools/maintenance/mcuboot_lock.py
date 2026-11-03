#!/usr/bin/env python3
"""Verify one reviewed MCUboot profile through shared source and SDK boundaries."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

if __name__ == "__main__":
    sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import fields, parse_json, snapshot  # noqa: E402
from tools.dev import dependencies  # noqa: E402
from tools.maintenance import source_catalogue  # noqa: E402

ITEM = source_catalogue.dependency('mcuboot')
LOCK = ROOT / ITEM.source_lock
COMMIT = '62471ebd5825ef05b7c10922a15ed70f8ade9426'
TREE = 'aefe4e15fe0e2a04205ea38b9cec45a40c556735'
REVIEWED_FILE_MAP_SHA256 = (
    '0ebd5342b64bd1c9415d657f3de86e1d8f124d16f6201634abfb1072a1d1247c'
)


def contract(lock):
    """Review identity is fixed; the catalogue owns path and transport facts."""
    fields(lock, {'schema', 'name', 'version', 'commit', 'tree',
                  'upstream_commit', 'upstream_url', 'fetch_url', 'path',
                  'license', 'admission', 'files'}, 'MCUboot source lock')
    expected = {'name': ITEM.identifier, 'version': '2.4.0', 'commit': COMMIT,
                'tree': TREE, 'upstream_commit':
                '6d3b3d2c38ab20c242e5b9abb04d050086383eb2',
                'upstream_url': ITEM.upstream_url, 'fetch_url': ITEM.fetch_url,
                'path': ITEM.path,
                'license': 'Apache-2.0 AND BSD-3-Clause AND BSD-2-Clause',
                'admission': 'organization_gitlink'}
    if (type(lock['schema']) is not int or lock['schema'] != 1 or
            any(lock[name] != value for name, value in expected.items())):
        raise ValueError('Exact reviewed MCUboot source identity required')
    files = lock['files']
    if not isinstance(files, dict) or not files:
        raise ValueError('MCUboot selected source files are required')
    for name, digest in files.items():
        if (not isinstance(name, str) or not name or '\\' in name or
                PurePosixPath(name).is_absolute() or
                any(piece in ('', '.', '..') for piece in name.split('/')) or
                not isinstance(digest, str) or
                re.fullmatch(r'[0-9a-f]{64}', digest) is None):
            raise ValueError('Invalid MCUboot selected file identity')
    encoded = json.dumps(files, sort_keys=True, separators=(',', ':')).encode()
    if hashlib.sha256(encoded).hexdigest() != REVIEWED_FILE_MAP_SHA256:
        raise ValueError('MCUboot reviewed source/notice closure differs')
    return lock


def verify(source, lock_path=LOCK, *, development=False, source_sdk=False):
    """Read stable selected bytes; a raw SDK payload check is not admission.

    Git observation delegates to the shared selected-checkout boundary. The
    enclosing SDK verifier may call this payload-only function without recursion;
    the CLI establishes whole-SDK ownership before publishing SDK admission.
    """
    source, lock_path = Path(source).absolute(), Path(lock_path).absolute()
    if development and source_sdk:
        raise ValueError('MCUboot development and SDK scopes are exclusive')
    raw, lock_identity = snapshot(lock_path)
    lock = contract(parse_json(raw))
    root = lock_path.parents[1]
    if (not source.is_dir() or
            any(part.is_symlink() for part in (source, *source.parents))):
        raise ValueError('MCUboot source requires explicit regular directories')
    if not development and (source != root / ITEM.path or
                             lock_path != root / ITEM.source_lock):
        raise ValueError('MCUboot requires canonical catalogue placement')

    def observe_git():
        if not source_sdk:
            dependencies.verify_selected_checkout(
                root, source, ITEM.identifier, COMMIT, TREE,
                parent_index=True, development=development)

    observe_git()
    identities = {}
    for name, expected in lock['files'].items():
        target = source / name
        identity = snapshot(target)[1]
        if identity['sha256'] != expected:
            raise ValueError('MCUboot selected source bytes differ: ' + name)
        identities[target] = identity
    if source_sdk:
        actual = set()
        for path in source.rglob('*'):
            if path.is_symlink():
                raise ValueError('MCUboot SDK payload cannot contain aliases')
            if path.is_file():
                actual.add(path.relative_to(source).as_posix())
        if actual != set(lock['files']):
            raise ValueError('MCUboot SDK payload differs from selected closure')
    for path, identity in identities.items():
        if snapshot(path)[1] != identity:
            raise ValueError('MCUboot source changed during observation')
    observe_git()
    if snapshot(lock_path)[1] != lock_identity:
        raise ValueError('MCUboot lock changed during observation')
    return {'name': ITEM.identifier, 'version': lock['version'],
            'commit': COMMIT, 'tree': TREE, 'license': lock['license'],
            'verified_files': len(identities),
            'scope': ('explicit_development_only' if development else
                      'source_sdk_payload' if source_sdk else
                      'organization_gitlink'),
            'production_admitted': not development and not source_sdk,
            'software_qualified': False, 'hardware_status': 'not_executed',
            'configure_inputs': [str(lock_path), *map(str, identities)]}


def verify_sdk(root):
    """Use the existing complete SDK verifier, without a second SDK runner."""
    sys.path.insert(0, str(ROOT / 'cmake/package'))
    import package_source_sdk
    return package_source_sdk.verify(root)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--lock', type=Path, default=LOCK)
    parser.add_argument('--development', action='store_true')
    parser.add_argument('--source-sdk', action='store_true')
    args = parser.parse_args(argv)
    try:
        _, lock_identity = snapshot(args.lock)
        manifest_path = args.lock.absolute().parents[1] / '.nexus-source-sdk.json'
        manifest_identity = None
        if args.source_sdk:
            if args.development:
                raise ValueError('MCUboot development and SDK scopes are exclusive')
            _, manifest_identity = snapshot(manifest_path)
            manifest = verify_sdk(manifest_path.parent)
            records = manifest.get('dependencies')
            selected = ([row for row in records if isinstance(row, dict) and
                         row.get('path') == ITEM.path]
                        if isinstance(records, list) else [])
            if (len(selected) != 1 or selected[0].get('commit') != COMMIT or
                    selected[0].get('tree') != TREE):
                raise ValueError('SDK lacks exact MCUboot commit/tree identity')
        result = verify(args.source, args.lock, development=args.development,
                        source_sdk=args.source_sdk)
        if snapshot(args.lock)[1] != lock_identity or (
                manifest_identity is not None and
                snapshot(manifest_path)[1] != manifest_identity):
            raise ValueError('MCUboot lock or SDK changed during proof')
        if args.source_sdk:
            result['production_admitted'] = True
    except (OSError, ValueError, TypeError, KeyError, ImportError) as error:
        print('MCUboot source rejected: ' + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
