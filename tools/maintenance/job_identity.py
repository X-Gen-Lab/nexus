#!/usr/bin/env python3
"""Bind each CI upload to observed checkout, inputs, tools and artifact bytes."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import re
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import (
    EvidenceError, atomic_json, file_identity, identifier, load_json, regular_file,
    stream_digest, verify_file_identity,
)
from tools.evidence.identity import (
    canonical_digest, observe_git_source, resolved_identity,
    resolved_input_files,
)
from tools.evidence.reproduce import run_logged
from tools.configure import metadata


def _contained(root: Path, path: Path) -> Path:
    path = path.absolute()
    if ('..' in path.parts or not path.is_relative_to(root) or
            '.git' in path.relative_to(root).parts):
        raise EvidenceError('artifact must remain inside the checkout')
    if any(part.is_symlink() for part in (path, *path.parents)):
        raise EvidenceError('artifact paths must not traverse a symlink')
    return path


def _artifact(root: Path, path: Path) -> dict:
    # Empty stderr is legitimate observation data, not an executed test report.
    path = regular_file(_contained(root, path), nonempty=False)
    with path.open('rb') as stream:
        checksum = stream_digest(stream)
    return {'relative_path': path.relative_to(root).as_posix(),
            'sha256': checksum, 'size': path.stat().st_size}


def _configuration(path: Path) -> dict:
    resolved, identity = resolved_identity(path)
    if path.name == 'resolved-cpu.json':
        navigation = load_json(path.parent / 'input_paths.json')
        expected = {'cpu_facts', 'platform_metadata'}
        if isinstance(navigation, dict) and 'assembly' in navigation:
            expected.add('assembly')
        if not isinstance(navigation, dict) or set(navigation) != expected:
            raise EvidenceError('CPU configuration input navigation mismatch')
        checksum, inputs = hashlib.sha256(), []
        labels = [('cpu_facts', b'cpu-facts'),
                  ('platform_metadata', b'platform-metadata')]
        if 'assembly' in navigation:
            labels.insert(0, ('assembly', b'assembly'))
        for key, label in labels:
            source = regular_file(navigation[key])
            observed = file_identity(source)
            checksum.update(label + b'\0' +
                            observed['size'].to_bytes(8, 'big'))
            with source.open('rb') as stream:
                while chunk := stream.read(65536):
                    checksum.update(chunk)
            verify_file_identity(observed)
            inputs.append({'label': key, 'file': observed})
        if checksum.hexdigest() != resolved.get('input_sha256'):
            raise EvidenceError('CPU configuration input changed')
        if metadata.policy_contract(load_json(navigation['platform_metadata'])) != \
                resolved.get('platform_metadata'):
            raise EvidenceError('CPU configuration metadata facts changed')
    else:
        inputs = resolved_input_files(path, resolved)
    return {'file': identity, 'validation': 'valid',
            'configuration_sha256': resolved['configuration_sha256'],
            'inputs': inputs}


def _inventory(root: Path, artifacts: list[Path], output: Path) -> tuple:
    selected, missing = {}, []
    for requested in artifacts:
        requested = _contained(root, requested)
        if requested.is_relative_to(output):
            raise EvidenceError('recursive artifact selection')
        if not requested.exists():
            missing.append(requested.relative_to(root).as_posix())
            continue
        paths = requested.rglob('*') if requested.is_dir() else (requested,)
        for path in sorted(paths):
            _contained(root, path)
            # Own version logs are bound in tools; the manifest binds itself.
            if path.is_relative_to(output) or not path.is_file():
                continue
            if path in selected:
                raise EvidenceError('duplicate artifact selection')
            selected[path] = _artifact(root, path)
            if len(selected) > 100000:
                raise EvidenceError('artifact inventory exceeds its bound')
    return selected, missing


def capture(*, root: Path, job: str, result: str, expected_commit: str,
            artifacts: list[Path], tools: dict[str, str], output: Path,
            require_dependencies: bool = True) -> dict:
    root = root.resolve()
    identifier(job, 'CI job')
    if result not in {'success', 'failure', 'cancelled'}:
        raise EvidenceError('unknown actual CI job result')
    if not re.fullmatch(r'(?:[0-9a-f]{40}|[0-9a-f]{64})', expected_commit):
        raise EvidenceError('exact checkout commit required')
    source = observe_git_source(root)
    if source['commit'] != expected_commit:
        raise EvidenceError('event SHA differs from actual checkout')
    complete = not any(line.startswith(('-', '+', 'U'))
                       for line in source['submodules'])
    if result == 'success' and source['dirty']:
        raise EvidenceError('dirty checkout cannot seal a successful CI job')
    if result == 'success' and require_dependencies and not complete:
        raise EvidenceError('source dependencies are missing or changed')
    output = _contained(root, output)
    if output.exists():
        raise EvidenceError('CI identity requires a fresh output directory')
    if not isinstance(artifacts, list) or not 0 < len(artifacts) <= 128:
        raise EvidenceError('explicit bounded artifact selections required')
    selected, missing = _inventory(root, artifacts, output)
    if result == 'success' and (missing or not selected):
        raise EvidenceError('successful CI job has missing or empty artifacts')
    if not isinstance(tools, dict) or not 0 < len(tools) <= 16:
        raise EvidenceError('explicit bounded tool identities required')
    output.mkdir(parents=True)
    observed_tools = []
    for name, executable in sorted(tools.items()):
        identifier(name, 'tool name')
        located = shutil.which(executable)
        if located is None:
            if result == 'success':
                raise EvidenceError('required tool is unavailable: ' + name)
            observed_tools.append({'name': name, 'available': False})
            continue
        binary = Path(located).resolve()
        binary_identity = file_identity(binary)
        execution = run_logged([str(binary), '--version'],
                               output / (name + '-version.log'),
                               timeout_s=30, cwd=root)
        verify_file_identity(binary_identity)
        version = Path(execution['raw']['path']).read_text(errors='replace')
        if result == 'success' and (execution['exit_code'] or
                                   not version.strip()):
            raise EvidenceError('required tool version execution failed: ' + name)
        observed_tools.append({'name': name, 'available': True,
                               'binary': binary_identity,
                               'version': version[:16384],
                               'execution': execution})
    configurations = []
    for path in selected:
        if path.name not in {'resolved.json', 'resolved-cpu.json'}:
            continue
        try:
            configuration = _configuration(path)
        except (EvidenceError, OSError, ValueError, TypeError, KeyError) as error:
            if result == 'success':
                raise EvidenceError('configuration validation failed: ' +
                                    str(error)) from error
            configuration = {'artifact': selected[path],
                             'validation': 'invalid', 'error': str(error)}
        configurations.append(configuration)
    if observe_git_source(root) != source:
        raise EvidenceError('checkout changed during CI identity capture')
    if _inventory(root, artifacts, output) != (selected, missing):
        raise EvidenceError('artifact changed during CI identity capture: inventory')
    version_logs = {Path(tool['execution']['raw']['path']) for tool in
                    observed_tools if tool.get('available')}
    if set(output.iterdir()) != version_logs:
        raise EvidenceError('identity output changed during CI capture')
    for configuration in configurations:
        for item in configuration.get('inputs', []):
            verify_file_identity(item['file'])
    locks = sorted((root / 'dependencies').glob('*.lock.json'))
    if (root / 'nexus.lock').is_file():
        locks.append(root / 'nexus.lock')
    record = {
        'schema_version': 1, 'kind': 'ci_job_identity', 'job': job,
        'job_result': result, 'source': source,
        'dependencies_complete': complete,
        'dependencies_required': require_dependencies,
        'locks': [file_identity(path) for path in locks],
        'tools': observed_tools, 'configurations': configurations,
        'artifacts': list(selected.values()), 'missing_artifacts': missing,
        'identity_output': output.relative_to(root).as_posix(),
        'recorded_utc': datetime.now(timezone.utc).isoformat(),
        'run': {name: os.environ[name] for name in
                ('GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT', 'GITHUB_WORKFLOW',
                 'GITHUB_JOB') if name in os.environ},
        'qualification': 'not_a_software_candidate',
    }
    record['record_sha256'] = canonical_digest(record)
    atomic_json(output / 'identity.json', record)
    return record


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--job', required=True)
    parser.add_argument('--result', required=True)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--artifact', action='append', type=Path, required=True)
    parser.add_argument('--tool', action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--observe-uninitialized-dependencies',
                        action='store_true')
    args = parser.parse_args(arguments)
    try:
        tools = {}
        for item in args.tool:
            name, separator, executable = item.partition('=')
            if not separator or not executable or name in tools:
                raise EvidenceError('unique name=executable tools required')
            tools[name] = executable
        capture(root=args.root, job=args.job, result=args.result,
                expected_commit=args.expected_commit,
                artifacts=args.artifact, tools=tools, output=args.output,
                require_dependencies=not args.observe_uninitialized_dependencies)
        return 0
    except (EvidenceError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
