#!/usr/bin/env python3
"""Verify retained remote qualification and the failed SDK attempt separately."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import tarfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_archive(path, expected):
    seen, json_records = set(), {}
    with tarfile.open(path, 'r:gz') as archive:
        for member in archive:
            name = PurePosixPath(member.name)
            if (not member.isfile() or name.is_absolute() or '..' in name.parts
                    or member.name in seen or member.name not in expected):
                raise ValueError('Unsafe, duplicate or undeclared packed member')
            data = archive.extractfile(member).read()
            row = expected[member.name]
            if len(data) != row['bytes'] or sha(data) != row['sha256']:
                raise ValueError('Packed evidence byte identity differs')
            seen.add(member.name)
            if member.name.endswith('.json'):
                json_records[member.name] = json.loads(data)
    if seen != set(expected):
        raise ValueError('Packed evidence member omitted')
    return json_records, len(seen)


def successful_run(records, run_path, jobs_path, source, count):
    run, jobs = records[run_path], records[jobs_path]
    if (run['head_sha'] != source or run['status'] != 'completed' or
            run['conclusion'] != 'success' or jobs['total_count'] != count or
            len(jobs['jobs']) != count or any(
                job['status'] != 'completed' or job['conclusion'] != 'success'
                for job in jobs['jobs'])):
        raise ValueError('Actual remote run/job admission differs')


def main():
    root = Path(__file__).resolve().parent
    retention = json.loads((root / 'qualification-retention.json').read_text())
    source = retention['source_commit']
    for row in retention['files']:
        path = root / row['path']
        if not path.is_file() or path.is_symlink():
            raise ValueError('Retained file missing or symbolic')
        data = path.read_bytes()
        if len(data) != row['bytes'] or sha(data) != row['sha256']:
            raise ValueError('Retained standalone byte identity differs')
    allowlist = json.loads((root / 'qualification-allowlist.json').read_text())
    records, count = read_archive(root / 'qualification-evidence.tar.gz',
        {row['path']: row for row in allowlist['archive_members']})
    successful_run(records, 'pr-monitor/main58297516/38682470932-run.json',
        'pr-monitor/main58297516/38682470932-jobs.json', source, 29)
    successful_run(records, 'pr-monitor/main58297516/38682470861-run.json',
        'pr-monitor/main58297516/38682470861-jobs.json', source, 4)
    pages = records['pages-publication/final-verification.json']
    active = records['pages-publication/public-active-manifest.json']
    if (pages['status'] != 'published_and_http_verified' or
            pages['source_commit'] != source or
            active['documentation_source']['commit'] != source or
            active['code_source']['commit'] != source):
        raise ValueError('Actual Pages source or result differs')
    failed = records['release-monitor/38684283007-run.json']
    failure_identity = records['release-monitor/failed-attestation-artifact/job-identities/source-sdk-attestation/identity.json']
    if (failed['head_sha'] != source or failed['conclusion'] != 'failure' or
            failure_identity['job_result'] != 'failure' or
            failure_identity['artifacts']):
        raise ValueError('Failed SDK publication scope differs')
    example_counts = []
    for stem in ('examples-pr3-ci-evidence', 'examples-main-ci-evidence'):
        manifest = json.loads((root / (stem + '-manifest.json')).read_text())
        if (manifest['run_conclusion'] != 'success' or
                manifest['job_success_count'] != 11 or
                manifest['failed_skipped_cancelled_jobs']):
            raise ValueError('Examples actual remote qualification differs')
        _, members = read_archive(root / (stem + '.tar.gz'),
            {row['path']: row for row in manifest['members']})
        if members != manifest['member_count']:
            raise ValueError('Examples manifest closure differs')
        example_counts.append(members)
    print(json.dumps({'status': 'passed', 'source_commit': source,
        'qualification_members': count, 'examples_archive_members': example_counts,
        'observed_main_ci_jobs': 29, 'observed_security_jobs': 4,
        'observed_examples_pr_and_main_jobs': [11, 11],
        'observed_pages': 'published_and_http_verified',
        'sdk_attempt_38684283007': 'failed_without_attestation_or_release',
        'physical_hil': 'not_executed'}))


if __name__ == '__main__':
    main()
