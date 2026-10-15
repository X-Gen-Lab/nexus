#!/usr/bin/env python3
"""Restore immutable remote qualification records and the failed SDK attempt."""
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

COMMIT = 'c90efc92259ddd7da28c65406d5a5e4268840b8e'
PREFIX = 'docs-release-0.1.0-58297516d707-qualification'
SOURCE = '58297516d707fdee117bd27b5df6af09d72edd2e'
REGISTRY_SHA256 = 'cfd96fb5639ced8a0d612a21d7217265fc7c1e92753e90aefdbce6000c384657'


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
    args.destination = args.destination.resolve()
    args.proof_output = args.proof_output.resolve()
    args.destination.mkdir(parents=True, exist_ok=False)
    args.proof_output.mkdir(parents=True, exist_ok=False)
    started = datetime.now(timezone.utc).isoformat()
    registry_data, registry_url = fetch('assets.json')
    if sha(registry_data) != REGISTRY_SHA256:
        raise ValueError('Immutable committed registry identity differs')
    registry = json.loads(registry_data)
    rows = [r for r in registry['members'] if r['path'].startswith(PREFIX + '/')]
    if len(rows) != 12:
        raise ValueError('Unexpected explicit qualification asset set')
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
        str(args.destination / 'verify-qualification.py')],
        cwd=args.destination, capture_output=True, text=True, timeout=60)
    if process.returncode:
        raise ValueError('Downloaded standalone stdlib verifier failed')
    allowlist = json.loads((args.destination / 'qualification-allowlist.json').read_text())
    qualification_records = inspect_archive(
        args.destination / 'qualification-evidence.tar.gz',
        {r['path']: r for r in allowlist['archive_members']})
    examples_records = {}
    for stem in ('examples-pr3-ci-evidence', 'examples-main-ci-evidence'):
        manifest = json.loads((args.destination / (stem + '-manifest.json')).read_text())
        examples_records[stem] = inspect_archive(
            args.destination / (stem + '.tar.gz'),
            {r['path']: r for r in manifest['members']})
        if len(examples_records[stem]) != manifest['member_count']:
            raise ValueError('Restored examples member count differs')
    observations = {'kind': 'independent_downloaded_qualification_members',
                    'input_asset_commit': COMMIT, 'source_commit': SOURCE,
                    'qualification_members': qualification_records,
                    'examples_members_by_archive': examples_records}
    write(args.proof_output / 'member-observations.json', observations)
    receipt = {'schema_version': 1, 'kind': 'independent_qualification_asset_restoration',
               'status': 'passed', 'started_at_utc': started,
               'finished_at_utc': datetime.now(timezone.utc).isoformat(),
               'input_asset_commit': COMMIT, 'input_asset_prefix': PREFIX,
               'source_commit': SOURCE, 'asset_registry_url': registry_url,
               'asset_registry_sha256': sha(registry_data), 'files': files,
               'reader_execution': {'returncode': process.returncode,
                                    'stdout': process.stdout, 'stderr': process.stderr},
               'qualification_members_verified': len(qualification_records),
               'examples_members_verified_by_archive':
                   {stem: len(records) for stem, records in examples_records.items()},
               'member_observations_sha256': sha((args.proof_output / 'member-observations.json').read_bytes()),
               'scope': 'Immutable-URL byte download and restoration of actual remote qualification records: main CI 29/29, Security 4/4, Pages publication and HTTP, examples PR/main 11/11. SDK attempt 38684283007 failed without attestation or Release; this restoration performs no new CI or publication and establishes no successful SDK public Release.',
               'code_scanning_alert_inventory': 'unavailable_403',
               'firmware_candidate_qualification': False, 'physical_hil': 'not_executed'}
    write(args.proof_output / 'restore.json', receipt)
    print(json.dumps({'status': 'passed', 'input_asset_commit': COMMIT,
                      'files': len(files), 'qualification_members': len(qualification_records),
                      'examples_members_by_archive':
                          {stem: len(records) for stem, records in examples_records.items()}}))


if __name__ == '__main__':
    main()
