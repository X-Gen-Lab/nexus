#!/usr/bin/env python3
"""Independently fetch and verify immutable Git-retained documentation bytes."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from urllib.parse import quote
import datetime
import hashlib
import io
import json
import subprocess
import sys
import tarfile
import time
import urllib.request

ASSETS = Path('/tmp/nexus-documentation-assets-20261014')
RELATIVE = Path('docs-polish-20261014-51e93508')
LOCAL = ASSETS / RELATIVE

def sha(data):
    return hashlib.sha256(data).hexdigest()

def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()

def json_write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')

def observe_members(fileobj, expected, allow_directories=False):
    observed = []
    seen = set()
    with tarfile.open(fileobj=fileobj, mode='r:gz') as archive:
        for member in archive:
            if allow_directories and member.isdir():
                continue
            if not member.isfile() or member.name in seen:
                raise RuntimeError(f'Nonregular/repeated member: {member.name}')
            seen.add(member.name)
            data = archive.extractfile(member).read()
            record = {'path': member.name, 'bytes': len(data), 'sha256': sha(data)}
            requirement = expected[member.name]
            if any(record[key] != requirement[key] for key in ['bytes', 'sha256']):
                raise RuntimeError(f'Member differs: {member.name}')
            observed.append(record)
    if seen != set(expected):
        raise RuntimeError('Archive member set differs')
    return observed

def main():
    commit = sys.argv[1]
    if len(commit) != 40:
        raise RuntimeError('A full immutable Git commit is required')
    started = now()
    destination = Path('/tmp/nexus-docs-polish-remote-restore-' + commit[:12])
    destination.mkdir(exist_ok=False)
    inputs = [(path.name, path.read_bytes(), str(RELATIVE / path.name)) for path in sorted(LOCAL.iterdir()) if path.is_file()]
    inputs.append(('repository-assets-inventory.json', (ASSETS / 'assets.json').read_bytes(), 'assets.json'))
    observations = []

    def fetch(item):
        name, intended, remote_path = item
        url = 'https://raw.githubusercontent.com/X-Gen-Lab/nexus/' + commit + '/' + quote(remote_path)
        attempts = []
        for number in range(1, 4):
            try:
                with urllib.request.urlopen(url, timeout=30) as response:
                    data = response.read(); status = response.status
                attempts.append({'attempt': number, 'status': status, 'bytes': len(data)})
                if status != 200 or data != intended:
                    raise RuntimeError('Remote bytes differ from committed intended file')
                (destination / name).write_bytes(data)
                return {'path': remote_path, 'restore_path': name, 'url': url, 'http_status': status, 'bytes': len(data),
                        'expected_sha256': sha(intended), 'observed_sha256': sha(data), 'attempts': attempts}
            except Exception as error:
                attempts.append({'attempt': number, 'error': str(error)})
                if number == 3:
                    raise
                time.sleep(1)

    with ThreadPoolExecutor(max_workers=4) as pool:
        observations = list(pool.map(fetch, inputs))
    # Run only the script whose downloaded bytes have already matched the exact
    # intended committed input, and retain its actual process return code/output.
    verification = subprocess.run([sys.executable, str(destination / 'verify-retained-assets.py')],
                                  cwd=destination, capture_output=True, text=True)
    if verification.returncode:
        raise RuntimeError(verification.stderr + verification.stdout)
    verification_result = json.loads(verification.stdout)
    index = json.loads((destination / 'raw-evidence-index.json').read_text())
    evidence_data = (destination / index['archive']['path']).read_bytes()
    evidence_members = observe_members(io.BytesIO(evidence_data), {r['path']: r for r in index['members']})
    nested = []
    nested_index = json.loads((destination / 'nested-evidence-index.json').read_text())
    with tarfile.open(fileobj=io.BytesIO(evidence_data), mode='r:gz') as archive:
        for record in nested_index['archives']:
            data = archive.extractfile(record['outer_member_path']).read()
            nested.append({'outer_member_path': record['outer_member_path'], 'sha256': sha(data),
                           'members': observe_members(io.BytesIO(data), {r['path']: r for r in record['members']}, True)})
    assets_index = json.loads((destination / 'retained-assets.json').read_text())
    offline = assets_index['offline_package']
    manifest_data = (destination / 'documentation-manifest.json').read_bytes()
    manifest = json.loads(manifest_data)
    checksums_data = (destination / 'SHA256SUMS').read_bytes()
    expected = {r['path']: r for r in manifest['files']}
    expected['documentation-manifest.json'] = {'bytes': len(manifest_data), 'sha256': sha(manifest_data)}
    expected['SHA256SUMS'] = {'bytes': len(checksums_data), 'sha256': sha(checksums_data)}
    offline_data = (destination / offline['path']).read_bytes()
    offline_members = observe_members(io.BytesIO(offline_data), expected)
    member_observations = {
        'kind': 'independent_remote_retained_member_observations', 'input_assets_commit': commit,
        'raw_evidence': evidence_members, 'nested_raw_evidence': nested, 'offline_package': offline_members,
    }
    observation_file = LOCAL / 'remote-member-observations.json'
    json_write(observation_file, member_observations)
    receipt = {
        'schema_version': 1, 'kind': 'independent_fixed_commit_git_asset_restoration', 'status': 'passed',
        'executor': '/root/final_evidence_retention', 'started_at_utc': started, 'finished_at_utc': now(),
        'input_assets_commit': commit, 'input_asset_prefix': str(RELATIVE), 'restore_destination': str(destination),
        'transport': 'HTTPS raw.githubusercontent.com; exact full commit URL per file; no release API upload',
        'files': observations, 'standalone_file_count': len(observations),
        'reader_verification': {'command': [sys.executable, str(destination / 'verify-retained-assets.py')],
                                'cwd': str(destination), 'returncode': verification.returncode,
                                'stdout': verification.stdout, 'stderr': verification.stderr,
                                'result': verification_result},
        'raw_evidence_archive_sha256': sha(evidence_data), 'raw_evidence_members_verified': len(evidence_members),
        'nested_evidence_archives_verified': len(nested),
        'nested_evidence_members_verified': sum(len(record['members']) for record in nested),
        'offline_archive_sha256': sha(offline_data), 'offline_manifest_sha256': sha(manifest_data),
        'offline_payload_files': len(manifest['files']), 'offline_total_files_verified': len(offline_members),
        'member_observations': {'path': observation_file.name, 'bytes': observation_file.stat().st_size,
                                'sha256': sha(observation_file.read_bytes())},
        'documentation_source_commit': manifest['documentation_source']['commit'],
        'qualified_firmware_baseline': manifest['qualified_code']['commit'],
        'new_firmware_qualification': False, 'physical_hil': 'not executed',
        'scope': 'Independent remote byte restoration and reader verification only. Existing executed build/test/browser/CI/HTTPS publication claims retain their receipt-specific source/scope.',
        'self_hashing': 'This followup receipt and observations are separate from the original sealed capsule; they do not recursively index themselves.',
    }
    receipt_path = LOCAL / 'remote-restore.json'
    json_write(receipt_path, receipt)
    sums = ''.join(sha(path.read_bytes()) + '  ' + path.name + '\n' for path in [receipt_path, observation_file])
    (LOCAL / 'REMOTE-RESTORE-SHA256SUMS').write_text(sums)
    print(json.dumps({'status': 'passed', 'commit': commit, 'restore_destination': str(destination),
                      'remote_files': len(observations), 'raw_members': len(evidence_members),
                      'nested_members': receipt['nested_evidence_members_verified'],
                      'offline_members': len(offline_members), 'receipt_sha256': sha(receipt_path.read_bytes())}))

if __name__ == '__main__':
    main()
