#!/usr/bin/env python3
"""Fixed P2 outer archive parts only; no network, resealing or deletion."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import stat

P2 = 'ab53225033dfb759e93c90beb6301a3048bbcfda'
TREE = '2309ae92e019fac68c6f463cbee205c3726fe73d'
SOURCE = '/dev/shm/nexus-final-20261103'
BASE = Path('/workspace/nexus/build/recovery-20261103/final-candidates-ab532250')
PACKAGE = BASE / 'local-preview.package.json'
OUTPUT = Path('/workspace/nexus/build/recovery-20261103/publication-p2-transport-ab532250-20261104')
NAMESPACE = 'docs-public-p2-ab532250-20261104'
REPOSITORY = 'X-Gen-Lab/nexus'
LIMIT = 32 * 1024**2
RESERVE = 64 * 1024**2
MANIFEST = 'transport.json'
ENVELOPE = 'local-preview.package.json'
HELPER = 'p2_outer_transport.py'


def parse(data):
    def pairs(rows):
        result = {}
        for key, value in rows:
            if key in result:
                raise ValueError('Duplicate JSON key')
            result[key] = value
        return result
    return json.loads(data.decode('utf-8'), object_pairs_hook=pairs)


def encode(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode()


def canonical(path, *, existing=False):
    path = Path(path)
    if (not path.is_absolute() or path != path.resolve()
            or any(p.is_symlink() for p in (path, *path.parents))
            or existing and not path.exists()):
        raise ValueError('Canonical nonsymlink path required')
    return path


def regular(path):
    path = canonical(path, existing=True)
    if not path.is_file():
        raise ValueError('Regular file required')
    return path


def signature(path):
    s = path.stat()
    return s.st_dev, s.st_ino, s.st_mode, s.st_size, s.st_mtime_ns, s.st_ctime_ns


def small(path):
    path = regular(path)
    if path.stat().st_size > LIMIT:
        raise ValueError('Bounded metadata exceeds 32 MiB')
    before = signature(path)
    data = path.read_bytes()
    if signature(path) != before:
        raise ValueError('Metadata changed during read')
    return data


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_new(path, data):
    with path.open('xb') as stream:
        stream.write(data)
    path.chmod(0o644)


def split_stream(stream, writer, *, part_bytes=LIMIT):
    if type(part_bytes) is not int or not 0 < part_bytes <= LIMIT:
        raise ValueError('Bounded positive part size required')
    whole, total, parts = hashlib.sha256(), 0, []
    while data := stream.read(part_bytes):
        if len(data) > part_bytes:
            raise ValueError('Read exceeded part bound')
        name = f'part-{len(parts):05d}'
        writer(name, data)
        parts.append({'name': name, 'ordinal': len(parts), 'offset': total,
                      'size': len(data), 'sha256': sha(data), 'mode': '100644'})
        total += len(data)
        whole.update(data)
    if not parts:
        raise ValueError('Empty outer archive is not admitted')
    return {'size': total, 'sha256': whole.hexdigest(), 'parts': parts}


def join_stream(plan, reader, output, *, part_bytes=LIMIT):
    whole, total, seen = hashlib.sha256(), 0, set()
    parts = plan['parts']
    if not isinstance(parts, list) or not parts:
        raise ValueError('Explicit nonempty part closure required')
    for index, part in enumerate(parts):
        if (set(part) != {'name', 'ordinal', 'offset', 'size', 'sha256', 'mode'}
                or part['name'] != f'part-{index:05d}' or part['name'] in seen
                or type(part['ordinal']) is not int or part['ordinal'] != index
                or type(part['offset']) is not int or part['offset'] != total
                or type(part['size']) is not int or not 0 < part['size'] <= part_bytes
                or index < len(parts) - 1 and part['size'] != part_bytes
                or part['mode'] != '100644' or not re.fullmatch('[0-9a-f]{64}', part['sha256'])):
            raise ValueError('Part sequence/order/offset/size/mode differs')
        seen.add(part['name'])
        digest, count = hashlib.sha256(), 0
        for block in reader(part['name']):
            count += len(block)
            if count > part['size']:
                raise ValueError('Part exceeded declared size')
            digest.update(block)
            whole.update(block)
            output.write(block)
        if count != part['size'] or digest.hexdigest() != part['sha256']:
            raise ValueError('Independent part SHA/size differs')
        total += count
    if total != plan['size'] or whole.hexdigest() != plan['sha256']:
        raise ValueError('Independent joined whole archive SHA/size differs')
    return {'size': total, 'sha256': whole.hexdigest(), 'parts': len(parts)}


def package_identity(data, archive):
    value = parse(data)
    if (set(value) != {'schema_version', 'kind', 'archive', 'manifest_sha256',
                      'files', 'bytes', 'signature_status', 'product_promotion'}
            or type(value['schema_version']) is not int or value['schema_version'] != 1
            or value['kind'] != 'portable_evidence' or value['signature_status'] != 'unsigned'
            or value['product_promotion'] is not False
            or set(value['archive']) != {'path', 'size', 'sha256'}
            or value['archive'] != {k: archive[k] for k in ('path', 'size', 'sha256')}
            or type(value['files']) is not int or value['files'] <= 0
            or type(value['bytes']) is not int or value['bytes'] <= 0
            or not re.fullmatch('[0-9a-f]{64}', value['manifest_sha256'])):
        raise ValueError('Original unsigned outer portable envelope differs')
    return value


def prepare():
    """Only call after the real local bundle operation has completely passed."""
    if canonical(OUTPUT).exists():
        raise ValueError('Fixed transfer output must be fresh; preserve partial attempts')
    state_path = BASE / 'bundle-control/operator-state.json'
    state_raw = small(state_path)
    state = parse(state_raw)
    operations = state.get('completed_actual_operations', [])
    if (state.get('status') != 'passed' or state.get('operation') != 'P2-unsigned-local-preview'
            or state.get('source_commit') != P2 or state.get('source_tree') != TREE
            or state.get('publication_authorized') is not False or state.get('signature_status') != 'unsigned'
            or [x.get('name') for x in operations] != ['prepare-unsigned-local-preview',
                'verify-original-local-preview', 'verify-actually-restored-local-preview']
            or any(type(x.get('exit_code')) is not int or x['exit_code'] != 0 for x in operations)):
        raise ValueError('Complete actual exact-P2 unsigned local bundle owner state required')
    package_raw = small(PACKAGE)
    package = parse(package_raw)
    archive = regular(package['archive']['path'])
    if archive.parent != BASE / 'store' or archive.name != package['archive']['sha256'] + '.tar.gz':
        raise ValueError('Only the original exact P2 outer store archive is admitted')
    package_identity(package_raw, package['archive'])
    before = signature(archive)
    if before[3] != package['archive']['size'] or type(package['archive']['size']) is not int or package['archive']['size'] <= 0:
        raise ValueError('Outer archive original size differs')
    # The existing append retains payload and authenticated readback copies.
    # Admit both before starting; this observation does not reserve capacity.
    existing_parent = next(p for p in (OUTPUT.parent, *OUTPUT.parents) if p.exists())
    if shutil.disk_usage(existing_parent).free < 2 * before[3] + RESERVE:
        raise ValueError('Insufficient upload parts plus readback capacity')
    OUTPUT.mkdir(parents=True)
    payload = OUTPUT / 'payload'
    payload.mkdir()
    helper_raw = small(Path(__file__).resolve())
    with archive.open('rb') as stream:
        plan = split_stream(stream, lambda name, data: write_new(payload / name, data))
    if (plan['size'] != package['archive']['size'] or plan['sha256'] != package['archive']['sha256']
            or signature(archive) != before or small(PACKAGE) != package_raw or small(state_path) != state_raw):
        raise ValueError('Original outer archive/envelope/owner state changed during split')
    write_new(payload / ENVELOPE, package_raw)
    write_new(payload / HELPER, helper_raw)
    descriptor = {'schema_version': 1, 'kind': 'fixed_P2_unsigned_outer_bundle_transport',
                  'source': {'repository': REPOSITORY, 'commit': P2, 'tree': TREE},
                  'namespace': NAMESPACE, 'part_limit_bytes': LIMIT,
                  'archive': {**package['archive'], 'mode': stat.S_IMODE(archive.stat().st_mode)},
                  'parts': plan['parts'],
                  'envelope': {'name': ENVELOPE, 'size': len(package_raw), 'sha256': sha(package_raw),
                               'original_path': str(PACKAGE), 'original_mode': stat.S_IMODE(PACKAGE.stat().st_mode), 'mode': '100644'},
                  'helper': {'name': HELPER, 'size': len(helper_raw), 'sha256': sha(helper_raw), 'mode': '100644'},
                  'actual_bundle_owner_state': {'path': str(state_path), 'size': len(state_raw), 'sha256': sha(state_raw)},
                  'scope': 'Only the completed public P2 outer bundle. Inner SDK/consumer/eight candidates/docs remain unchanged. No private examples, six private historical archives, mixed f9 archive, or 42+4 local-only Native retention archives are selected.',
                  'signature_status': 'unsigned', 'publication_authorized': False,
                  'physical_status': 'not_executed', 'remote_inner_restore': 'pending separate actual operation',
                  'remote_live_verification': 'pending separate actual operation'}
    descriptor_raw = encode(descriptor)
    if len(descriptor_raw) > LIMIT:
        raise ValueError('Outer descriptor exceeds one bounded blob')
    write_new(payload / MANIFEST, descriptor_raw)
    append_raw = ('\nUnsigned public P2 software evidence: `' + NAMESPACE + '/transport.json` preserves the original local bundle through ordered <=32 MiB opaque parts. The original envelope is unchanged; recovery requires actual store restore and live producer verification. No product release, remote CI/OIDC, physical HIL or remote Native-program recovery is asserted.\n').encode()
    write_new(OUTPUT / 'README.append.txt', append_raw)
    selected = [*plan['parts'], {'name': ENVELOPE, 'size': len(package_raw), 'sha256': sha(package_raw), 'mode': '100644'},
                {'name': HELPER, 'size': len(helper_raw), 'sha256': sha(helper_raw), 'mode': '100644'},
                {'name': MANIFEST, 'size': len(descriptor_raw), 'sha256': sha(descriptor_raw), 'mode': '100644'}]
    config = {'repository': REPOSITORY, 'branch': 'documentation-assets', 'expected_visibility': 'public',
              'sources': [{'repository': REPOSITORY, 'commit': P2}], 'namespace': NAMESPACE,
              'payload_root': str(payload), 'files': [{'local': str(payload / x['name']),
                  'path': NAMESPACE + '/' + x['name'], 'bytes': x['size'], 'sha256': x['sha256'], 'mode': x['mode']} for x in selected],
              'readme_append_utf8': str(OUTPUT / 'README.append.txt'), 'control': str(OUTPUT / 'append-control'),
              'readback_root': str(OUTPUT / 'readback'), 'message': 'chore(assets): append exact unsigned P2 software bundle'}
    write_new(OUTPUT / 'append.config.actual.json', encode(config))
    write_new(OUTPUT / 'prepared.json', encode({'kind': 'actual_completed_bundle_transport_preparation',
              'status': 'actual_parts_and_config_prepared_not_uploaded', 'source': descriptor['source'],
              'archive': descriptor['archive'], 'descriptor_sha256': sha(descriptor_raw),
              'parts': len(plan['parts']), 'config': str(OUTPUT / 'append.config.actual.json'),
              'no_network_or_deletion': True, 'remote_recovery': 'not executed'}))
    print(json.dumps({'status': 'actual_parts_and_config_prepared_not_uploaded', 'output': str(OUTPUT), 'descriptor_sha256': sha(descriptor_raw)}))


def join(directory, output, expected_descriptor_sha256):
    """Use authenticated exact-commit readback bytes after append has finished."""
    directory = canonical(directory, existing=True)
    output = canonical(output)
    if (not directory.is_dir() or output.exists() or output.is_relative_to(directory)
            or not output.parent.is_dir() or not re.fullmatch('[0-9a-f]{64}', expected_descriptor_sha256)):
        raise ValueError('New disjoint join file and trusted actual descriptor SHA required')
    raw = small(directory / MANIFEST)
    if sha(raw) != expected_descriptor_sha256:
        raise ValueError('Authenticated original descriptor byte identity differs')
    descriptor = parse(raw)
    required = {'schema_version', 'kind', 'source', 'namespace', 'part_limit_bytes', 'archive', 'parts', 'envelope',
                'helper', 'actual_bundle_owner_state', 'scope', 'signature_status', 'publication_authorized',
                'physical_status', 'remote_inner_restore', 'remote_live_verification'}
    if (set(descriptor) != required or descriptor['schema_version'] != 1
            or descriptor['kind'] != 'fixed_P2_unsigned_outer_bundle_transport'
            or descriptor['source'] != {'repository': REPOSITORY, 'commit': P2, 'tree': TREE}
            or descriptor['namespace'] != NAMESPACE or descriptor['part_limit_bytes'] != LIMIT
            or descriptor['signature_status'] != 'unsigned' or descriptor['publication_authorized'] is not False
            or descriptor['physical_status'] != 'not_executed'):
        raise ValueError('Fixed public P2 unsigned descriptor contract differs')
    arc = descriptor['archive']
    if (set(arc) != {'path', 'size', 'sha256', 'mode'} or type(arc['size']) is not int or arc['size'] <= 0
            or not re.fullmatch('[0-9a-f]{64}', arc['sha256']) or type(arc['mode']) is not int
            or not 0 <= arc['mode'] <= 0o777 or arc['path'] != str(BASE / 'store' / (arc['sha256'] + '.tar.gz'))):
        raise ValueError('Fixed original outer archive identity differs')
    for name, record in ((ENVELOPE, descriptor['envelope']), (HELPER, descriptor['helper'])):
        data = small(directory / name)
        if record['name'] != name or record['mode'] != '100644' or len(data) != record['size'] or sha(data) != record['sha256']:
            raise ValueError('Exact original envelope/helper byte identity differs')
    package_identity(small(directory / ENVELOPE), arc)
    names = {p['name'] for p in descriptor['parts']} | {MANIFEST, ENVELOPE, HELPER}
    observed = list(directory.iterdir())
    if {p.name for p in observed} != names or any(p.is_symlink() or not p.is_file() or stat.S_IMODE(p.stat().st_mode) != 0o644 for p in observed):
        raise ValueError('Exact regular readback member/mode closure differs')
    partial = output.with_name(output.name + '.partial')
    receipt = output.with_name(output.name + '.join.json')
    if partial.exists() or receipt.exists() or shutil.disk_usage(output.parent).free < arc['size'] + RESERVE:
        raise ValueError('Join temporary/receipt must be new with complete archive capacity')
    def reader(name):
        path = regular(directory / name)
        before = signature(path)
        with path.open('rb') as stream:
            while block := stream.read(1024 * 1024):
                yield block
        if signature(path) != before:
            raise ValueError('Authenticated part changed during independent join')
    with partial.open('xb') as stream:
        result = join_stream({**arc, 'parts': descriptor['parts']}, reader, stream)
        stream.flush()
        os.fsync(stream.fileno())
    partial.chmod(arc['mode'])
    partial.replace(output)
    write_new(receipt, encode({'kind': 'actual_independent_P2_outer_archive_join',
              'status': 'all_parts_and_complete_original_archive_SHA_size_verified', 'archive': str(output),
              **result, 'archive_mode': arc['mode'], 'descriptor_sha256': expected_descriptor_sha256,
              'original_envelope_unchanged': True, 'inner_actual_restore': 'pending separate maintained store operation',
              'live_verify_local': 'pending separate operation with live original producers', 'no_deletion': True}))
    print(json.dumps({'status': 'independent_whole_archive_joined_restore_and_live_verify_pending', 'output': str(output), **result}))


def self_test():
    data = b'01234567abcdefghXYZ9876'
    fragments = {}
    plan = split_stream(io.BytesIO(data), lambda name, raw: fragments.__setitem__(name, raw), part_bytes=8)
    assert len(plan['parts']) == 3
    target = io.BytesIO()
    actual = join_stream(plan, lambda name: [fragments[name][:3], fragments[name][3:]], target, part_bytes=8)
    assert target.getvalue() == data and actual['sha256'] == sha(data)
    for operation in ('corrupt', 'order'):
        changed = parse(encode(plan))
        if operation == 'order':
            changed['parts'][0], changed['parts'][1] = changed['parts'][1], changed['parts'][0]
        try:
            join_stream(changed, lambda name: [b'!!!!!!!!' if operation == 'corrupt' else fragments[name]], io.BytesIO(), part_bytes=8)
        except ValueError:
            pass
        else:
            raise AssertionError('Corrupt/reordered tiny parts must fail')
    print(json.dumps({'status': 'tiny_three_parts_self_test_passed', 'bytes': len(data), 'parts': 3,
                      'part_bytes': 8, 'corrupt_and_order_rejected': True,
                      'no_original_bundle_read_or_network': True}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--prepare', action='store_true')
    mode.add_argument('--join', action='store_true')
    mode.add_argument('--self-test', action='store_true')
    parser.add_argument('--directory', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--expected-descriptor-sha256')
    args = parser.parse_args()
    if args.prepare:
        prepare()
    elif args.self_test:
        self_test()
    else:
        if args.directory is None or args.output is None or args.expected_descriptor_sha256 is None:
            parser.error('--join requires --directory --output --expected-descriptor-sha256')
        join(args.directory, args.output, args.expected_descriptor_sha256)
