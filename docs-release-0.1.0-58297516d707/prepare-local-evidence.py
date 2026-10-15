#!/usr/bin/env python3
"""Prepare a compact evidence transport without staging or changing Git refs."""

import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile
from urllib.parse import parse_qs, urlsplit


def sha(data):
    return hashlib.sha256(data).hexdigest()


def reject_transport_secrets(name, data):
    """Report only the path, never a temporary URL or credential value."""
    text = data.decode('utf-8', errors='replace')
    texts = [text]
    if name.endswith('.json'):
        try:
            value = json.loads(text)
        except ValueError:
            value = None
        def strings(value):
            if isinstance(value, str):
                texts.append(value)
            elif isinstance(value, dict):
                for item in value.values():
                    strings(item)
            elif isinstance(value, list):
                for item in value:
                    strings(item)
        strings(value)
    for text in texts:
        if re.search(r'\b(?:ghp_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|sk-proj-[A-Za-z0-9_-]{20,})', text):
            raise ValueError('Possible credential in selected evidence: ' + name)
        for url in re.findall(r'https?://[^\s<>"\x27]+', text):
            query = {key.lower() for key in parse_qs(urlsplit(url).query)}
            if query & {'sig', 'signature', 'x-amz-signature', 'access_token',
                        'oauth_token', 'token', 'x-goog-signature'}:
                raise ValueError('Signed or credential-bearing URL in selected evidence: ' + name)
    if name.endswith('.tar.gz'):
        total = 0
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
            for number, member in enumerate(archive, 1):
                if number > 100000:
                    raise ValueError('Nested evidence archive exceeds member bound')
                if member.isdir():
                    continue
                if not member.isfile():
                    raise ValueError('Nested evidence contains a nonregular member')
                total += member.size
                if total > 256 * 1024 * 1024:
                    raise ValueError('Nested evidence exceeds byte bound')
                reject_transport_secrets(name + ':' + member.name,
                                         archive.extractfile(member).read())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-commit')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--extra', type=Path, action='append', default=[])
    args = parser.parse_args()
    session = Path('/tmp/nexus-release-0.1.0-20261015')
    source = Path('/workspace/nexus-build-system-20261012')
    assets = session / 'assets'
    baseline = 'b195b8cbceacf65fc31109e2fc92cd3aa3c2ab21'
    if subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=assets,
                               text=True).strip() != baseline:
        raise ValueError('Asset preparation requires the exact original baseline')
    registry = json.loads((assets / 'assets.json').read_text())
    known = {(r['bytes'], r['sha256']): r['path'] for r in registry['members']}
    review = json.loads((session / 'recovery/independent-release-source-review.json').read_text())
    included = {}
    origins = {}
    excluded = []
    for group in ('version', 'workflow', 'docs', 'catalogs', 'recovery',
                  'dotted-docnames'):
        for path in sorted((session / group).rglob('*')):
            if path.is_symlink():
                raise ValueError('Evidence selection contains a symlink: ' + str(path))
            if not path.is_file() or '__pycache__' in path.parts:
                continue
            relative = path.relative_to(session).as_posix()
            data = path.read_bytes()
            record = {'path': relative, 'bytes': len(data), 'sha256': sha(data)}
            reason = None
            if group == 'catalogs' and any(p in {'api', 'doctrees'} for p in path.relative_to(session / group).parts):
                reason = 'Derived API XML and doctrees; complete byte inventory retained; actual POT files and extraction commands are included.'
            if group == 'recovery' and 'surviving-original-receipts' in path.parts:
                reason = 'Unchanged surviving receipts retained once in recovery/surviving-original-candidate-receipts.tar.gz.'
            if group == 'recovery' and path.name in {
                    'assets.json', 'README.txt', 'raw-evidence.tar.gz',
                    'raw-evidence-index.json', 'EVIDENCE-INVENTORY.json',
                    'EVIDENCE-EXCLUSIONS.json', 'EVIDENCE-LOCATORS.json',
                    'evidence-parts.json', 'export-receipt.json'}:
                reason = 'Already retained byte-identically on immutable original asset commit.'
                member = known.get((len(data), sha(data)))
                if member is None and path.name != 'assets.json':
                    raise ValueError('Cannot prove durable excluded member: ' + relative)
                record['durable_url'] = ('https://raw.githubusercontent.com/X-Gen-Lab/nexus/' +
                                         baseline + '/' + (member or 'assets.json'))
            if reason:
                record['reason'] = reason
                excluded.append(record)
            else:
                included[relative] = data
                origins[relative] = str(path)
    for name in ('derived-cache-cleanup.json', 'source-commit-message.txt'):
        path = session / name
        if path.is_file():
            included[name] = path.read_bytes()
            origins[name] = str(path)
    for name in ('prepare_local_evidence.py', 'preparation.json',
                 'hook-installation.json'):
        path = session / 'asset-preparation' / name
        if path.is_file():
            member = 'asset-preparation/' + name
            included[member] = path.read_bytes()
            origins[member] = str(path)
    hook = assets / '.git/hooks/pre-commit'
    if hook.is_file():
        included['asset-preparation/normal-assets-pre-commit'] = hook.read_bytes()
        origins['asset-preparation/normal-assets-pre-commit'] = str(hook)
    for extra in args.extra:
        extra = extra.resolve(strict=True)
        monitor = session / 'pr-monitor'
        allowed_sanitized = {monitor / 'final917d1ae' / name for name in (
            'codeql-final-checks.json',
            'codeql-annotated-c-baseline-comparison.json',
            'codeql-targeted-triage.json')}
        if extra.is_relative_to(monitor) and extra not in allowed_sanitized:
            raise ValueError('Transport metadata is outside the explicit sanitized allowlist')
        paths = sorted(extra.rglob('*')) if extra.is_dir() else [extra]
        for path in paths:
            if path.is_symlink():
                raise ValueError('Extra evidence contains a symlink')
            if not path.is_file():
                continue
            name = 'final/' + extra.name + ('/' + path.relative_to(extra).as_posix() if extra.is_dir() else '')
            if name in included:
                raise ValueError('Duplicate evidence member')
            included[name] = path.read_bytes()
            origins[name] = str(path)
    for row in review['reviewed_files']:
        path = row['path']
        if args.source_commit:
            data = subprocess.check_output(['git', 'show', args.source_commit + ':' + path], cwd=source)
        else:
            data = (source / path).read_bytes()
        if len(data) != row['bytes'] or sha(data) != row['sha256']:
            raise ValueError('Reviewed source bytes changed: ' + path)
        included['reviewed-source/' + path] = data
        origins['reviewed-source/' + path] = ('git:' + args.source_commit + ':' + path if args.source_commit else str(source / path))
    bilingual_review = json.loads((session / 'docs/readme-cn-bilingual-correction-review.json').read_text())
    if args.source_commit:
        data = subprocess.check_output(['git', 'show', args.source_commit + ':README_CN.md'], cwd=source)
        if sha(data) != bilingual_review['reviewed_sha256']:
            raise ValueError('Independently reviewed Chinese README differs from final source')
        included['reviewed-source/README_CN.md'] = data
        origins['reviewed-source/README_CN.md'] = 'git:' + args.source_commit + ':README_CN.md'
    for name, data in included.items():
        reject_transport_secrets(name, data)
    if args.output.exists():
        raise ValueError('Evidence output must be fresh; preserve previous preparations')
    args.output.mkdir(parents=True)
    records = [{'path': p, 'bytes': len(data), 'sha256': sha(data), 'origin_path': origins[p]}
               for p, data in sorted(included.items())]
    index = {'schema_version': 1, 'kind': 'nexus_release_local_evidence_transport',
             'prepared_at_utc': datetime.now(timezone.utc).isoformat(),
             'source_commit': args.source_commit, 'asset_baseline': baseline,
             'status': 'sealed_local_evidence' if args.source_commit else 'preparation_awaiting_final_source_and_hook_receipts',
             'main_ci_qualification': 'not established by this local evidence capsule; final main CI and publication receipts remain separate',
             'scope': 'Original local RED/GREEN, workflow, prose/catalog reviews and historical recovery records; receipt-specific original execution scope remains authoritative. No physical HIL or new firmware candidate qualification.',
             'members': records, 'member_count': len(records),
             'actual_pot_members': sum(r['path'].endswith('.pot') for r in records)}
    (args.output / 'local-evidence-index.json').write_text(json.dumps(index, indent=2) + '\n')
    (args.output / 'local-evidence-exclusions.json').write_text(json.dumps({'kind': 'derived_or_already_durable_exclusions', 'members': excluded}, indent=2) + '\n')
    archive = args.output / 'local-evidence.tar.gz'
    with archive.open('wb') as raw:
        with gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0, compresslevel=6) as compressed:
            with tarfile.open(fileobj=compressed, mode='w|', format=tarfile.PAX_FORMAT) as tar:
                for name, data in sorted(included.items()):
                    entry = tarfile.TarInfo(name)
                    entry.size, entry.mode, entry.mtime = len(data), 0o600, 0
                    tar.addfile(entry, io.BytesIO(data))
    expected = {r['path']: r for r in records}
    seen = set()
    with tarfile.open(archive) as tar:
        for entry in tar:
            if not entry.isfile() or entry.name in seen or entry.name not in expected:
                raise ValueError('Unexpected or duplicate packed member')
            data = tar.extractfile(entry).read()
            row = expected[entry.name]
            if len(data) != row['bytes'] or sha(data) != row['sha256']:
                raise ValueError('Evidence changed in transport')
            seen.add(entry.name)
    if seen != set(expected):
        raise ValueError('Evidence member omitted')
    receipt = {'status': 'passed', 'members_verified': len(seen),
               'actual_pot_members': index['actual_pot_members'],
               'archive': {'path': archive.name, 'bytes': archive.stat().st_size,
                           'sha256': sha(archive.read_bytes())},
               'source_commit': args.source_commit, 'git_stage_or_commit_executed': False}
    (args.output / 'local-evidence-verification.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt))


if __name__ == '__main__':
    main()
