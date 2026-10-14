"""Independently retrieve an immutable evidence release and restore selected proofs."""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile
import threading
import time
from urllib.request import urlopen
import uuid


def parse(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('Duplicate JSON field')
            result[key] = value
        return result
    return json.loads(raw, object_pairs_hook=pairs)


def relative(name):
    if not isinstance(name, str) or not name or '\\' in name or ':' in name:
        raise ValueError('Invalid member path')
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or str(path) != name:
        raise ValueError('Unnormalized member path')
    return name


def safe(path):
    path = Path(path).absolute()
    if any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError('Symlink destination rejected')
    return path


def source_snapshot(root):
    def git(*args):
        return subprocess.run(['git', '-C', str(root), *args], capture_output=True, check=True).stdout
    status = git('status', '--porcelain=v1', '--untracked-files=all')
    return {'root': str(root), 'commit': git('rev-parse', 'HEAD').decode().strip(),
        'tree': git('rev-parse', 'HEAD^{tree}').decode().strip(), 'dirty': bool(status),
        'status_sha256': hashlib.sha256(status).hexdigest(),
        'dirty_diff_sha256': hashlib.sha256(git('diff', '--no-ext-diff', '--binary', 'HEAD')).hexdigest(),
        'submodules_sha256': hashlib.sha256(git('submodule', 'status', '--recursive')).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--asset-commit', required=True)
    parser.add_argument('--asset-prefix', required=True)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--audited-verifier-sha256', required=True)
    parser.add_argument('--archive-sha256', required=True)
    parser.add_argument('--inventory-sha256', required=True)
    parser.add_argument('--source-root', type=Path)
    parser.add_argument('--evidence-root', type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch('[0-9a-f]{40}', args.asset_commit):
        raise ValueError('Expected immutable remote commit')
    for value in (args.audited_verifier_sha256, args.archive_sha256, args.inventory_sha256):
        if not re.fullmatch('[0-9a-f]{64}', value):
            raise ValueError('Expected external SHA256 trust anchor')
    if not re.fullmatch('[A-Za-z0-9_.-]+', args.asset_prefix) or args.tag != args.asset_prefix:
        raise ValueError('Expected one explicit evidence version namespace')
    evidence = safe(args.evidence_root)
    source = safe(args.source_root) if args.source_root else None
    if source and (evidence.is_relative_to(source) or source.is_relative_to(evidence)):
        raise ValueError('Evidence output overlaps source')
    attempt = evidence / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + uuid.uuid4().hex)
    attempt.mkdir(parents=True)
    script = Path(__file__).read_bytes()
    receipt = {'schema_version': 1, 'kind': 'independent_remote_documentation_evidence_restore',
        'status': 'running', 'physical_status': 'not_executed',
        'started_at': datetime.now(timezone.utc).isoformat(), 'command': [sys.executable, *sys.argv],
        'asset_commit': args.asset_commit, 'asset_prefix': args.asset_prefix, 'tag': args.tag,
        'script_sha256': hashlib.sha256(script).hexdigest(),
        'audited_verifier_sha256': args.audited_verifier_sha256,
        'source_before': source_snapshot(source) if source else None,
        'source_scope': 'Observer provenance only; portable verification and restoration require no Nexus source or installed prefix.',
        'commands': [], 'requests': [], 'hash_cycle_boundary': 'This receipt and its observations are outside the verified D2 archive; publish separately.'}
    record = attempt / 'execution.json'
    def save():
        record.write_text(json.dumps(receipt, indent=2) + '\n')
    save()
    started = time.monotonic()
    stop = threading.Event()
    def heartbeat():
        while not stop.wait(30):
            print(json.dumps({'status': 'running', 'elapsed_seconds': round(time.monotonic()-started, 1),
                'scope': 'real HTTPS evidence retrieval, full archive verification, selected portable restore'}), flush=True)
    beat = threading.Thread(target=heartbeat, daemon=True)
    beat.start()
    def api(path, label):
        command = ['gh', 'api', 'repos/X-Gen-Lab/nexus/' + path]
        response = subprocess.run(command, capture_output=True, timeout=60)
        out = attempt / (label + '.json'); err = attempt / (label + '.stderr')
        out.write_bytes(response.stdout); err.write_bytes(response.stderr)
        receipt['commands'].append({'argv': command, 'returncode': response.returncode,
            'stdout': str(out), 'stdout_sha256': hashlib.sha256(response.stdout).hexdigest(), 'stderr': str(err)})
        if response.returncode:
            raise ValueError('Actual remote API request failed: ' + label)
        return parse(response.stdout)
    base = 'https://raw.githubusercontent.com/X-Gen-Lab/nexus/' + args.asset_commit + '/'
    def fetch(name, expected):
        relative(name)
        destination = safe(attempt / 'downloaded' / name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        sha = hashlib.sha256(); blob = hashlib.sha1(('blob ' + str(expected['bytes']) + '\0').encode()); count = 0
        began = time.monotonic()
        with urlopen(base + name, timeout=60) as response, destination.open('xb') as output:
            if response.status != 200 or response.geturl() != base + name:
                raise ValueError('Remote HTTP status or URL changed')
            while block := response.read(1024*1024):
                count += len(block); sha.update(block); blob.update(block); output.write(block)
                if count > expected['bytes'] or time.monotonic()-started > 600:
                    raise ValueError('Remote retrieval exceeds byte or time bound')
        if count != expected['bytes'] or ('sha256' in expected and sha.hexdigest() != expected['sha256']) or blob.hexdigest() != expected['blob']:
            raise ValueError('Remote bytes or SHA identity mismatch: ' + name)
        receipt['requests'].append({'url': base + name, 'final_url': base + name, 'status': 200,
            'bytes': count, 'sha256': sha.hexdigest(), 'git_blob_sha1': blob.hexdigest(),
            'path': str(destination), 'elapsed_seconds': round(time.monotonic()-began, 3)})
        save()
        return destination
    try:
        commit = api('git/commits/' + args.asset_commit, 'remote-commit')
        if commit['sha'] != args.asset_commit:
            raise ValueError('Remote Git commit differs')
        tree = api('git/trees/' + commit['tree']['sha'] + '?recursive=1', 'remote-tree')
        if tree.get('truncated'):
            raise ValueError('Remote Git tree truncated')
        blobs = {row['path']: row for row in tree['tree'] if row['type'] == 'blob'}
        tag = api('git/ref/tags/' + args.tag, 'remote-tag')['object']
        for i in range(5):
            if tag['type'] == 'commit': break
            if tag['type'] != 'tag': raise ValueError('Unexpected Git tag object')
            tag = api('git/tags/' + tag['sha'], 'remote-tag-object-' + str(i))['object']
        if tag['type'] != 'commit' or tag['sha'] != args.asset_commit:
            raise ValueError('Immutable evidence tag does not bind D2 commit')
        item = blobs['assets.json']
        asset_index = fetch('assets.json', {'bytes': item['size'], 'blob': item['sha']})
        index = parse(asset_index.read_bytes())
        members = {}
        for row in index['members']:
            name = relative(row['path'])
            if name in members or type(row['bytes']) is not int or row['bytes'] < 0 or not re.fullmatch('[0-9a-f]{64}', row['sha256']):
                raise ValueError('Remote asset inventory invalid')
            members[name] = row
        prefix = args.asset_prefix + '/'
        wanted = {name for name in members if name.startswith(prefix)}
        actual = {name for name in blobs if name.startswith(prefix)}
        if wanted != actual or len(wanted) != 11:
            raise ValueError('Expected exact eleven-member remote evidence namespace')
        paths = {}
        for name in sorted(wanted):
            if blobs[name]['size'] != members[name]['bytes']:
                raise ValueError('Git tree and inventory byte count differ')
            paths[name.removeprefix(prefix)] = fetch(name, {**members[name], 'blob': blobs[name]['sha']})
        export = parse(paths['export-receipt.json'].read_bytes())
        if set(paths) != set(export['upload_members']) | {'export-receipt.json'} or export['status'] != 'passed':
            raise ValueError('Export upload closure or status differs')
        if export['archive_sha256'] != args.archive_sha256 or export['inventory_sha256'] != args.inventory_sha256:
            raise ValueError('Export differs from external expected identity')
        verifier_path = paths['VERIFY-EVIDENCE.py']
        if hashlib.sha256(verifier_path.read_bytes()).hexdigest() != args.audited_verifier_sha256:
            raise ValueError('Downloaded verifier is not byte-identical to audited helper')
        spec = importlib.util.spec_from_file_location('audited_immutable_evidence_verifier', verifier_path)
        helper = importlib.util.module_from_spec(spec); spec.loader.exec_module(helper)
        inventory_raw = paths['EVIDENCE-INVENTORY.json'].read_bytes()
        if hashlib.sha256(inventory_raw).hexdigest() != args.inventory_sha256:
            raise ValueError('Downloaded outer inventory differs')
        inventory = helper.parse_json(inventory_raw)
        expected = {row['path']: {'bytes': row['bytes'], 'sha256': row['sha256']} for row in inventory['members']}
        expected['EVIDENCE-INVENTORY.json'] = {'bytes': len(inventory_raw), 'sha256': args.inventory_sha256}
        joined = attempt / 'joined-evidence.tar.gz'
        joined_result = helper.join_parts(paths['evidence-parts.json'], joined)
        actual_join = helper.file_identity(joined)[0]
        if actual_join != {'bytes': export['archive_bytes'], 'sha256': args.archive_sha256} or joined_result != export['actual_tar_verification']:
            raise ValueError('Actual ordered part join or full audit result differs')
        if joined_result['members'] != 17118 or joined_result['payload_files'] != 17113:
            raise ValueError('Final evidence closure counts differ')
        with tarfile.open(joined, 'r:gz') as archive:
            accepted = helper.parse_json(archive.extractfile('payload/final-closure/accepted-task-records.json').read())
        if (accepted['status'] != 'passed' or accepted['validation_scope'] != 'accepted_docs_exit' or
                accepted['task_count'] != 58 or len(accepted['records']) != 58 or
                {row['task_id'] for row in accepted['records']} != {f'DOC-{i:02d}' for i in range(1,59)} or
                any(row['status'] != 'passed' or row['validation_scope'] != 'accepted_docs_exit' for row in accepted['records'])):
            raise ValueError('Required 58 accepted documentation task records incomplete')
        references = {}
        for row in accepted['records']:
            for entry in row['evidence']:
                name = helper.relative(entry['member_path'])
                wanted_identity = {key: entry[key] for key in ('bytes', 'sha256')}
                if name not in expected or expected[name] != wanted_identity or name in references and references[name] != wanted_identity:
                    raise ValueError('Accepted task proof does not bind an actual archived member')
                references[name] = wanted_identity
        selected = set(references) | {'payload/final-closure/final-review.json',
            'payload/final-closure/accepted-task-records.json', 'payload/final-closure/candidate-ledger.csv',
            'payload/pages-hidden-fix/normal-gates/receipt.json'}
        if len(selected) != 36 or not selected <= set(expected):
            raise ValueError('Expected exact 36 selected task proof members')
        selected |= {'EVIDENCE-INVENTORY.json', 'EVIDENCE-LOCATORS.json', 'EVIDENCE-EXCLUSIONS.json', 'VERIFY-EVIDENCE.py', 'README.txt'}
        restored = safe(attempt / 'independent-reader'); restored.mkdir()
        observations = []; restored_rows = []; observed = set()
        with tarfile.open(joined, 'r|gz') as archive:
            for member in archive:
                name = helper.relative(member.name)
                if name in observed or not member.isfile() or name not in expected:
                    raise ValueError('Changed/unregistered archive member during selected restoration')
                observed.add(name); digest = hashlib.sha256(); size = 0
                output = None
                if name in selected:
                    destination = safe(restored / name)
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    output = destination.open('xb')
                try:
                    stream = archive.extractfile(member)
                    while block := stream.read(1024*1024):
                        size += len(block); digest.update(block)
                        if output: output.write(block)
                finally:
                    if output: output.close()
                identity = {'bytes': size, 'sha256': digest.hexdigest()}
                if identity != expected[name]:
                    raise ValueError('Archive bytes changed during selected restoration')
                item = {'path': name, **identity}
                observations.append(item)
                if name in selected: restored_rows.append(item)
        if observed != set(expected) or {row['path'] for row in restored_rows} != selected or helper.file_identity(joined)[0] != actual_join:
            raise ValueError('Complete observed/restored member closure differs')
        ledger_path = restored / 'payload/final-closure/candidate-ledger.csv'
        with ledger_path.open() as stream:
            ledger = {row['task_id']: row for row in csv.DictReader(stream)}
        if set(ledger) != {row['task_id'] for row in accepted['records']} or any(ledger[row['task_id']]['exit_condition'] != row['original_exit_condition'] for row in accepted['records']):
            raise ValueError('Independent reader original exit conditions differ')
        for row in restored_rows:
            identity = helper.file_identity(restored / row['path'])[0]
            if identity != {key: row[key] for key in ('bytes','sha256')}:
                raise ValueError('Restored reader member changed')
        observation_file = attempt / 'archive-member-observations.json'
        observation_file.write_text(json.dumps({'schema_version':1,'status':'passed','members':observations},indent=2)+'\n')
        reader_file = attempt / 'selected-reader-members.json'
        reader_file.write_text(json.dumps({'schema_version':1,'status':'passed','task_records':58,'proof_members':36,'control_members':5,'restored_root':str(restored),'members':restored_rows},indent=2)+'\n')
        receipt.update(status='passed',returncode=0,archive_bytes=actual_join['bytes'],archive_sha256=actual_join['sha256'],
            inventory_sha256=args.inventory_sha256,actual_full_archive_verification=joined_result,
            actual_remote_files=11,observed_members=len(observations),selected_proof_members=36,
            restored_reader_members=len(restored_rows),restored_reader_root=str(restored),accepted_task_records=58,
            member_observations={'path':str(observation_file),**helper.file_identity(observation_file)[0]},
            selected_reader={'path':str(reader_file),**helper.file_identity(reader_file)[0]},
            limitations=['Hashes are not signatures.','Documentation evidence grants no firmware or physical HIL qualification.','Selected reader restore is 36 proofs plus five controls; all 17118 archive members were actually streamed and verified.'])
        code = 0
    except Exception as error:
        receipt.update(status='failed',returncode=1,error=str(error));code=1
    finally:
        stop.set();beat.join()
        receipt.update(elapsed_seconds=round(time.monotonic()-started,3),finished_at=datetime.now(timezone.utc).isoformat(),source_after=source_snapshot(source) if source else None)
        if receipt['source_before'] != receipt['source_after'] or hashlib.sha256(Path(__file__).read_bytes()).hexdigest() != receipt['script_sha256']:
            receipt.update(status='failed',returncode=1,error='Observer source or execution script changed during verification');code=1
        save()
    print(json.dumps({'status':receipt['status'],'receipt':str(record),'elapsed_seconds':receipt['elapsed_seconds'],'restored_reader_root':receipt.get('restored_reader_root')}),flush=True)
    return code

if __name__ == '__main__':
    raise SystemExit(main())
