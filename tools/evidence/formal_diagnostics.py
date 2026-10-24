#!/usr/bin/env python3
"""Read original formal evidence and split its unchanged archive for download."""

import argparse
import hashlib
from pathlib import Path, PurePosixPath
import shutil
import sys
import tarfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.evidence import store
from tools.evidence.common import (EvidenceError, atomic_json, digest,
    file_identity, load_json, regular_file, stream_digest)
from tools.evidence.identity import canonical_digest
from tools.evidence.qualification_transfer import (context, safe_directory,
    signature)

PART_BYTES = 450 * 1024 ** 2
MAX_PARTS = 16
DIAGNOSTIC_BYTES = 400 * 1024 ** 2
JOB_MEMBER = 'payload/job-identities/formal-software/identity.json'
FORMAL_MEMBER = 'payload/formal-transfer/formal-package.json'
NATIVE_PRESETS = ('native-debug', 'native-release', 'native-asan', 'native-tsan')


def _extract(archive, selected, output, limit=DIAGNOSTIC_BYTES):
    """Copy explicit regular members only; never execute archived content."""
    missing, total = set(selected), 0
    with tarfile.open(archive, 'r|gz') as stream:
        for member in stream:
            if member.name not in selected:
                continue
            total += member.size
            if (member.name not in missing or not member.isfile() or
                    total > limit):
                raise EvidenceError('Invalid or oversized diagnostic member')
            missing.remove(member.name)
            relative = store._safe_path(selected[member.name])
            destination = output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            with stream.extractfile(member) as source:
                with destination.open('xb') as target:
                    shutil.copyfileobj(source, target, 1024 * 1024)
            destination.chmod(member.mode & 0o777)
    if missing:
        raise EvidenceError('Required original archive members are missing')


def _artifact(directory):
    envelope = directory / 'formal-build-package.json'
    package = load_json(envelope)
    identity = package.get('archive')
    if not isinstance(identity, dict):
        raise EvidenceError('Original archive identity is missing')
    name = Path(identity.get('path', '')).name
    if not name or name != package['archive'].get('sha256', '') + '.tar.gz':
        raise EvidenceError('Original archive name differs from identity')
    archive = directory / 'formal-build-store' / name
    allowed = {envelope, archive, archive.with_suffix('').with_suffix('.json')}
    observed = set()
    for path in directory.rglob('*'):
        if path.is_symlink() or not (path.is_dir() or path.is_file()):
            raise EvidenceError('Unexpected artifact link or special entry')
        if path.is_file():
            observed.add(path)
        elif path != directory / 'formal-build-store':
            raise EvidenceError('Unexpected artifact directory')
    if observed != allowed:
        raise EvidenceError('Unknown or missing original artifact files')
    if load_json(archive.with_suffix('').with_suffix('.json')) != package:
        raise EvidenceError('Original envelope copies differ')
    if not 0 < archive.stat().st_size <= PART_BYTES * MAX_PARTS:
        raise EvidenceError('Original archive exceeds transport size bound')
    return package, archive, {str(path): signature(path) for path in observed}


def _identity(job, expected, nested_envelope):
    check = dict(job)
    checksum = check.pop('record_sha256', None)
    if (canonical_digest(check) != checksum or
            job.get('kind') != 'ci_job_identity' or
            job.get('schema_version') != 1 or
            job.get('job') != 'formal-software' or
            job.get('job_result') not in ('success', 'failure', 'cancelled') or
            job.get('qualification') != 'not_a_software_candidate' or
            job.get('source', {}).get('commit') != expected['commit'] or
            job.get('run', {}).get('GITHUB_RUN_ID') != expected['run_id'] or
            job.get('run', {}).get('GITHUB_RUN_ATTEMPT') != expected['attempt']):
        raise EvidenceError('Original formal job differs from requested context')
    entries = [item for item in job.get('artifacts', []) if
               item.get('relative_path') == 'build/formal-transfer/formal-package.json']
    if (len(entries) != 1 or
            entries[0].get('size') != nested_envelope.stat().st_size or
            entries[0].get('sha256') != digest(nested_envelope)):
        raise EvidenceError('Original job does not bind the nested envelope')


def _split(archive, output, expected):
    parts, whole, size = [], hashlib.sha256(), 0
    with archive.open('rb') as source:
        while size < expected['size']:
            if len(parts) >= MAX_PARTS:
                raise EvidenceError('Original archive exceeds part count')
            name = 'part-' + str(len(parts)).zfill(3)
            checksum, count = hashlib.sha256(), 0
            with (output / name).open('xb') as target:
                while count < PART_BYTES:
                    chunk = source.read(min(1024 * 1024, PART_BYTES - count))
                    if not chunk:
                        break
                    target.write(chunk)
                    checksum.update(chunk)
                    whole.update(chunk)
                    count += len(chunk)
            if not count:
                raise EvidenceError('Original archive ended during split')
            parts.append({'ordinal': len(parts), 'name': name, 'offset': size,
                          'size': count, 'sha256': checksum.hexdigest()})
            size += count
        if (source.read(1) or size != expected['size'] or
                whole.hexdigest() != expected['sha256']):
            raise EvidenceError('Original archive changed during split')
    return parts


def _option(stage, name):
    argv = stage.get('argv', [])
    if not isinstance(argv, list) or any(not isinstance(v, str) for v in argv):
        raise EvidenceError('Invalid original stage arguments')
    indexes = [i for i, value in enumerate(argv) if value == name]
    if not indexes:
        return None
    if len(indexes) != 1 or indexes[0] + 1 == len(argv):
        raise EvidenceError('Ambiguous original diagnostic argument')
    return argv[indexes[0] + 1]


def _failure_files(failed, formal_entries, build_entries):
    formal, build, missing = {}, {}, []
    for stage in failed:
        report = _option(stage, '--report')
        if report is not None:
            raw = PurePosixPath(stage.get('raw', {}).get('path', ''))
            selected = PurePosixPath(report)
            if (not raw.is_absolute() or not selected.is_absolute() or
                    '..' in raw.parts or '..' in selected.parts or
                    raw.name != stage['name'] + '.raw'):
                raise EvidenceError('Unsafe original diagnostic report path')
            try:
                relative = selected.relative_to(raw.parent)
            except ValueError as error:
                raise EvidenceError('Report escapes original stage output') from error
            relative = store._safe_path(relative.as_posix())
            member = 'payload/software/' + relative
            if formal_entries.get(member, {}).get('kind') == 'file':
                formal[member] = 'original-files/software/' + relative
            else:
                missing.append('software/' + relative)
            prefix = str(PurePosixPath(member).with_suffix('.contexts')) + '/'
            for name, entry in formal_entries.items():
                if entry['kind'] == 'file' and name.startswith(prefix):
                    formal[name] = 'original-files/' + str(
                        PurePosixPath(name).relative_to('payload'))
        if stage.get('checker') == 'native_contracts':
            preset = _option(stage, '--preset')
            if preset not in NATIVE_PRESETS:
                raise EvidenceError('Unknown original Native preset')
            prefix = 'payload/' + preset + '/tdd/'
            found = False
            for name, entry in build_entries.items():
                if entry['kind'] == 'file' and name.startswith(prefix):
                    build[name] = 'original-files/build/' + str(
                        PurePosixPath(name).relative_to('payload'))
                    found = True
            if not found:
                missing.append('build/' + preset + '/tdd/')
            for suffix in ('ctest-results.xml', 'Testing/Temporary/LastTest.log'):
                member = 'payload/' + preset + '/' + suffix
                if build_entries.get(member, {}).get('kind') == 'file':
                    build[member] = 'original-files/build/' + preset + '/' + suffix
                else:
                    missing.append('build/' + preset + '/' + suffix)
    return formal, build, missing


def prepare(artifact, output, expected):
    """Verify both original store layers, retain diagnostics and split bytes."""
    expected = context(expected)
    artifact, output = safe_directory(artifact), safe_directory(output)
    if (output.exists() or output == artifact or artifact in output.parents or
            output in artifact.parents):
        raise EvidenceError('Diagnostic output must be new and separate')
    package, archive, signatures = _artifact(artifact)
    store.verify_package(package, archive_override=archive)
    output.mkdir(parents=True)
    diagnostics, transport, parts = (output / name for name in
                                     ('diagnostics', 'transport', 'parts'))
    for directory in (diagnostics, transport, parts):
        directory.mkdir()
    shutil.copyfile(artifact / 'formal-build-package.json',
                    transport / 'original-build-package.json')
    _extract(archive, {FORMAL_MEMBER: 'original-formal-package.json',
                      JOB_MEMBER: 'original-job-identity.json'}, diagnostics)
    envelope = diagnostics / 'original-formal-package.json'
    job = load_json(diagnostics / 'original-job-identity.json',
                    max_bytes=64 * 1024 ** 2)
    _identity(job, expected, envelope)
    formal = load_json(envelope)
    name = Path(formal['archive']['path']).name
    if name != formal['archive']['sha256'] + '.tar.gz':
        raise EvidenceError('Nested archive name differs from identity')
    inner = output / name
    _extract(archive, {'payload/formal-transfer/store/' + name: name},
             output, limit=PART_BYTES * MAX_PARTS)
    store.verify_package(formal, archive_override=inner)
    _extract(inner, {'payload/software/report.json': 'original-report.json'},
             diagnostics)
    report = load_json(diagnostics / 'original-report.json',
                       max_bytes=64 * 1024 ** 2)
    if (report.get('source_before', {}).get('commit') != expected['commit'] or
            report.get('status') not in ('failed', 'executed_passed', 'running') or
            not isinstance(report.get('stages'), list)):
        raise EvidenceError('Original report differs from requested source')
    failed = []
    names = set()
    for stage in report['stages']:
        name = stage.get('name')
        if (not isinstance(name, str) or '/' in name or
                store._safe_path(name) != name or name in names or
                stage.get('status') not in ('passed', 'failed')):
            raise EvidenceError('Invalid original stage identity')
        names.add(name)
        if stage['status'] != 'passed':
            failed.append(stage)
    _, entries = store._members(formal, archive_override=inner)
    build_entries = {}
    if any(stage.get('checker') == 'native_contracts' for stage in failed):
        _, build_entries = store._members(package, archive_override=archive)
    extra, build_selected, missing = _failure_files(failed, entries, build_entries)
    prefixes = tuple('payload/software/' + stage['name'] + '.attempts/'
                     for stage in failed)
    required = {'payload/software/' + stage['name'] + '.raw'
                for stage in failed}
    required.update('payload/software/' + name + '.command.json'
                    for name in names)
    selected, total = {}, 0
    for name, entry in entries.items():
        if entry['kind'] != 'file':
            continue
        if name in required or name.startswith(prefixes) or name in extra:
            total += entry['size']
            if total > DIAGNOSTIC_BYTES:
                raise EvidenceError('Complete diagnostics exceed size bound')
            relative = str(PurePosixPath(name).relative_to('payload'))
            selected[name] = 'original-files/' + relative
    if not required <= selected.keys():
        raise EvidenceError('Original stage raw or receipt is missing')
    total += sum(build_entries[name]['size'] for name in build_selected)
    if total > DIAGNOSTIC_BYTES:
        raise EvidenceError('Complete diagnostics exceed size bound')
    _extract(inner, selected, diagnostics, limit=DIAGNOSTIC_BYTES)
    if build_selected:
        _extract(archive, build_selected, diagnostics, limit=DIAGNOSTIC_BYTES)
    copied = []
    for role, records, chosen in (('formal', entries, selected),
                                 ('build', build_entries, build_selected)):
        for name, relative in chosen.items():
            path, expected_file = diagnostics / relative, records[name]
            with regular_file(path, nonempty=False).open('rb') as source:
                checksum = stream_digest(source)
            if (path.stat().st_size != expected_file['size'] or
                    checksum != expected_file['sha256']):
                raise EvidenceError('Copied original diagnostic bytes differ')
            copied.append({'archive_role': role, 'archive_member': name,
                           'output': relative, 'size': expected_file['size'],
                           'sha256': expected_file['sha256']})
    atomic_json(diagnostics / 'members.json', {'files': copied})
    summary = {'schema_version': 1, 'source_status': report['status'],
               'failure': report.get('failure'), 'failed_stages': failed,
               'executed_stages': len(report['stages']), 'context': expected,
               'missing_diagnostics': missing,
               'software_qualified': False,
               'scope': 'original diagnostics and byte transport only'}
    atomic_json(diagnostics / 'summary.json', summary)
    if sum(path.stat().st_size for path in diagnostics.rglob('*')
           if path.is_file()) > DIAGNOSTIC_BYTES:
        raise EvidenceError('Complete diagnostic output exceeds size bound')
    result = {**summary, 'part_bytes': PART_BYTES,
              'archive': package['archive'],
              'package': file_identity(transport / 'original-build-package.json'),
              'parts': _split(archive, parts, package['archive'])}
    final_package, final_archive, final_signatures = _artifact(artifact)
    if ((final_package, final_archive, final_signatures) !=
            (package, archive, signatures)):
        raise EvidenceError('Original artifact changed during extraction')
    atomic_json(transport / 'manifest.json', result)
    # Only our verified temporary copy is retired; originals remain untouched.
    inner.unlink()
    return result


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--run-attempt', required=True)
    args = parser.parse_args(arguments)
    try:
        result = prepare(args.artifact, args.output, {
            'commit': args.expected_commit, 'run_id': args.run_id,
            'attempt': args.run_attempt})
        print('Original formal status: ' + result['source_status'])
        for stage in result['failed_stages']:
            print('Original failed stage: ' + stage['name'])
        print('Verified byte parts: ' + str(len(result['parts'])))
        return 0
    except (EvidenceError, OSError, ValueError, KeyError, TypeError,
            tarfile.TarError) as error:
        print('Formal diagnostics rejected: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
