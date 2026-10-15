#!/usr/bin/env python3
"""Verify the explicitly retained local postintegration evidence bytes."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import tarfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify_archive(path, expected):
    seen = set()
    with tarfile.open(path, 'r:gz') as archive:
        for member in archive:
            relative = PurePosixPath(member.name)
            if (not member.isfile() or relative.is_absolute() or
                    '..' in relative.parts or member.name in seen or
                    member.name not in expected):
                raise ValueError('Unexpected, unsafe or duplicate packed member')
            data = archive.extractfile(member).read()
            row = expected[member.name]
            if len(data) != row['bytes'] or sha(data) != row['sha256']:
                raise ValueError('Packed member byte identity differs')
            seen.add(member.name)
    if seen != set(expected):
        raise ValueError('Packed member omitted')
    return len(seen)


def main():
    root = Path(__file__).resolve().parent
    retention = json.loads((root / 'postintegration-retention.json').read_text())
    for row in retention['files']:
        path = root / row['path']
        if path.is_symlink() or not path.is_file():
            raise ValueError('Retained standalone file missing or symbolic')
        data = path.read_bytes()
        if len(data) != row['bytes'] or sha(data) != row['sha256']:
            raise ValueError('Retained standalone identity differs')
    allowlist = json.loads((root / 'postintegration-allowlist.json').read_text())
    if allowlist['source_commit'] != retention['source_commit']:
        raise ValueError('Allowlist source differs')
    rows = allowlist['archive_members']
    expected = {row['path']: row for row in rows}
    if len(expected) != len(rows):
        raise ValueError('Repeated explicit allowlist member')
    post_count = verify_archive(root / 'postintegration-evidence.tar.gz', expected)
    manifest = json.loads((root / 'examples-local-validation-manifest.json').read_text())
    receipt = json.loads((root / 'examples-local-validation-receipt.json').read_text())
    if (manifest['sdk_commit'] != retention['source_commit'] or
            receipt['sdk_commit'] != retention['source_commit']):
        raise ValueError('Examples SDK source identity differs')
    expected_examples = {row['member']: row for row in manifest['files']}
    if len(expected_examples) != manifest['member_count']:
        raise ValueError('Examples manifest has duplicate or missing members')
    examples_count = verify_archive(root / 'examples-local-validation.tar.gz',
                                    expected_examples)
    print(json.dumps({'status': 'passed',
                      'source_commit': retention['source_commit'],
                      'postintegration_members': post_count,
                      'examples_members': examples_count,
                      'scope': 'Explicit local validation byte retention only',
                      'full_main_ci_release_pages_success': 'not claimed'}))


if __name__ == '__main__':
    main()
