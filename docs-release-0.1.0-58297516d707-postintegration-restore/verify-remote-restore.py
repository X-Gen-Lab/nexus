#!/usr/bin/env python3
"""Restore the immutable local-validation addendum without credentials."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tarfile
import urllib.request

COMMIT = 'b6dd7cbe53658fe855f335081084680ce909e582'
PREFIX = 'docs-release-0.1.0-58297516d707-postintegration'
SOURCE = '58297516d707fdee117bd27b5df6af09d72edd2e'
REGISTRY_SHA256 = 'f8624627f92f9ff379bcd4a2150a3512774a860d6f69a01712dc448abf5fb23b'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def fetch(path):
    url = 'https://raw.githubusercontent.com/X-Gen-Lab/nexus/' + COMMIT + '/' + path
    with urllib.request.urlopen(url, timeout=30) as response:
        data, status = response.read(), response.status
    if status != 200:
        raise ValueError('Immutable download failed')
    return data, url


def inspect_archive(path, expected):
    observations, seen = [], set()
    with tarfile.open(path, 'r:gz') as archive:
        for member in archive:
            relative = PurePosixPath(member.name)
            if (not member.isfile() or relative.is_absolute() or
                    '..' in relative.parts or member.name not in expected or
                    member.name in seen):
                raise ValueError('Unsafe or undeclared archive member')
            data = archive.extractfile(member).read()
            row = expected[member.name]
            if len(data) != row['bytes'] or sha(data) != row['sha256']:
                raise ValueError('Downloaded archive member differs')
            seen.add(member.name)
            observations.append({'path': member.name, 'bytes': len(data),
                                 'sha256': sha(data)})
    if seen != set(expected):
        raise ValueError('Downloaded archive member omitted')
    return observations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--proof-output', type=Path, required=True)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=False)
    args.proof_output.mkdir(parents=True, exist_ok=False)
    started = datetime.now(timezone.utc).isoformat()
    registry_data, registry_url = fetch('assets.json')
    if sha(registry_data) != REGISTRY_SHA256:
        raise ValueError('Immutable committed registry identity differs')
    registry = json.loads(registry_data)
    rows = [r for r in registry['members'] if r['path'].startswith(PREFIX + '/')]
    if len(rows) != 10:
        raise ValueError('Unexpected explicit postintegration asset set')
    def restore(row):
        data, url = fetch(row['path'])
        if len(data) != row['bytes'] or sha(data) != row['sha256']:
            raise ValueError('Downloaded standalone bytes differ')
        name = PurePosixPath(row['path']).relative_to(PREFIX)
        if len(name.parts) != 1 or '..' in name.parts:
            raise ValueError('Standalone path escapes fixed prefix')
        (args.destination / str(name)).write_bytes(data)
        return {'path': row['path'], 'url': url, 'http_status': 200,
                'bytes': len(data), 'sha256': sha(data)}
    with ThreadPoolExecutor(max_workers=3) as pool:
        files = list(pool.map(restore, rows))
    process = subprocess.run([sys.executable, '-B',
        str(args.destination / 'verify-postintegration.py')],
        cwd=args.destination, capture_output=True, text=True, timeout=60)
    if process.returncode:
        raise ValueError('Downloaded standalone stdlib verifier failed')
    allowlist = json.loads((args.destination / 'postintegration-allowlist.json').read_text())
    examples = json.loads((args.destination / 'examples-local-validation-manifest.json').read_text())
    post_records = inspect_archive(args.destination / 'postintegration-evidence.tar.gz',
                                  {r['path']: r for r in allowlist['archive_members']})
    example_records = inspect_archive(args.destination / 'examples-local-validation.tar.gz',
                                     {r['member']: r for r in examples['files']})
    observations = {'kind': 'independent_downloaded_local_addendum_members',
                    'input_asset_commit': COMMIT, 'source_commit': SOURCE,
                    'postintegration_members': post_records,
                    'examples_members': example_records}
    write(args.proof_output / 'member-observations.json', observations)
    receipt = {'schema_version': 1, 'kind': 'independent_postintegration_asset_restoration',
               'status': 'passed', 'started_at_utc': started,
               'finished_at_utc': datetime.now(timezone.utc).isoformat(),
               'input_asset_commit': COMMIT, 'input_asset_prefix': PREFIX,
               'source_commit': SOURCE, 'asset_registry_url': registry_url,
               'asset_registry_sha256': sha(registry_data), 'files': files,
               'reader_execution': {'returncode': process.returncode,
                                    'stdout': process.stdout, 'stderr': process.stderr},
               'postintegration_members_verified': len(post_records),
               'examples_members_verified': len(example_records),
               'member_observations_sha256': sha((args.proof_output / 'member-observations.json').read_bytes()),
               'scope': 'Immutable-URL byte download and local evidence restoration only; full main CI, GitHub Release and remote Pages success are not claimed.',
               'firmware_candidate_qualification': False, 'physical_hil': 'not_executed'}
    write(args.proof_output / 'restore.json', receipt)
    print(json.dumps({'status': 'passed', 'input_asset_commit': COMMIT,
                      'files': len(files), 'postintegration_members': len(post_records),
                      'examples_members': len(example_records)}))


if __name__ == '__main__':
    main()
