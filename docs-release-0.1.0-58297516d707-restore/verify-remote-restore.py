#!/usr/bin/env python3
"""Independently restore immutable release evidence and verify every member."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tarfile
import urllib.request

COMMIT = 'a85bafd6c9f816a5f92dd70a426318436f237d2c'
SOURCE = '58297516d707fdee117bd27b5df6af09d72edd2e'
PREFIX = 'docs-release-0.1.0-58297516d707'
REGISTRY_SHA256 = 'd456cd7b0a9716ca76fd45e49b9d9b33ba74bdf3213998cc269bdac3ed8f971f'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def fetch(path):
    url = 'https://raw.githubusercontent.com/X-Gen-Lab/nexus/' + COMMIT + '/' + path
    with urllib.request.urlopen(url, timeout=30) as response:
        data, status = response.read(), response.status
    if status != 200:
        raise ValueError('Immutable asset download failed')
    return data, url, status


def safe_member(name):
    path = PurePosixPath(name)
    return not path.is_absolute() and '..' not in path.parts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--proof-output', type=Path, required=True)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=False)
    args.proof_output.mkdir(parents=True, exist_ok=False)
    started = datetime.now(timezone.utc).isoformat()
    registry_data, registry_url, status = fetch('assets.json')
    if sha(registry_data) != REGISTRY_SHA256:
        raise ValueError('Immutable asset registry differs from the committed input')
    registry = json.loads(registry_data)
    selections = [row for row in registry['members']
                  if row['path'].startswith(PREFIX + '/')]
    if len(selections) != 9:
        raise ValueError('Unexpected retained local evidence asset set')
    def restore(row):
        data, url, status = fetch(row['path'])
        if len(data) != row['bytes'] or sha(data) != row['sha256']:
            raise ValueError('Downloaded retained bytes differ')
        name = PurePosixPath(row['path']).relative_to(PREFIX)
        if not safe_member(str(name)) or len(name.parts) != 1:
            raise ValueError('Retained path escapes prefix')
        (args.destination / str(name)).write_bytes(data)
        return {'path': row['path'], 'url': url, 'http_status': status,
                'bytes': len(data), 'expected_sha256': row['sha256'],
                'observed_sha256': sha(data)}
    with ThreadPoolExecutor(max_workers=3) as pool:
        files = list(pool.map(restore, selections))
    verification = subprocess.run([sys.executable, '-B',
        str(args.destination / 'verify-local-evidence.py')],
        cwd=args.destination, capture_output=True, text=True, timeout=60)
    if verification.returncode:
        raise ValueError('Independently downloaded stdlib reader failed')
    index = json.loads((args.destination / 'local-evidence-index.json').read_text())
    expected = {row['path']: row for row in index['members']}
    seen, members, nested = set(), [], []
    with tarfile.open(args.destination / 'local-evidence.tar.gz', 'r:gz') as archive:
        for member in archive:
            if (not member.isfile() or not safe_member(member.name) or
                    member.name in seen or member.name not in expected):
                raise ValueError('Unsafe, duplicate or undeclared evidence member')
            data = archive.extractfile(member).read()
            requirement = expected[member.name]
            if len(data) != requirement['bytes'] or sha(data) != requirement['sha256']:
                raise ValueError('Packed evidence bytes differ')
            members.append({'path': member.name, 'bytes': len(data), 'sha256': sha(data)})
            seen.add(member.name)
            if member.name.endswith('hook-tdd.tar.gz'):
                nested_seen, nested_files, nested_bytes = set(), 0, 0
                with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as inner:
                    for entry in inner:
                        if not safe_member(entry.name) or entry.name in nested_seen:
                            raise ValueError('Unsafe or repeated nested hook member')
                        nested_seen.add(entry.name)
                        if entry.isdir():
                            continue
                        if not entry.isfile():
                            raise ValueError('Nonregular nested hook member')
                        payload = inner.extractfile(entry).read()
                        if len(payload) != entry.size:
                            raise ValueError('Nested hook member is truncated')
                        hashlib.sha256(payload).hexdigest()
                        nested_files += 1
                        nested_bytes += len(payload)
                nested.append({'outer_member': member.name, 'sha256': sha(data),
                               'members_read': len(nested_seen),
                               'regular_files_read': nested_files,
                               'uncompressed_bytes_read': nested_bytes})
    if seen != set(expected) or len(nested) != 3:
        raise ValueError('Evidence closure or three normal hook archives missing')
    observations = {'kind': 'independent_downloaded_evidence_member_observations',
                    'input_asset_commit': COMMIT, 'source_commit': SOURCE,
                    'members': members, 'normal_hook_archives': nested}
    write(args.proof_output / 'member-observations.json', observations)
    receipt = {'schema_version': 1, 'kind': 'independent_release_local_evidence_restoration',
               'status': 'passed', 'started_at_utc': started,
               'finished_at_utc': datetime.now(timezone.utc).isoformat(),
               'input_asset_commit': COMMIT, 'input_asset_prefix': PREFIX,
               'source_commit': SOURCE, 'asset_registry_url': registry_url,
               'asset_registry_sha256': sha(registry_data), 'files': files,
               'reader_execution': {'argv': [sys.executable, '-B', 'verify-local-evidence.py'],
                                    'returncode': verification.returncode,
                                    'stdout': verification.stdout, 'stderr': verification.stderr},
               'evidence_members_verified': len(members),
               'actual_pot_members_retained': index['actual_pot_members'],
               'normal_hook_archives_completely_read': nested,
               'member_observations_sha256': sha((args.proof_output / 'member-observations.json').read_bytes()),
               'main_ci_qualification': 'not claimed by local evidence restoration',
               'release_publication_qualification': False,
               'physical_hil': 'not_executed',
               'scope': 'Independent immutable-URL byte download and archive verification only; original receipt-specific execution and source scopes remain unchanged.'}
    write(args.proof_output / 'restore.json', receipt)
    print(json.dumps({'status': 'passed', 'input_asset_commit': COMMIT,
                      'source_commit': SOURCE, 'files': len(files),
                      'evidence_members': len(members), 'normal_hook_archives': len(nested)}))


if __name__ == '__main__':
    main()
