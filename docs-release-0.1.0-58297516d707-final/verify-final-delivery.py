#!/usr/bin/env python3
"""Verify immutable final software-delivery evidence without executing an SDK."""
from pathlib import Path, PurePosixPath
import hashlib
import io
import json
import tarfile

C = '58297516d707fdee117bd27b5df6af09d72edd2e'
E2 = '51bebd47dd63dce90c7fded1efafef27cd20fd3e'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def safe_name(name):
    relative = PurePosixPath(name)
    if not name or relative.is_absolute() or '..' in relative.parts or str(relative) != name:
        raise ValueError('Unsafe or noncanonical evidence member')

def outer_records(path, expected):
    seen, values = set(), {}
    with tarfile.open(path, 'r:gz') as archive:
        for member in archive:
            safe_name(member.name)
            if not member.isfile() or member.name in seen or member.name not in expected:
                raise ValueError('Unexpected final evidence member')
            data = archive.extractfile(member).read()
            row = expected[member.name]
            if len(data) != row['bytes'] or sha(data) != row['sha256']:
                raise ValueError('Final evidence member identity differs')
            seen.add(member.name)
            values[member.name] = data
    if seen != set(expected):
        raise ValueError('Final evidence member omitted')
    return values

def check_hook(data, expected):
    count, seen = 0, set()
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
        for member in archive:
            name = member.name.rstrip('/') if member.isdir() else member.name
            safe_name(name)
            if name in seen or not (member.isfile() or member.isdir()):
                raise ValueError('Unsafe or duplicate raw hook member')
            seen.add(name)
            count += 1
            if member.isfile() and len(archive.extractfile(member).read()) != member.size:
                raise ValueError('Truncated original raw hook member')
    if count != expected:
        raise ValueError('Raw hook member count differs')
    return count

def check_restore(records):
    prefix = 'independent-release-restore/'
    inventory_data = records[prefix + 'local-evidence-files.json']
    inventory = json.loads(inventory_data)
    capsule = json.loads(records[prefix + 'local-evidence-capsule.json'])
    expected_files = inventory['files']
    expected_dirs = set(inventory['directories'])
    seen_files, seen_dirs, total_bytes = set(), set(), 0
    with tarfile.open(fileobj=io.BytesIO(records[prefix + 'independent-release-restore-evidence.tar.gz']), mode='r:gz') as archive:
        for member in archive:
            name = member.name.rstrip('/') if member.isdir() else member.name
            safe_name(name)
            if name in seen_files or name in seen_dirs:
                raise ValueError('Duplicate restoration member')
            if member.isdir():
                if name not in expected_dirs:
                    raise ValueError('Undeclared restoration directory')
                seen_dirs.add(name)
            elif member.isfile():
                data = archive.extractfile(member).read()
                if name == 'local-evidence-files.json':
                    if data != inventory_data:
                        raise ValueError('Restoration self-index differs')
                else:
                    if name not in expected_files:
                        raise ValueError('Undeclared restoration file')
                    row = expected_files[name]
                    if len(data) != row['size'] or sha(data) != row['sha256'] or member.mode != row['mode']:
                        raise ValueError('Restoration size/hash/mode differs')
                seen_files.add(name)
                total_bytes += len(data)
            else:
                raise ValueError('Nonregular restoration member')
    if (seen_files != set(expected_files) | {'local-evidence-files.json'} or
            seen_dirs != expected_dirs or len(seen_files) != capsule['regular_members'] or
            len(seen_dirs) != capsule['directory_members'] or
            total_bytes != capsule['raw_regular_bytes']):
        raise ValueError('Restoration closure differs')
    return len(seen_files), len(seen_dirs)

def main():
    root = Path(__file__).resolve().parent
    retention = json.loads((root / 'final-retention.json').read_bytes())
    for row in retention['files']:
        path = root / row['path']
        if path.is_symlink() or not path.is_file():
            raise ValueError('Final standalone file missing or symbolic')
        data = path.read_bytes()
        if len(data) != row['bytes'] or sha(data) != row['sha256']:
            raise ValueError('Final standalone file identity differs')
    allowlist = json.loads((root / 'final-allowlist.json').read_bytes())
    records = outer_records(root / 'final-evidence.tar.gz', {r['path']: r for r in allowlist['archive_members']})
    completion = json.loads(records['final-publication/software-delivery-completion.json'])
    acceptance = json.loads(records['independent-release-restore/independent-release-acceptance.json'])
    cleanup = json.loads(records['cleanup-results/platform-cleanup.json'])
    runtime = json.loads(records['release-monitor/recovery-51bebd47/38687802092-completed-runtime-receipt.json'])
    if (completion['software_delivery_status'] != 'passed' or completion['source']['main_commit'] != C or
            completion['delivery_builder']['commit'] != E2 or completion['release']['asset_count'] != 13 or
            completion['platform_cleanup']['exact_lease_remote_deletions'] != 6):
        raise ValueError('Software completion identity differs')
    fresh = acceptance['fresh_downloaded_SDK_consumers']
    if (acceptance['status'] != 'passed' or acceptance['source_commit'] != C or
            acceptance['builder_commit'] != E2 or acceptance['public_asset_count'] != 13 or
            acceptance['cryptographic_verification']['status'] != 'passed' or
            acceptance['cryptographic_verification']['signer_digest'] != E2 or
            acceptance['immutable_source_comparison']['all_raw_source_files'] != 5415 or
            fresh['profile_count'] != 7 or fresh['image_count'] != 10 or
            fresh['command_count'] != 28 or fresh['all_exit_codes_zero'] is not True or
            acceptance['physical_hil'] != 'not_executed'):
        raise ValueError('Independent downloaded SDK acceptance differs')
    if (cleanup['status'] != 'passed' or cleanup['source_commit'] != C or cleanup['builder_commit'] != E2 or
            {r['number'] for r in cleanup['pr_actions']} != {5, 6, 7, 8} or
            len(cleanup['remote_deletions']) != 6 or
            any(r['exit_code'] != 0 or not r['ancestor_of_main'] for r in cleanup['remote_deletions']) or
            cleanup['local_heads_unchanged'] is not True or
            cleanup['local_worktree_registrations_unchanged'] is not True):
        raise ValueError('Actual cleanup receipt differs')
    if (runtime['run_id'] != 38687802092 or runtime['source_commit'] != C or runtime['builder_commit'] != E2 or
            runtime['run_conclusion'] != 'success' or
            {r['name'] for r in runtime['required_jobs']} != {'guard', 'source-sdk', 'publish'} or
            any(r['conclusion'] != 'success' for r in runtime['required_jobs'])):
        raise ValueError('Actual R3 runtime differs')
    hooks = [check_hook(records['delivery-hook-evidence/hook-tdd.tar.gz'], 14293),
             check_hook(records['publisher-condition-hook-evidence/hook-tdd.tar.gz'], 15733)]
    restored_files, restored_dirs = check_restore(records)
    print(json.dumps({'status': 'passed', 'source_commit': C, 'builder_commit': E2,
        'final_evidence_members': len(records), 'raw_hook_members': hooks,
        'restoration_regular_files': restored_files, 'restoration_directories': restored_dirs,
        'actual_r3_required_success_jobs': 3, 'actual_public_release_assets': 13,
        'fresh_profiles_images_commands': [7, 10, 28], 'closed_superseded_prs': 4,
        'exact_lease_deletions': 6, 'scope': 'Byte restoration and retained software-preview observations only; no SDK test rerun, physical HIL, production qualification or new publication by this reader.'}))

if __name__ == '__main__':
    main()
