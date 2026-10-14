#!/usr/bin/env python3
"""Verify retained bytes with Python stdlib, without extracting archives."""
from pathlib import Path, PurePosixPath
import hashlib
import io
import json
import tarfile

ROOT = Path(__file__).resolve().parent

def digest(data):
    return hashlib.sha256(data).hexdigest()

def assert_equal(actual, expected, description):
    if actual != expected:
        raise SystemExit(f'{description}: expected {expected!r}; got {actual!r}')

def verify_archive(path, expected, allow_directories=False):
    found = set()
    observations = []
    arguments = {'fileobj': path} if hasattr(path, 'read') else {'name': path}
    with tarfile.open(mode='r:gz', **arguments) as archive:
        for member in archive:
            if allow_directories and member.isdir():
                continue
            if not member.isfile() or member.name in found:
                raise SystemExit(f'Nonregular or repeated archive member: {member.name}')
            name = PurePosixPath(member.name)
            if name.is_absolute() or '..' in name.parts:
                raise SystemExit(f'Unsafe archive member: {member.name}')
            found.add(member.name)
            data = archive.extractfile(member).read()
            record = expected.get(member.name)
            if record is None:
                raise SystemExit(f'Unexpected archive member: {member.name}')
            assert_equal(len(data), record['bytes'], member.name + ' byte count')
            assert_equal(digest(data), record['sha256'], member.name + ' SHA-256')
            observations.append({'path': member.name, 'bytes': len(data), 'sha256': digest(data)})
    assert_equal(found, set(expected), str(path) + ' member set')
    return observations

def main():
    assets = json.loads((ROOT / 'retained-assets.json').read_text())
    for record in assets['files']:
        data = (ROOT / record['path']).read_bytes()
        assert_equal(len(data), record['bytes'], record['path'] + ' byte count')
        assert_equal(digest(data), record['sha256'], record['path'] + ' SHA-256')
    outer_checksums = {}
    for line in (ROOT / 'RETAINED-SHA256SUMS').read_text().splitlines():
        value, name = line.split('  ', 1)
        if name in outer_checksums:
            raise SystemExit('Repeated retained checksum entry: ' + name)
        outer_checksums[name] = value
        assert_equal(digest((ROOT / name).read_bytes()), value, name + ' retained checksum')
    assert_equal(set(outer_checksums), {record['path'] for record in assets['files']} | {'retained-assets.json'},
                 'retained checksum member set')
    evidence = json.loads((ROOT / 'raw-evidence-index.json').read_text())
    evidence_observations = verify_archive(ROOT / evidence['archive']['path'],
                                           {r['path']: r for r in evidence['members']})
    nested_index = json.loads((ROOT / 'nested-evidence-index.json').read_text())
    nested_observations = []
    with tarfile.open(ROOT / evidence['archive']['path'], 'r:gz') as archive:
        for record in nested_index['archives']:
            data = archive.extractfile(record['outer_member_path']).read()
            assert_equal(digest(data), record['archive_sha256'], 'nested evidence archive')
            members = verify_archive(io.BytesIO(data), {r['path']: r for r in record['members']}, True)
            nested_observations.append({'path': record['outer_member_path'], 'members': len(members)})
    offline_record = assets['offline_package']
    manifest_bytes = (ROOT / 'documentation-manifest.json').read_bytes()
    assert_equal(digest(manifest_bytes), offline_record['manifest_sha256'], 'offline manifest')
    manifest = json.loads(manifest_bytes)
    payload = {r['path']: r for r in manifest['files']}
    checksums_bytes = (ROOT / 'SHA256SUMS').read_bytes()
    payload['documentation-manifest.json'] = {'bytes': len(manifest_bytes), 'sha256': digest(manifest_bytes)}
    payload['SHA256SUMS'] = {'bytes': len(checksums_bytes), 'sha256': digest(checksums_bytes)}
    for line in checksums_bytes.decode().splitlines():
        value, name = line.split('  ', 1)
        assert_equal(payload[name]['sha256'], value, name + ' SHA256SUMS')
    assert_equal(manifest['documentation_source']['commit'], assets['documentation_source_commit'], 'documentation source')
    offline_observations = verify_archive(ROOT / offline_record['path'], payload)
    print(json.dumps({'status': 'passed', 'retained_files': len(assets['files']),
                      'evidence_members': len(evidence_observations),
                      'nested_evidence_archives': nested_observations,
                      'offline_payload_files': len(manifest['files']),
                      'offline_total_files': len(offline_observations),
                      'offline_archive_sha256': offline_record['archive_sha256'],
                      'offline_manifest_sha256': offline_record['manifest_sha256']}))

if __name__ == '__main__':
    main()
