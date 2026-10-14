#!/usr/bin/env python3
"""Seal already executed documentation evidence; never run builds or tests."""
from pathlib import Path
import collections
import datetime
import gzip
import hashlib
import io
import json
import re
import shutil
import subprocess
import tarfile

SOURCE = Path('/workspace/nexus-build-system-20261012')
POLISH = Path('/tmp/nexus-docs-polish-20261014')
DELIVERY = Path('/workspace/nexus-docs-polish-delivery-20261014')
ASSETS = Path('/tmp/nexus-documentation-assets-20261014')
PREFIX = ASSETS / 'docs-polish-20261014-51e93508'
COMMIT = '51e93508f22fc1eb540c4ce4dd8713a91cc429f6'
MAINTENANCE_COMMIT = '78c3bc90b9df861906b423c5dab8d326c010491c'
MAINTENANCE_TREE = '790162580804f9109558602e3d3088e5e60fd832'
ARCHIVE_SHA = '928a1d3f78a2568f955f764b3fa1d1b3ee7e861c7985ea1a31985d0c72da8727'
MANIFEST_SHA = '38979748d90b08861eba98f4577192768edf64eebee150f21e133848da38b990'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')

def git(*args):
    return subprocess.check_output(['git', '-C', str(SOURCE), *args], text=True).strip()

def add(selected, path, member):
    if not path.is_file() or path.is_symlink():
        raise RuntimeError(f'Not a regular evidence input: {path}')
    if member in selected and selected[member] != path:
        raise RuntimeError(f'Duplicate member: {member}')
    selected[member] = path

def add_source(selected, path):
    add(selected, path, 'source/' + str(path.relative_to(SOURCE)))

def add_attempt(selected, receipt):
    receipt = Path(receipt)
    for path in sorted(receipt.parent.iterdir()):
        if path.is_file() and path.suffix in {'.json', '.txt', '.log'}:
            if path.is_relative_to(SOURCE):
                add_source(selected, path)
            elif path.is_relative_to(DELIVERY):
                add(selected, path, 'delivery/' + str(path.relative_to(DELIVERY)))
            elif path.is_relative_to(POLISH):
                add(selected, path, 'polish/' + str(path.relative_to(POLISH)))
            else:
                add(selected, path, 'external-attempts/' + str(path).lstrip('/'))

def main():
    for name in ['prepared-publication.json', 'post-deploy-final-51e93508.json', 'ci-final.json', 'ci-final-followup.json',
                 'ci-final-followup.raw.log', 'ci-budget-followup-source.json', 'ci-budget-followup-hook-tdd.tar.gz',
                 'final-publication.json', 'independent-package-review-51e93508.json']:
        if not (DELIVERY / name).is_file():
            raise RuntimeError(f'Wait for parent final receipt: {name}')
    maintenance_head = git('rev-parse', 'HEAD')
    if maintenance_head != MAINTENANCE_COMMIT or git('rev-parse', 'HEAD^{tree}') != MAINTENANCE_TREE:
        raise RuntimeError('Maintenance workspace differs from the authorized exact followup source')
    if git('status', '--porcelain'):
        raise RuntimeError('Maintenance retention workspace is not clean')
    maintenance_delta = []
    if maintenance_head != COMMIT:
        maintenance_delta = git('diff', '--name-only', COMMIT, maintenance_head).splitlines()
        if sorted(maintenance_delta) != ['.github/workflows/build-matrix.yml', '.github/workflows/enterprise-tools.yml']:
            raise RuntimeError('Maintenance followup exceeded the authorized two-workflow scope')
        delta = git('diff', '--unified=0', COMMIT, maintenance_head)
        removed = [line[1:].strip() for line in delta.splitlines() if line.startswith('-') and not line.startswith('---')]
        added = [line[1:].strip() for line in delta.splitlines() if line.startswith('+') and not line.startswith('+++')]
        added_behavior = [line for line in added if not line.startswith('#')]
        added_comments = [line for line in added if line.startswith('#')]
        if (removed != ['timeout-minutes: 30'] * 2 or added_behavior != ['timeout-minutes: 60'] * 2
                or added_comments != ['# Allow fresh sanitizer builds, the complete test plan and evidence uploads.',
                                      '# Allow real SDK builds, the complete test plan and evidence uploads.']):
            raise RuntimeError('Maintenance followup changed more than two 30-to-60 job budgets')
    ci_followup = json.loads((DELIVERY / 'ci-final-followup.json').read_text())
    if ci_followup.get('status') != 'completed' or ci_followup.get('conclusion') != 'success':
        raise RuntimeError('Maintenance followup CI has not actually passed')
    if ci_followup.get('headSha') != maintenance_head:
        raise RuntimeError('Maintenance followup CI source differs from exact clean HEAD')
    if PREFIX.exists():
        raise RuntimeError(f'Refuse to overwrite an existing retention prefix: {PREFIX}')
    package_result = json.loads((DELIVERY / 'package-result.json').read_text())
    package = Path(package_result['directory'])
    offline = Path(package_result['archive'])
    if sha(offline.read_bytes()) != ARCHIVE_SHA:
        raise RuntimeError('Offline archive digest differs from accepted bytes')
    if sha((package / 'documentation-manifest.json').read_bytes()) != MANIFEST_SHA:
        raise RuntimeError('Offline manifest digest differs from accepted bytes')

    selected = {}
    quality_prefixes = ('docs-polish', 'docs-api-layout', 'api-layout',
                        'docs-native', 'native', 'docs-browser')
    for path in sorted((SOURCE / 'build/quality').rglob('*')):
        if path.is_file() and path.relative_to(SOURCE / 'build/quality').parts[0].startswith(quality_prefixes):
            add_source(selected, path)
    followup_tdd_root = 'maintenance-followup/' + maintenance_head + '/build/native-debug/tdd/'
    for path in sorted((SOURCE / 'build/native-debug/tdd').rglob('*')):
        if path.is_file():
            if path.suffix not in {'.json', '.txt', '.xml', '.log'}:
                raise RuntimeError(f'Unexpected TDD non-evidence file: {path}')
            add(selected, path, followup_tdd_root + str(path.relative_to(SOURCE / 'build/native-debug/tdd')))
    freeze_receipt = Path('/tmp/nexus-docs-polish-source51-tdd-freeze.json')
    add(selected, freeze_receipt, 'documentation-source51-tdd-freeze.json')
    freeze = json.loads(freeze_receipt.read_text())
    frozen_tdd = Path(freeze['frozen_copy'])
    if sha(frozen_tdd.read_bytes()) != freeze['archive_sha256']:
        raise RuntimeError('Frozen source51 TDD archive changed')
    if sha((DELIVERY / 'final-hook-tdd.tar.gz').read_bytes()) != freeze['archive_sha256']:
        raise RuntimeError('Original final source51 TDD snapshot was overwritten')
    for rel in ['gates', 'diagrams', 'translations/reviews']:
        for path in sorted((POLISH / rel).rglob('*')):
            if path.is_file():
                add(selected, path, 'polish/' + str(path.relative_to(POLISH)))
    for path in sorted((POLISH / 'search').iterdir()):
        if path.is_file():
            add(selected, path, 'polish/' + str(path.relative_to(POLISH)))
        elif path.name.endswith('.attempts'):
            for child in sorted(path.rglob('*')):
                if child.is_file():
                    add(selected, child, 'polish/' + str(child.relative_to(POLISH)))
    for rel in ['final-editorial-review.json', 'relocated-historical-cache/relocation.json']:
        add(selected, POLISH / rel, 'polish/' + rel)
    for path in sorted(DELIVERY.iterdir()):
        if path.is_file() and path.suffix in {'.json', '.log', '.txt', '.gz'}:
            add(selected, path, 'delivery/' + path.name)
    # Preserve distinct older scopes without retaining their extracted sites again.
    for result in sorted(DELIVERY.glob('*package-result.json')):
        value = json.loads(result.read_text())
        old_package = Path(value['directory'])
        for name in ['documentation-manifest.json', 'SHA256SUMS',
                     'documentation-build.json', 'documentation-aliases.json']:
            path = old_package / name
            add(selected, path, 'delivery/package-metadata/' + old_package.name + '/' + name)
    for build_name in ['a73568e-build.json', '6f395-build.json', 'final-build.json']:
        build = json.loads((DELIVERY / build_name).read_text())
        for receipt in build['command_receipts']:
            add_attempt(selected, receipt)
    review = json.loads((DELIVERY / 'independent-package-review-51e93508.json').read_text())
    current_catalog_messages = sum(record['messages'] for record in review['catalog_records'])
    if current_catalog_messages != review['catalog_messages']:
        raise RuntimeError('Independent final package catalog counts disagree')
    if review.get('gettext_receipt'):
        receipt = review['gettext_receipt']
        add_attempt(selected, receipt['path'] if isinstance(receipt, dict) else receipt)
    installation = Path(json.loads((DELIVERY / 'final-build.json').read_text())['tools']['installation_receipt'])
    add(selected, installation, 'tool-identity/installation.json')

    PREFIX.mkdir()
    shutil.copyfile(offline, PREFIX / offline.name)
    for name in ['documentation-manifest.json', 'SHA256SUMS']:
        shutil.copyfile(package / name, PREFIX / name)
    source_snapshot = {
        'kind': 'retention_source_snapshot', 'captured_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'source_root': str(SOURCE), 'commit': maintenance_head,
        'documentation_package_source_commit': COMMIT, 'maintenance_source_commit': maintenance_head,
        'maintenance_delta_from_documentation_commit': maintenance_delta,
        'maintenance_delta_admission': 'Only native/tooling job timeout-minutes 30 to 60 and two explanatory comments; no test, matrix, command, child timeout or skip changes.',
        'tree': git('rev-parse', 'HEAD^{tree}'), 'porcelain': git('status', '--porcelain'),
        'submodules': git('submodule', 'status', '--recursive').splitlines(),
        'new_firmware_qualification': False, 'physical_hil': 'not executed',
        'qualified_firmware_baseline': 'cc6a065c002041234a212a964be863a69198f9b4',
    }
    write_json(PREFIX / 'source-snapshot.json', source_snapshot)
    members = []
    evidence_archive = PREFIX / 'raw-evidence.tar.gz'
    with evidence_archive.open('wb') as sink:
        with gzip.GzipFile(filename='', mode='wb', fileobj=sink, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as archive:
                for member, origin in sorted(selected.items()):
                    before = origin.stat()
                    data = origin.read_bytes()
                    after = origin.stat()
                    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                        raise RuntimeError(f'Evidence changed during sealing: {origin}')
                    info = tarfile.TarInfo(member)
                    info.size = len(data); info.mode = 0o644; info.mtime = 0
                    archive.addfile(info, io.BytesIO(data))
                    members.append({'path': member, 'origin_path': str(origin),
                                    'bytes': len(data), 'sha256': sha(data)})
                # The original final51 snapshot is primary. Current TDD files
                # may now describe the separately authorized maintenance hook.
                with tarfile.open(frozen_tdd, 'r:gz') as snapshot:
                    for item in snapshot:
                        if item.isdir():
                            continue
                        if not item.isfile() or not item.name.startswith('tdd/'):
                            raise RuntimeError('Invalid frozen TDD member')
                        data = snapshot.extractfile(item).read()
                        member = 'documentation-source51/build/native-debug/' + item.name
                        info = tarfile.TarInfo(member)
                        info.size = len(data); info.mode = 0o644; info.mtime = 0
                        archive.addfile(info, io.BytesIO(data))
                        members.append({'path': member, 'origin_path': str(frozen_tdd),
                                        'origin_archive_member': item.name,
                                        'bytes': len(data), 'sha256': sha(data)})
    evidence_bytes = evidence_archive.read_bytes()
    index = {
        'schema_version': 1, 'kind': 'nexus_documentation_polish_raw_evidence_index',
        'documentation_source_commit': COMMIT,
        'maintenance_source_commit': maintenance_head,
        'tdd_scopes': {'documentation-source51': 'Frozen original final source51 commit-gate execution',
                       'maintenance-followup/' + maintenance_head: 'Current raw TDD after the separate two-workflow maintenance hook; exact clean maintenance source tree recorded in source-snapshot.json'},
        'tdd_accumulated_attempts': 'Both snapshots preserve accumulated older attempt directories. Only their latest direct command/execution receipts describe the final gate in that snapshot; older attempts retain original timestamps and are not reassigned to the snapshot HEAD.',
        'qualified_firmware_baseline': source_snapshot['qualified_firmware_baseline'],
        'physical_hil': 'not executed', 'new_firmware_qualification': False,
        'archive': {'path': evidence_archive.name, 'bytes': len(evidence_bytes), 'sha256': sha(evidence_bytes)},
        'member_count': len(members), 'member_bytes': sum(member['bytes'] for member in members),
        'path_roots': {'source': str(SOURCE), 'polish': str(POLISH), 'delivery': str(DELIVERY)},
        'members': members,
    }
    write_json(PREFIX / 'raw-evidence-index.json', index)
    nested_archives = []
    for member, origin in sorted(selected.items()):
        if origin.name.endswith('.tar.gz'):
            nested_members = []
            with tarfile.open(origin, 'r:gz') as nested:
                for item in nested:
                    if item.isdir():
                        continue
                    if not item.isfile():
                        raise RuntimeError(f'Nonregular nested evidence member: {origin}:{item.name}')
                    data = nested.extractfile(item).read()
                    nested_members.append({'path': item.name, 'bytes': len(data), 'sha256': sha(data)})
            nested_archives.append({'outer_member_path': member, 'origin_path': str(origin),
                                    'archive_sha256': sha(origin.read_bytes()),
                                    'member_count': len(nested_members), 'members': nested_members})
    write_json(PREFIX / 'nested-evidence-index.json', {
        'kind': 'nested_raw_evidence_member_index',
        'scope': 'Original source-specific TDD snapshots; current raw TDD is also directly indexed in raw-evidence-index.json. Directory entries contain no raw bytes.',
        'archives': nested_archives,
    })
    excluded = {
        'kind': 'nexus_documentation_retention_exclusions',
        'excluded': [
            {'scope': 'search/*fixture/** and search/__pycache__/**', 'reason': 'Generated fixture sites, fixture Git internals, doctrees and Python caches; raw browser/probe/build receipts and their command output are retained.'},
            {'scope': 'delivery/packages/*/** payload directories and delivery/independent-restore*/**', 'reason': 'Final immutable offline archive retains current payload once; exact per-package manifests and metadata retain source scope; generated extraction copies are not needed.'},
            {'scope': 'superseded docs-20261014-a73568e172ab.tar.gz and docs-20261014-6f395bfe228a.tar.gz', 'reason': 'Rejected/superseded package receipts and manifests are retained; only the accepted final offline package is published in this prefix.'},
            {'scope': 'polish/relocated-historical-cache/** except relocation.json', 'reason': 'Retired caches are not current qualification evidence; relocation record is retained.'},
            {'scope': 'build binaries, object files, generated Doxygen XML, gettext trees and doctrees', 'reason': 'Durable source, exact accepted offline package, raw command outputs, JUnit and execution receipts establish the retained documentation/host scope.'},
        ],
    }
    write_json(PREFIX / 'exclusions.json', excluded)
    ci = json.loads((DELIVERY / 'ci-final.json').read_text())
    docs_jobs = [{'name': j['name'], 'status': j['status'], 'conclusion': j.get('conclusion', ''),
                  'url': j.get('url')} for j in ci['jobs'] if 'doc' in j['name'].lower()]
    write_json(PREFIX / 'acceptance-scopes.json', {
        'kind': 'documentation_evidence_scope_notes', 'documentation_source_commit': COMMIT,
        'maintenance_source_commit': maintenance_head,
        'latest_clean_receipts': ['delivery/final-build.json', 'delivery/package-result.json',
                                  'delivery/independent-package-review-51e93508.json',
                                  'source/build/quality/docs-browser-final-51e93508.json'],
        'canonical_pages': 102, 'language_page_states': 204,
        'catalogs': len(review['catalog_records']), 'catalog_messages': current_catalog_messages,
        'documentation_suite_latest': {'executed': 305, 'skipped': 0, 'failures': 0, 'errors': 0,
                                       'receipt': 'source/build/quality/docs-polish-search-tests.json'},
        'superseded_documentation_suite_counts': {'docs-polish-tests.json': 285, 'docs-polish-fixed-tests.json': 295},
        'native_host_suite': {'passed': 3252, 'suites': 177, 'receipt': 'documentation-source51/build/native-debug/tdd/execution.json',
                              'scope': 'Actual host execution with raw commit-gate logs and command receipts; not proof of historical TDD order or physical hardware qualification.'},
        'maintenance_followup_tdd_receipt': followup_tdd_root + 'execution.json',
        'tools_suite': {'passed': 1555, 'testing': 678, 'configure': 481, 'ci': 369, 'workflows': 27},
        'latest_browser': {'page_loads': 408, 'representative_viewports': 30, 'actual_searches': 8,
                           'transport': 'loopback HTTP; direct file access not executed',
                           'equivalent200': '720 CSS px viewport at device scale factor 2; no browser UI zoom claimed'},
        'editorial_review_scope': 'polish/final-editorial-review.json records prior 5a7a75c working-tree source hashes; final clean-source build/browser/package receipts are separate.',
        'superseded_receipts': 'Older a73568e and 6f395 build/package/browser/native-query/CI receipts, failed previews, script assumption failures and actual RED execution are preserved with their original values and source identities.',
        'offline_package': {'payload_files': 1675, 'total_files': 1677, 'archive_sha256': ARCHIVE_SHA,
                            'manifest_sha256': MANIFEST_SHA},
        'ci_original_run': 38608480244,
        'ci_original_source': COMMIT,
        'ci_attempt1_outcome': 'Native ASan and Tool Contracts cancelled at 30-minute job limits; CI Status failed. Original JSON/raw log are retained. Timings do not establish absence of a hang.',
        'ci_capture_run_ids': sorted({int(match.group(1)) for job in ci['jobs'] if (match := re.search(r'/actions/runs/(\d+)', job.get('url', '')))}),
        'ci_capture': {'status': ci['status'], 'conclusion': ci.get('conclusion', ''), 'documentation_jobs': docs_jobs},
        'ci_followup_capture': {'status': ci_followup['status'], 'conclusion': ci_followup['conclusion'],
                                'headSha': ci_followup['headSha'],
                                'run_ids': sorted({int(match.group(1)) for job in ci_followup['jobs'] if (match := re.search(r'/actions/runs/(\d+)', job.get('url', '')))}),
                                'receipt': 'delivery/ci-final-followup.json',
                                'scope': 'Separate maintenance source with only two job-budget changes. The accepted offline package and Pages publication remain documentation source51.'},
        'new_firmware_qualification': False, 'physical_hil': 'not executed',
    })
    print(json.dumps({'prefix': str(PREFIX), 'members': len(members), 'evidence_bytes': len(evidence_bytes),
                      'evidence_sha256': sha(evidence_bytes), 'ci_status': ci['status']}))

if __name__ == '__main__':
    main()
