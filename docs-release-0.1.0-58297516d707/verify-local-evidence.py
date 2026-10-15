#!/usr/bin/env python3
"""Verify the complete retained local evidence transport using only stdlib."""
from pathlib import Path
import hashlib
import json
import tarfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    root = Path(__file__).resolve().parent
    retention = json.loads((root / 'local-retention.json').read_text())
    for row in retention['files']:
        path = root / row['path']
        if path.is_symlink() or not path.is_file():
            raise ValueError('Missing or symbolic retained input')
        data = path.read_bytes()
        if len(data) != row['bytes'] or sha(data) != row['sha256']:
            raise ValueError('Retained input byte identity differs')
    index = json.loads((root / 'local-evidence-index.json').read_text())
    verification = json.loads((root / 'local-evidence-verification.json').read_text())
    if (index['source_commit'] != retention['source_commit'] or
            index['status'] != 'sealed_local_evidence' or
            verification['source_commit'] != retention['source_commit']):
        raise ValueError('Source identity differs')
    archive = root / verification['archive']['path']
    data = archive.read_bytes()
    if len(data) != verification['archive']['bytes'] or sha(data) != verification['archive']['sha256']:
        raise ValueError('Evidence archive identity differs')
    expected = {row['path']: row for row in index['members']}
    if len(expected) != len(index['members']):
        raise ValueError('Repeated inventory member')
    seen = set()
    with tarfile.open(archive, 'r:gz') as stream:
        for member in stream:
            if not member.isfile() or member.name in seen or member.name not in expected:
                raise ValueError('Unexpected evidence archive member')
            payload = stream.extractfile(member).read()
            row = expected[member.name]
            if len(payload) != row['bytes'] or sha(payload) != row['sha256']:
                raise ValueError('Evidence member byte identity differs')
            seen.add(member.name)
    if seen != set(expected):
        raise ValueError('Evidence archive omitted a member')
    print(json.dumps({'status': 'passed', 'source_commit': retention['source_commit'],
                      'standalone_files': len(retention['files']),
                      'evidence_members': len(seen),
                      'actual_pot_files': index['actual_pot_members'],
                      'main_ci_qualification': 'not claimed by local evidence retention',
                      'archive_sha256': sha(data)}))


if __name__ == '__main__':
    main()
