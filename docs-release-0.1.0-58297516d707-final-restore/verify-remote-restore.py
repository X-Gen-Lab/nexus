#!/usr/bin/env python3
"""Restore the immutable final software-preview evidence without SDK execution."""
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

COMMIT = '0928f73809ca6bd3c9cae1f7d29f308db6bb201f'
PREFIX = 'docs-release-0.1.0-58297516d707-final'
SOURCE = '58297516d707fdee117bd27b5df6af09d72edd2e'
REGISTRY_SHA256 = '22fde6e72362e99c86b3a71ab24d0ac7a9440077c72e826d104a0ca7855f2380'


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
    if len(rows) != 9:
        raise ValueError('Unexpected explicit final asset set')
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
        str(args.destination / 'verify-final-delivery.py')],
        cwd=args.destination, capture_output=True, text=True, timeout=60)
    if process.returncode:
        raise ValueError('Downloaded standalone stdlib verifier failed')
    allowlist = json.loads((args.destination / 'final-allowlist.json').read_bytes())
    final_records = inspect_archive(args.destination / 'final-evidence.tar.gz',
        {r['path']: r for r in allowlist['archive_members']})
    with tarfile.open(args.destination / 'final-evidence.tar.gz', 'r:gz') as archive:
        prefix = 'independent-release-restore/'
        inventory_data = archive.extractfile(prefix + 'local-evidence-files.json').read()
        inventory = json.loads(inventory_data)
        restored_archive = archive.extractfile(prefix + 'independent-release-restore-evidence.tar.gz').read()
    restored_files, restored_dirs = [], []
    seen = set()
    with tarfile.open(fileobj=io.BytesIO(restored_archive), mode='r:gz') as archive:
        for member in archive:
            name = member.name.rstrip('/') if member.isdir() else member.name
            relative = PurePosixPath(name)
            if (relative.is_absolute() or '..' in relative.parts or str(relative) != name or
                    name in seen or not (member.isfile() or member.isdir())):
                raise ValueError('Unsafe or duplicate restored evidence member')
            seen.add(name)
            if member.isdir():
                if name not in inventory['directories']:
                    raise ValueError('Undeclared restored directory')
                restored_dirs.append(name)
            else:
                data = archive.extractfile(member).read()
                if name == 'local-evidence-files.json':
                    if data != inventory_data:
                        raise ValueError('Restored self-index differs')
                else:
                    row = inventory['files'][name]
                    if len(data) != row['size'] or sha(data) != row['sha256'] or member.mode != row['mode']:
                        raise ValueError('Restored raw file size/hash/mode differs')
                restored_files.append({'path': name, 'bytes': len(data),
                                       'sha256': sha(data), 'mode': member.mode})
    if ({r['path'] for r in restored_files} != set(inventory['files']) | {'local-evidence-files.json'} or
            set(restored_dirs) != set(inventory['directories']) or
            len(final_records) != 98 or len(restored_files) != 1063 or len(restored_dirs) != 887):
        raise ValueError('Downloaded final evidence closure differs')
    reader = json.loads(process.stdout)
    if (reader['raw_hook_members'] != [14293, 15733] or
            reader['source_commit'] != SOURCE or reader['status'] != 'passed'):
        raise ValueError('Downloaded reader observation differs')
    observations = {'kind': 'independent_downloaded_final_evidence_members',
        'input_asset_commit': COMMIT, 'source_commit': SOURCE,
        'final_evidence_members': final_records,
        'sealed_restoration_regular_files': restored_files,
        'sealed_restoration_directories': restored_dirs,
        'raw_hook_archive_members_read': reader['raw_hook_members']}
    write(args.proof_output / 'member-observations.json', observations)
    receipt = {'schema_version': 1, 'kind': 'independent_final_delivery_asset_restoration',
        'status': 'passed', 'started_at_utc': started,
        'finished_at_utc': datetime.now(timezone.utc).isoformat(),
        'input_asset_commit': COMMIT, 'input_asset_prefix': PREFIX,
        'source_commit': SOURCE,
        'builder_commit': '51bebd47dd63dce90c7fded1efafef27cd20fd3e',
        'asset_registry_url': registry_url, 'asset_registry_sha256': sha(registry_data),
        'files': files, 'reader_execution': {'returncode': process.returncode,
            'stdout': process.stdout, 'stderr': process.stderr},
        'final_evidence_members_verified': len(final_records),
        'sealed_restoration_regular_files_verified': len(restored_files),
        'sealed_restoration_directories_verified': len(restored_dirs),
        'normal_hook_raw_archive_members_read': reader['raw_hook_members'],
        'member_observations_sha256': sha((args.proof_output / 'member-observations.json').read_bytes()),
        'scope': 'Actual immutable-URL HTTP download and byte restoration of final software-preview evidence. Retained records establish actual R3 public publication, independent public SDK crypto/C-blob/fresh-consumer acceptance and completed cleanup. This restoration performs no SDK build/test, cryptographic verifier rerun, remote mutation or new release publication.',
        'public_release_body_final_evidence_link_patch': 'separate root metadata receipt follows this HTTP restoration',
        'physical_hil': 'not_executed', 'production_qualification': False}
    write(args.proof_output / 'restore.json', receipt)
    print(json.dumps({'status': 'passed', 'input_asset_commit': COMMIT,
        'files': len(files), 'final_evidence_members': len(final_records),
        'restoration_regular_files': len(restored_files),
        'restoration_directories': len(restored_dirs),
        'normal_hook_raw_archive_members_read': reader['raw_hook_members']}))

if __name__ == '__main__':
    main()
