#!/usr/bin/env python3
"""Prepare, join, verify and restore an independently portable evidence archive.

This stdlib-only execution helper is outside the Nexus source repository.
Evidence records retain their original absolute paths and execution outcomes.
The archive provides relative member names and a separate origin locator map.
"""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tarfile
import tempfile

CHUNK = 1024 * 1024
MAX_FILES = 100000
MAX_BYTES = 2 * 1024 * 1024 * 1024
MAX_PART = 32 * 1024 * 1024
INVENTORY = 'EVIDENCE-INVENTORY.json'
LOCATORS = 'EVIDENCE-LOCATORS.json'
EXCLUSIONS = 'EVIDENCE-EXCLUSIONS.json'
VERIFIER = 'VERIFY-EVIDENCE.py'
EXCLUDED = 'evidence-export'
PATTERNS = {
    'github-token': re.compile(rb'(?<![A-Za-z0-9])(?:gh[pousr]_[A-Za-z0-9]{36,255}|github_pat_[A-Za-z0-9_]{60,255})'),
    'bearer-credential': re.compile(rb'(?<![A-Za-z0-9])Bearer[ \t]+[A-Za-z0-9_.+/=-]{24,256}', re.I),
    'private-key': re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA |ENCRYPTED )?PRIVATE KEY-----'),
    'signed-auth-url': re.compile(rb'[?&](?:X-Amz-Signature|X-Goog-Signature)=[0-9a-f]{16,255}', re.I),
    'aws-access-key': re.compile(rb'(?<![A-Z0-9])AKIA[A-Z0-9]{16}(?![A-Z0-9])'),
}


def canonical(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n').encode()


def parse_json(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('Duplicate metadata key')
            result[key] = value
        return result
    def constant(_):
        raise ValueError('Nonfinite metadata constant')
    return json.loads(raw, object_pairs_hook=pairs, parse_constant=constant)


def safe(path):
    path = Path(os.path.abspath(path))
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError('Evidence symlink path rejected')
    return path.resolve()


def relative(name):
    if (not isinstance(name, str) or not name or '\\' in name or
            any(ord(value) < 32 for value in name)):
        raise ValueError('Invalid evidence member path')
    path = PurePosixPath(name)
    if not path.parts or path.is_absolute() or '..' in path.parts or str(path) != name or ':' in name:
        raise ValueError('Noncanonical evidence member path')
    return name


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def file_identity(path, *, sensitive=False):
    path = safe(path)
    before = path.stat()
    if not stat.S_ISREG(before.st_mode):
        raise ValueError('Evidence requires regular input files')
    checksum, count, tail, findings = hashlib.sha256(), 0, b'', {}
    with path.open('rb') as stream:
        while block := stream.read(CHUNK):
            count += len(block)
            checksum.update(block)
            if sensitive:
                window = tail + block
                for category, pattern in PATTERNS.items():
                    if pattern.search(window):
                        findings[category] = True
                tail = window[-512:]
    after = path.stat()
    if ((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) !=
            (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) or count != before.st_size):
        raise ValueError('Evidence input changed while reading')
    return {'bytes': count, 'sha256': checksum.hexdigest()}, sorted(findings)


def exclusion_index(rows):
    result = {}
    for row in rows:
        if (not isinstance(row, dict) or set(row) != {'path', 'bytes', 'sha256', 'reason', 'immutable_url', 'representation', 'archive_member', 'artifact_sha256'} or
                type(row['bytes']) is not int or row['bytes'] < 0 or
                any(not isinstance(row[key], str) or not re.fullmatch(r'[0-9a-f]{64}', row[key]) for key in ('sha256', 'artifact_sha256')) or
                not isinstance(row['reason'], str) or not row['reason'] or
                not isinstance(row['immutable_url'], str) or not re.fullmatch(r'https://raw\.githubusercontent\.com/X-Gen-Lab/nexus/[0-9a-f]{40}/[A-Za-z0-9_./-]+', row['immutable_url'])):
            raise ValueError('Evidence exclusion must bind exact immutable asset fields')
        name = relative(row['path'])
        relative('/'.join(row['immutable_url'].split('/')[6:]))
        parts = PurePosixPath(name).parts
        if (len(parts) < 4 or parts[0] != 'git-assets-restore' or
                parts[2] not in {'downloaded', 'independent-restoration', 'cache', 'tool-cache', 'restored-cache'} or
                (parts[2] == 'downloaded' and not name.endswith(('.bin', '.tar.gz'))) or
                name in result):
            raise ValueError('Only duplicate artifact bodies may be excluded; retain receipts and raw metadata')
        protected = ('.json', '.log', '.txt', '.xml', '.csv', '.py', '.patch',
                     '.headers', '.body', '.stderr', '.stdout', '.yaml', '.yml', '.pdf')
        if (name.lower().endswith(protected) or
                any(value.lower() in {'browser', 'screenshots', 'errors', 'raw', 'logs'} for value in parts)):
            raise ValueError('Execution receipts, raw outputs and browser evidence cannot be excluded')
        if row['representation'] not in {'direct-file', 'joined-parts', 'archive-member'}:
            raise ValueError('Unknown immutable artifact representation')
        if row['representation'] == 'archive-member':
            relative(row['archive_member'])
        elif row['archive_member'] is not None or row['artifact_sha256'] != row['sha256']:
            raise ValueError('Evidence exclusion artifact identity inconsistent')
        result[name] = row
    if len(result) > MAX_FILES:
        raise ValueError('Evidence exclusions exceed bound')
    return result


def snapshot(source, *, sensitive=True, exclusions=()):
    source = safe(source)
    if not source.is_dir():
        raise ValueError('Evidence input directory missing')
    omission = exclusion_index(exclusions)
    records, excluded, findings, total = [], [], [], 0
    for parent, directories, files in os.walk(source, followlinks=False):
        parent = Path(parent)
        if parent == source:
            directories[:] = [name for name in directories if name != EXCLUDED]
        directories.sort()
        for name in directories:
            child = parent / name
            if child.is_symlink() or not child.is_dir():
                raise ValueError('Evidence directory symlink or special node rejected')
        for name in sorted(files):
            path = parent / name
            rel = relative(path.relative_to(source).as_posix())
            identity, categories = file_identity(path, sensitive=sensitive)
            if rel in omission:
                row = omission[rel]
                if identity != {key: row[key] for key in ('bytes', 'sha256')}:
                    raise ValueError('Evidence duplicate exclusion checksum or bytes mismatch')
                excluded.append(row)
            else:
                records.append({'path': 'payload/' + rel, **identity})
            if categories:
                findings.append({'relative_path': rel, 'categories': categories})
            total += identity['bytes'] if rel not in omission else 0
            if len(records) + len(excluded) > MAX_FILES or total > MAX_BYTES:
                raise ValueError('Evidence input exceeds archive bounds')
    if {row['path'] for row in excluded} != set(omission):
        raise ValueError('Evidence exclusion input missing')
    return {'files': sorted(records, key=lambda value: value['path']),
            'total_bytes': total, 'sensitive_findings': findings,
            'excluded': sorted(excluded, key=lambda row: row['path']),
            'excluded_total_bytes': sum(row['bytes'] for row in excluded)}


def member(tar, name, raw):
    info = tarfile.TarInfo(relative(name))
    info.size, info.mode, info.mtime = len(raw), 0o644, 0
    info.uid = info.gid = 0
    info.uname = info.gname = ''
    tar.addfile(info, io.BytesIO(raw))


def verify_archive(archive, expected_inventory=None):
    archive = safe(archive)
    actual, metadata, total = {}, {}, 0
    with tarfile.open(archive, 'r:gz') as tar:
        for item in tar:
            name = relative(item.name)
            if name in actual or not item.isfile() or item.size < 0:
                raise ValueError('Duplicate or nonregular evidence archive member')
            total += item.size
            if len(actual) >= MAX_FILES + 5 or total > MAX_BYTES + 64 * CHUNK:
                raise ValueError('Evidence archive exceeds bounds')
            if item.mode != 0o644 or item.uid != 0 or item.gid != 0 or item.mtime != 0:
                raise ValueError('Evidence archive metadata differs from deterministic policy')
            stream = tar.extractfile(item)
            checksum, size, retained = hashlib.sha256(), 0, []
            while block := stream.read(CHUNK):
                size += len(block)
                checksum.update(block)
                if name in {INVENTORY, LOCATORS, EXCLUSIONS}:
                    if size > 32 * CHUNK:
                        raise ValueError('Evidence metadata exceeds bound')
                    retained.append(block)
            if size != item.size:
                raise ValueError('Evidence member truncated')
            actual[name] = {'bytes': size, 'sha256': checksum.hexdigest()}
            if retained:
                metadata[name] = b''.join(retained)
    if any(name not in metadata for name in (INVENTORY, LOCATORS, EXCLUSIONS)):
        raise ValueError('Evidence inventory or locators missing')
    if any(not actual.get(name, {}).get('bytes') for name in (VERIFIER, 'README.txt')):
        raise ValueError('Portable evidence verifier or reader instructions missing')
    if any(str(parent) in actual for name in actual for parent in PurePosixPath(name).parents):
        raise ValueError('Evidence archive file and parent path collision')
    if expected_inventory is not None and digest(metadata[INVENTORY]) != expected_inventory:
        raise ValueError('Evidence inventory outer identity mismatch')
    inventory = parse_json(metadata[INVENTORY])
    if (not isinstance(inventory, dict) or set(inventory) != {'schema_version', 'kind', 'members', 'physical_status'} or
            type(inventory['schema_version']) is not int or inventory['schema_version'] != 1 or
            inventory['kind'] != 'nexus_documentation_evidence_inventory' or
            inventory['physical_status'] != 'not_executed' or not isinstance(inventory['members'], list)):
        raise ValueError('Evidence inventory schema invalid')
    expected = {}
    for row in inventory['members']:
        if (not isinstance(row, dict) or set(row) != {'path', 'bytes', 'sha256'} or
                type(row['bytes']) is not int or row['bytes'] < 0 or
                not isinstance(row['sha256'], str) or not re.fullmatch(r'[0-9a-f]{64}', row['sha256'])):
            raise ValueError('Evidence inventory member invalid')
        name = relative(row['path'])
        if name == INVENTORY or name in expected:
            raise ValueError('Evidence inventory self-reference or duplicate')
        expected[name] = {key: row[key] for key in ('bytes', 'sha256')}
    if {key: value for key, value in actual.items() if key != INVENTORY} != expected:
        raise ValueError('Evidence exact member closure or checksum mismatch')
    locators = parse_json(metadata[LOCATORS])
    if (not isinstance(locators, dict) or set(locators) != {'schema_version', 'kind', 'original_root', 'mappings'} or
            type(locators['schema_version']) is not int or locators['schema_version'] != 1 or
            locators['kind'] != 'nexus_evidence_relative_locators' or
            not isinstance(locators['original_root'], str) or not Path(locators['original_root']).is_absolute() or
            not isinstance(locators['mappings'], list)):
        raise ValueError('Evidence locator schema invalid')
    located = set()
    for row in locators['mappings']:
        if not isinstance(row, dict) or set(row) != {'archive_path', 'original_path'}:
            raise ValueError('Evidence locator fields invalid')
        name = relative(row['archive_path'])
        if (not name.startswith('payload/') or name in located or name not in actual or
                row['original_path'] != locators['original_root'] + '/' + name.removeprefix('payload/')):
            raise ValueError('Evidence relative locator mismatch')
        located.add(name)
    if located != {name for name in actual if name.startswith('payload/')}:
        raise ValueError('Evidence locator closure mismatch')
    omissions = parse_json(metadata[EXCLUSIONS])
    if (not isinstance(omissions, dict) or set(omissions) != {'schema_version', 'kind', 'records'} or
            type(omissions['schema_version']) is not int or omissions['schema_version'] != 1 or
            omissions['kind'] != 'nexus_evidence_immutable_duplicate_exclusions' or not isinstance(omissions['records'], list)):
        raise ValueError('Evidence exclusion schema invalid')
    omitted = exclusion_index(omissions['records'])
    if any('payload/' + name in actual for name in omitted):
        raise ValueError('Evidence exclusion overlaps payload')
    return {'status': 'passed', 'members': len(actual), 'payload_files': len(located),
            'payload_bytes': sum(actual[name]['bytes'] for name in located),
            'inventory_sha256': digest(metadata[INVENTORY]), 'physical_status': 'not_executed'}


def join_parts(manifest_path, output):
    manifest_path, output = safe(manifest_path), safe(output)
    if output.exists():
        raise ValueError('Evidence join output already exists')
    document = parse_json(manifest_path.read_bytes())
    if (not isinstance(document, dict) or set(document) != {'schema_version', 'kind', 'archive', 'parts', 'part_size_bytes', 'inventory_sha256', 'physical_status'} or
            type(document['schema_version']) is not int or document['schema_version'] != 1 or
            document['kind'] != 'nexus_evidence_archive_parts' or document['physical_status'] != 'not_executed' or
            type(document['part_size_bytes']) is not int or not 1 <= document['part_size_bytes'] <= MAX_PART or
            not isinstance(document['parts'], list) or not 1 <= len(document['parts']) <= 100000):
        raise ValueError('Evidence parts manifest schema invalid')
    archive = document['archive']
    if (not isinstance(archive, dict) or set(archive) != {'path', 'bytes', 'sha256'} or
            type(archive['bytes']) is not int or not 0 < archive['bytes'] <= MAX_BYTES + 64 * CHUNK or
            not isinstance(archive['sha256'], str) or not re.fullmatch(r'[0-9a-f]{64}', archive['sha256']) or
            not isinstance(document['inventory_sha256'], str) or not re.fullmatch(r'[0-9a-f]{64}', document['inventory_sha256'])):
        raise ValueError('Evidence archive identity invalid')
    filename = relative(archive['path'])
    if '/' in filename:
        raise ValueError('Evidence archive basename required')
    selected = []
    for index, row in enumerate(document['parts']):
        if (not isinstance(row, dict) or set(row) != {'path', 'bytes', 'sha256'} or
                row['path'] != filename + '.part' + f'{index:04d}' or
                type(row['bytes']) is not int or not 0 < row['bytes'] <= document['part_size_bytes'] or
                (index < len(document['parts']) - 1 and row['bytes'] != document['part_size_bytes'])):
            raise ValueError('Evidence part sequence or fields invalid')
        part = safe(manifest_path.parent / relative(row['path']))
        identity, _ = file_identity(part)
        if identity != {key: row[key] for key in ('bytes', 'sha256')} or output == part:
            raise ValueError('Evidence part bytes or checksum mismatch')
        selected.append(part)
    if output == manifest_path:
        raise ValueError('Evidence join cannot replace its input manifest')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.evidence-join-', dir=output.parent) as directory:
        temporary = Path(directory) / 'archive.tar.gz'
        with temporary.open('wb') as stream:
            for part in selected:
                with part.open('rb') as fragment:
                    shutil.copyfileobj(fragment, stream, CHUNK)
        identity, _ = file_identity(temporary)
        if identity != {key: archive[key] for key in ('bytes', 'sha256')}:
            raise ValueError('Joined evidence archive identity mismatch')
        result = verify_archive(temporary, document['inventory_sha256'])
        temporary.rename(output)
    return result


def restore(archive, destination):
    archive, destination = safe(archive), safe(destination)
    if destination.exists():
        raise ValueError('Evidence restore output already exists')
    observed = file_identity(archive)[0]
    result = verify_archive(archive)
    if file_identity(archive)[0] != observed:
        raise ValueError('Evidence archive changed after verification')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.evidence-restore-', dir=destination.parent) as directory:
        staged = Path(directory) / 'payload'
        staged.mkdir()
        with tarfile.open(archive, 'r:gz') as tar:
            seen = set()
            for item in tar:
                name = relative(item.name)
                if not item.isfile() or name in seen:
                    raise ValueError('Evidence restore archive member changed')
                seen.add(name)
                target = staged / name
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open('xb') as output:
                    shutil.copyfileobj(tar.extractfile(item), output, CHUNK)
        if file_identity(archive)[0] != observed:
            raise ValueError('Evidence archive changed during restoration')
        raw = (staged / INVENTORY).read_bytes()
        if digest(raw) != result['inventory_sha256']:
            raise ValueError('Restored evidence inventory differs from verified identity')
        expected = {row['path']: {key: row[key] for key in ('bytes', 'sha256')}
                    for row in parse_json(raw)['members']}
        expected[INVENTORY] = {'bytes': len(raw), 'sha256': digest(raw)}
        actual = {path.relative_to(staged).as_posix(): file_identity(path)[0]
                  for path in staged.rglob('*') if path.is_file()}
        if actual != expected:
            raise ValueError('Restored evidence file closure differs from verified archive')
        staged.rename(destination)
    return result


def assemble(source, output, label, *, part_bytes=MAX_PART, required=(), exclusions=()):
    source, output = safe(source), safe(output)
    if output == source or output in source.parents or source in output.parents:
        raise ValueError('Evidence output must not overlap inputs')
    if output.exists():
        raise ValueError('Evidence output already exists; preserve the previous attempt')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,127}', label):
        raise ValueError('Invalid evidence label')
    if type(part_bytes) is not int or not 1 <= part_bytes <= MAX_PART:
        raise ValueError('Evidence parts must be at most 32 MiB')
    before = snapshot(source, exclusions=exclusions)
    if before['sensitive_findings']:
        raise ValueError('Sensitive material detected; inspect redacted preliminary report')
    files = before['files']
    if not files or any('payload/' + relative(name) not in {row['path'] for row in files} for name in required):
        raise ValueError('Required evidence member missing')
    locator = {'schema_version': 1, 'kind': 'nexus_evidence_relative_locators',
               'original_root': str(source), 'mappings': [{'archive_path': row['path'],
                'original_path': str(source / row['path'].removeprefix('payload/'))} for row in files]}
    generated = {LOCATORS: canonical(locator), EXCLUSIONS: canonical({
        'schema_version': 1, 'kind': 'nexus_evidence_immutable_duplicate_exclusions',
        'records': before['excluded']}), VERIFIER: Path(__file__).read_bytes(),
        'README.txt': b'Nexus documentation evidence archive\n\nOriginal logs, receipts and failed attempts are preserved unchanged in payload/.\nEVIDENCE-LOCATORS.json maps portable archive members to original factual paths.\nEVIDENCE-INVENTORY.json hashes every other member and excludes itself.\nThe external parts manifest records the archive SHA-256 and sequential <=32 MiB parts.\nUse VERIFY-EVIDENCE.py join --manifest evidence-parts.json --output joined.tar.gz,\nthen verify --archive joined.tar.gz or restore --archive joined.tar.gz --output restored.\nA trusted immutable Git commit anchors the parts manifest; digests are not signatures.\nDocumentation evidence grants no additional firmware, physical HIL or release qualification.\n'}
    inventory = {'schema_version': 1, 'kind': 'nexus_documentation_evidence_inventory',
        'physical_status': 'not_executed', 'members': sorted(files + [
            {'path': name, 'bytes': len(raw), 'sha256': digest(raw)}
            for name, raw in generated.items()], key=lambda row: row['path'])}
    inventory_raw = canonical(inventory)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.evidence-export-', dir=output.parent) as directory:
        staged = Path(directory) / 'bundle'
        staged.mkdir()
        filename = 'nexus-docs-evidence-' + label + '.tar.gz'
        archive = staged / filename
        with archive.open('wb') as raw_archive:
            with gzip.GzipFile(filename='', mode='wb', fileobj=raw_archive, mtime=0, compresslevel=6) as compressed:
                with tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as tar:
                    member(tar, INVENTORY, inventory_raw)
                    for name, raw in sorted(generated.items()):
                        member(tar, name, raw)
                    for row in files:
                        path = safe(source / row['path'].removeprefix('payload/'))
                        current, _ = file_identity(path)
                        if current != {key: row[key] for key in ('bytes', 'sha256')}:
                            raise ValueError('Evidence input changed before archive read')
                        info = tarfile.TarInfo(row['path'])
                        info.size, info.mode, info.mtime = row['bytes'], 0o644, 0
                        with path.open('rb') as stream:
                            tar.addfile(info, stream)
        verified = verify_archive(archive, digest(inventory_raw))
        archive_identity, _ = file_identity(archive)
        parts = []
        with archive.open('rb') as stream:
            while block := stream.read(part_bytes):
                name = filename + '.part' + f'{len(parts):04d}'
                (staged / name).write_bytes(block)
                parts.append({'path': name, 'bytes': len(block), 'sha256': digest(block)})
        parts_manifest = {'schema_version': 1, 'kind': 'nexus_evidence_archive_parts',
            'archive': {'path': filename, **archive_identity}, 'parts': parts,
            'part_size_bytes': part_bytes, 'inventory_sha256': digest(inventory_raw),
            'physical_status': 'not_executed'}
        (staged / 'evidence-parts.json').write_bytes(canonical(parts_manifest))
        for name, raw in {**generated, INVENTORY: inventory_raw}.items():
            (staged / name).write_bytes(raw)
        joined = staged / 'joined-verification.tar.gz'
        joined_result = join_parts(staged / 'evidence-parts.json', joined)
        if file_identity(joined)[0] != archive_identity:
            raise ValueError('Actual part reassembly differs from original archive')
        joined.unlink()
        after = snapshot(source, exclusions=exclusions)
        if before != after:
            raise ValueError('Evidence source changed during assembly; freeze and retry')
        receipt = {'schema_version': 1, 'kind': 'nexus_evidence_export', 'status': 'passed',
            'physical_status': 'not_executed', 'source_root': str(source),
            'payload_files': len(files), 'payload_bytes': before['total_bytes'],
            'excluded_files': len(before['excluded']), 'excluded_bytes': before['excluded_total_bytes'],
            'exclusions_sha256': digest(generated[EXCLUSIONS]),
            'source_snapshot_unchanged': True, 'excluded_subtree': EXCLUDED,
            'archive': filename, 'archive_bytes': archive_identity['bytes'],
            'archive_sha256': archive_identity['sha256'], 'part_count': len(parts),
            'maximum_part_bytes': max(row['bytes'] for row in parts),
            'inventory_sha256': digest(inventory_raw), 'locators_sha256': digest(generated[LOCATORS]),
            'actual_tar_verification': verified, 'actual_part_join_verification': joined_result,
            'upload_members': [row['path'] for row in parts] + ['evidence-parts.json', VERIFIER, INVENTORY, LOCATORS, EXCLUSIONS, 'README.txt'],
            'digest_boundaries': 'Inventory excludes itself. Archive is outside payload. Parts manifest and export receipt are outside archive; receipt excludes its own hash.'}
        (staged / 'export-receipt.json').write_bytes(canonical(receipt))
        staged.rename(output)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    scan = sub.add_parser('scan')
    scan.add_argument('--input', type=Path, required=True)
    scan.add_argument('--report', type=Path, required=True)
    build = sub.add_parser('assemble')
    build.add_argument('--input', type=Path, required=True)
    build.add_argument('--output', type=Path, required=True)
    build.add_argument('--label', required=True)
    build.add_argument('--require', action='append', default=[])
    build.add_argument('--exclusions', type=Path)
    for command in ('join', 'verify', 'restore'):
        item = sub.add_parser(command)
        item.add_argument('--manifest' if command == 'join' else '--archive', type=Path, required=True)
        if command != 'verify':
            item.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'scan':
            report = safe(args.report)
            source = safe(args.input)
            if source in report.parents and EXCLUDED not in report.relative_to(source).parts[:1]:
                raise ValueError('Scan report cannot enter its own input payload')
            result = snapshot(source)
            report.parent.mkdir(parents=True, exist_ok=True)
            report.write_bytes(canonical(result))
        elif args.command == 'assemble':
            exclusions = parse_json(safe(args.exclusions).read_bytes()) if args.exclusions else []
            result = assemble(args.input, args.output, args.label, required=args.require, exclusions=exclusions)
        elif args.command == 'join':
            result = join_parts(args.manifest, args.output)
        elif args.command == 'verify':
            result = verify_archive(args.archive)
        else:
            result = restore(args.archive, args.output)
        print(canonical(result).decode(), end='')
        return 0
    except (OSError, ValueError, KeyError, TypeError, tarfile.TarError) as error:
        # Do not print matched credential bytes or signed redirect URLs.
        print('Evidence operation failed: ' + str(error))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
