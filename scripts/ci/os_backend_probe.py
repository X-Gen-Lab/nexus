"""Execute backend onboarding probes without registering or claiming support.

The reviewed CMake graph owns targets. This gate rebuilds nominated GoogleTest
executables, discovers every case and executes fresh, unfiltered reports. MCU
candidates additionally rebuild a nominated firmware target and bind its actual
ELF to the production CPU resolution. Passing still requires maintainer review.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

import arch_compile
from os_wait_cost import prepare_output
from scripts.ci.tdd_gate import environment, validate_google_report
from tools.configure import cpu as cpu_contract

CASE = re.compile(r'[A-Za-z0-9_/]+\.[A-Za-z0-9_/]+')
COMMON_REQUIREMENTS = frozenset({
    'absolute_deadline', 'never', 'wake_races', 'single_waiter',
    'static_storage', 'storage_lifecycle',
})


def fields(value: dict, expected: set, where: str) -> None:
    """Unknown policy cannot silently obtain the closest available backend."""
    if not isinstance(value, dict) or value.keys() != expected:
        raise ValueError('missing or unknown ' + where + ' fields')


def validate_contract(value: dict) -> dict:
    """Validate policy and exact required cases, never implementation quality."""
    fields(value, {'schema_version', 'backend', 'contract', 'requirements',
                   'kernel'}, 'root')
    if type(value['schema_version']) is not int or value['schema_version'] != 1:
        raise ValueError('unsupported backend contract schema')
    backend = value['backend']
    fields(backend, {'name', 'kind', 'claim_support'}, 'backend')
    if (not isinstance(backend['name'], str) or
            not re.fullmatch(r'[a-z][a-z0-9_-]{0,63}', backend['name']) or
            backend['kind'] not in ('host', 'baremetal', 'kernel') or
            backend['claim_support'] is not False):
        raise ValueError('invalid backend identity or unsupported promotion')
    kind = backend['kind']
    policy = value['contract']
    fixed = {
        'deadline': 'absolute_monotonic_us', 'never': 'NX_DEADLINE_NEVER',
        'overflow': 'saturating', 'clock_continues_while_waiting': True,
        'hint_is_result': False, 'storage': 'caller_owned',
        'lifecycle_owner': 'single',
        'notification_reclaim': 'publishers_and_waiter_quiesced',
        'hidden_workers': False,
    }
    fields(policy, set(fixed) | {'task_reclaim', 'queue_shutdown', 'irq_policy',
                                'mask_policy', 'incoming_masks',
                                'host_library_allocations'}, 'contract')
    if any(type(policy[key]) is not type(expected) or
           policy[key] != expected for key, expected in fixed.items()):
        raise ValueError('unsupported time, ownership or hidden-work policy')
    host = kind == 'host'
    expected = {
        'task_reclaim': ('not_applicable' if kind == 'baremetal'
                         else 'successful_join'),
        'queue_shutdown': ('not_applicable' if kind == 'baremetal'
                           else 'close_then_drain'),
        'irq_policy': 'host_thread_only' if host else 'configurable_irq',
        'incoming_masks': 'not_applicable' if host else 'preserved',
        'host_library_allocations': host,
    }
    if any(type(policy[key]) is not type(item) or policy[key] != item
           for key, item in expected.items()):
        raise ValueError('backend context or reclaim policy is inconsistent')
    masks = ('primask', 'basepri') if kind == 'kernel' else ('none',)
    if policy['mask_policy'] not in masks:
        raise ValueError('unsupported IRQ mask policy')
    required = set(COMMON_REQUIREMENTS)
    if kind != 'baremetal':
        required.update(('task_join', 'queue_shutdown'))
    if not host:
        required.add('mask_isr')
    if kind == 'kernel':
        required.add('kernel_integration')
    fields(value['requirements'], required, 'requirement')
    for group, cases in value['requirements'].items():
        if (not isinstance(cases, list) or not cases or
                any(not isinstance(case, str) or not CASE.fullmatch(case) or
                    'DISABLED_' in case for case in cases) or
                len(cases) != len(set(cases))):
            raise ValueError('empty, duplicate or filtered cases: ' + group)
    locked = value['kernel']
    if kind == 'kernel':
        fields(locked, {'lock', 'required_symbols'}, 'kernel')
        path = locked['lock']
        names = locked['required_symbols']
        if (not isinstance(path, str) or not path or
                Path(path).is_absolute() or '..' in Path(path).parts or
                not isinstance(names, list) or not names or
                any(not isinstance(name, str) or
                    not re.fullmatch(r'[A-Za-z_]\w*', name) for name in names) or
                len(names) != len(set(names))):
            raise ValueError('kernel source lock and real API symbols required')
    elif locked is not None:
        raise ValueError('non-kernel backend cannot invent kernel evidence')
    return value


def discover_cases(text: str) -> set[str]:
    """Preserve exact framework identities, including parameter instantiations."""
    suite = None
    cases = []
    for line in text.splitlines():
        item = line.split('#', 1)[0].rstrip()
        if item and not item[0].isspace() and item.endswith('.'):
            if suite is not None and not any(case.startswith(suite + '.')
                                             for case in cases):
                raise ValueError('empty discovered suite')
            suite = item[:-1]
        elif suite is not None and item.startswith('  ') and item.strip():
            case = suite + '.' + item.strip()
            if not CASE.fullmatch(case) or 'DISABLED_' in case:
                raise ValueError('disabled or malformed discovered case')
            cases.append(case)
    if (not cases or len(cases) != len(set(cases)) or
            (suite is not None and
             not any(case.startswith(suite + '.') for case in cases))):
        raise ValueError('empty or duplicate GoogleTest discovery')
    return set(cases)


def verify_coverage(value: dict, cases: set[str]) -> None:
    """Every declared contract requires an exact actual completed case."""
    missing = {name for group in value['requirements'].values()
               for name in group} - cases
    if missing:
        raise ValueError('missing actual contract cases: ' + ', '.join(
            sorted(missing)))


def elf_cost(text: str) -> dict:
    """Actual GNU size row accounts initialized data in both memory domains."""
    lines = text.strip().splitlines()
    if len(lines) != 2 or lines[0].split() != [
            'text', 'data', 'bss', 'dec', 'hex', 'filename']:
        raise ValueError('missing actual GNU size measurement')
    values = lines[1].split(maxsplit=5)
    if len(values) != 6:
        raise ValueError('incomplete ELF size row')
    code, data, bss, total = map(int, values[:4])
    if (code <= 0 or min(data, bss) < 0 or code + data + bss != total or
            int(values[4], 16) != total):
        raise ValueError('empty or inconsistent ELF size measurement')
    return {'flash_bytes': code + data, 'ram_bytes': data + bss,
            'text_bytes': code, 'data_bytes': data, 'bss_bytes': bss}


class Execution:
    """Retain subprocesses with clean test environment and bounded execution."""

    def __init__(self, output: Path):
        self.output = output
        self.commands = []
        self.env = environment()

    def execute(self, argv: list[str], label: str) -> str:
        command = {'argv': argv, 'cwd': str(ROOT), 'returncode': None}
        self.commands.append(command)
        prefix = self.output / f'{len(self.commands):03d}-{label}'
        print('+ ' + ' '.join(argv), flush=True)
        try:
            result = subprocess.run(argv, cwd=ROOT, env=self.env,
                                    capture_output=True, timeout=60,
                                    check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            command['error'] = str(error)
            raise ValueError('actual command could not execute') from error
        command['returncode'] = result.returncode
        for stream in ('stdout', 'stderr'):
            path = prefix.with_suffix('.' + stream)
            path.write_bytes(getattr(result, stream))
            command[stream] = arch_compile.digest(path)
        if result.returncode:
            raise ValueError(f'command failed ({result.returncode}): {argv[0]}')
        return result.stdout.decode('utf-8', errors='strict')


def actual_build(directory: Path) -> Path:
    """Reject alternate source graphs and builds outside the owned workspace."""
    if directory.is_symlink():
        raise ValueError('build must not be a symlink')
    build = directory.resolve()
    if not build.is_relative_to(ROOT / 'build') or build == ROOT / 'build':
        raise ValueError('build must be an existing checkout build directory')
    cache = (build / 'CMakeCache.txt').read_text()
    homes = re.findall(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$', cache,
                       flags=re.MULTILINE)
    if homes != [str(ROOT)]:
        raise ValueError('backend target must use the production CMake graph')
    return build


def planned_binaries(build: Path, targets: list[str]) -> list[Path]:
    """The generated registration owns names; user paths cannot replace targets."""
    if (not targets or len(targets) != len(set(targets)) or
            any(not re.fullmatch(r'[A-Za-z0-9_]+', name) for name in targets)):
        raise ValueError('unique exact GoogleTest target names required')
    manifest = json.loads((build / 'google-contracts.json').read_text())
    fields(manifest, {'schema', 'executables'}, 'Google registration')
    names = manifest['executables']
    if (type(manifest['schema']) is not int or manifest['schema'] != 1 or
            not isinstance(names, list) or not names or
            any(not isinstance(name, str) for name in names) or
            len(names) != len(set(names))):
        raise ValueError('missing or duplicate GoogleTest executable plan')
    candidates = [Path(name).resolve() for name in names]
    if any(not path.is_relative_to(build) for path in candidates):
        raise ValueError('GoogleTest registration escapes its build directory')
    selected = []
    for target in targets:
        matches = [path for path in candidates if path.name == target]
        if len(matches) != 1:
            raise ValueError('target missing or ambiguous in Google registration')
        selected.extend(matches)
    return selected


def source_inputs(contract: Path, builds: list[Path]) -> dict:
    """Bind current source and generated build inputs, not a clean candidate."""
    paths = {Path(__file__), ROOT / 'scripts/ci/arch_compile.py',
             ROOT / 'scripts/ci/os_wait_cost.py', ROOT / 'scripts/ci/tdd_gate.py',
             ROOT / 'scripts/validation/junit.py', ROOT / 'CMakeLists.txt',
             ROOT / 'CMakePresets.json', contract}
    for layer in ('core', 'arch', 'os', 'cmake', 'tools/configure', 'tests/google'):
        paths.update(path for path in (ROOT / layer).rglob('*')
                     if path.is_file() and path.suffix in {
                         '.c', '.h', '.cpp', '.cmake', '.py', '.toml', '.json'})
        paths.update((ROOT / layer).rglob('CMakeLists.txt'))
    for build in builds:
        for name in ('CMakeCache.txt', 'google-contracts.json',
                     'compile_commands.json', 'generated/resolved.json',
                     'generated/resolved-cpu.json'):
            path = build / name
            if path.is_file():
                paths.add(path)
        commands = json.loads((build / 'compile_commands.json').read_text())
        if not isinstance(commands, list) or not commands:
            raise ValueError('actual build compilation database is required')
        for entry in commands:
            path = Path(entry['file'])
            if not path.is_absolute():
                path = Path(entry['directory']) / path
            paths.add(path.resolve())
    return {str(path.resolve()): arch_compile.digest(path)
            for path in sorted(paths)}


def inspect_elf(recorder: Execution, binary: Path, prefix: str,
                compiler: str | None = None) -> dict:
    """Read real ELF format, symbols and sections; markers cannot replace them."""
    if not binary.is_file() or binary.is_symlink():
        raise ValueError('linked executable is missing or is not a regular ELF')
    if compiler is None:
        selected = {name: shutil.which(name) for name in ('size', 'nm', 'readelf')}
        if any(path is None for path in selected.values()):
            raise ValueError('GNU size, nm and readelf are required')
    else:
        selected = arch_compile.tools(compiler)
        selected['size'] = str(Path(selected['gcc']).with_name('arm-none-eabi-size'))
    identities = {}
    for name in ('size', 'nm', 'readelf'):
        path = selected[name]
        version = recorder.execute([path, '--version'], prefix + '-' + name)
        if 'GNU' not in version:
            raise ValueError('actual GNU ELF tools required')
        identities[name] = arch_compile.digest(Path(path))
    header = recorder.execute([selected['readelf'], '-h', str(binary)],
                              prefix + '-header')
    if 'ELF' not in header or (compiler is not None and not (
            re.search(r'Class:\s*ELF32', header) and
            re.search(r'Machine:\s*ARM\s*$', header, re.MULTILINE))):
        raise ValueError('linked executable has the wrong ELF machine')
    names = recorder.execute([selected['nm'], '--defined-only', str(binary)],
                             prefix + '-symbols')
    if not names.strip():
        raise ValueError('stripped ELF cannot establish backend API retention')
    cost = elf_cost(recorder.execute([selected['size'], str(binary)],
                                    prefix + '-size'))
    result = {'elf': arch_compile.digest(binary), 'cost': cost,
              'tools': identities, 'scope': 'host_elf_not_mcu_budget' if
              compiler is None else 'linked_mcu_elf_not_physical_timing'}
    if compiler is not None:
        arch_compile.check_arch_symbols(recorder.execute(
            [selected['nm'], '-u', str(binary)], prefix + '-undefined'))
        symbols = {match.group(1) for match in re.finditer(
            r'^\s*[0-9a-fA-F]+\s+[A-Za-z]\s+(\S+)\s*$', names,
            flags=re.MULTILINE)}
        forbidden = {'malloc', 'calloc', 'realloc', 'free', 'pvPortMalloc',
                     'vPortFree', '__cxa_throw', '__gxx_personality_v0'}
        if symbols & forbidden or any('gtest' in name.lower() for name in symbols):
            raise ValueError('firmware retains heap or host test dependencies')
        result['symbols'] = sorted(symbols)
        result['attributes'] = recorder.execute(
            [selected['readelf'], '-A', str(binary)], prefix + '-attributes')
    return result


def firmware_profile(generated: dict, contract: dict, attributes: str) -> dict:
    """Consume production CPU resolution; never infer a new CPU feature set."""
    profile = generated.get('cpu_profile')
    if not isinstance(profile, dict) or profile.get('arch') == 'native':
        raise ValueError('production MCU CPU resolution is required')
    expected_backend = ('baremetal' if contract['backend']['kind'] == 'baremetal'
                        else contract['backend']['name'])
    if profile.get('backend') != expected_backend:
        raise ValueError('linked backend differs from nominated contract')
    try:
        irq = profile['irq']
        resolved = cpu_contract.resolve(profile['cpu'], {
            'priority_bits': irq['priority_bits'],
            'external_count': irq['external_count'],
        }, expected_backend, enum_abi=profile['enum_abi']).to_dict()
    except (KeyError, RuntimeError, TypeError) as error:
        raise ValueError('unsupported production CPU/backend resolution') from error
    for key in ('arch', 'fpu', 'float_abi', 'enum_abi', 'cpu', 'arch_macro',
                'has_basepri', 'atomic_backend', 'compile_options'):
        if profile.get(key) != resolved[key]:
            raise ValueError('CPU resolution differs from production authority')
    masks = contract['contract']['mask_policy']
    if masks == 'basepri' and profile['has_basepri'] is not True:
        raise ValueError('BASEPRI policy conflicts with resolved CPU capability')
    hard = bool(re.search(r'Tag_ABI_VFP_args:\s*VFP registers', attributes))
    if hard != (profile['float_abi'] == 'hard'):
        raise ValueError('linked floating ABI differs from production resolution')
    if not re.search(r'Tag_ABI_enum_size:\s*small', attributes):
        raise ValueError('linked enum ABI differs from static firmware contract')
    return profile


def verify_compile_profile(profile: dict, commands: list[dict]) -> None:
    """Bind declared ISA/ABI flags to actual owned firmware compile commands."""
    observed = 0
    for entry in commands:
        path = Path(entry['file']).resolve()
        if not any(path.is_relative_to(ROOT / layer)
                   for layer in ('core', 'arch', 'os')) or path.suffix != '.c':
            continue
        arguments = entry.get('arguments')
        if arguments is None:
            arguments = shlex.split(entry['command'])
        if not isinstance(arguments, list) or not all(
                flag in arguments for flag in profile['compile_options']):
            raise ValueError('actual owned firmware compile flags disagree with CPU')
        observed += 1
    if observed == 0:
        raise ValueError('firmware compilation contains no owned platform sources')


def run(args) -> int:
    """Execute nominated targets and produce immutable, scoped admission proof."""
    try:
        output = prepare_output(ROOT, args.output)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2
    recorder = Execution(output)
    report = {'schema_version': 1, 'status': 'failed',
              'scope': 'backend_onboarding_probe', 'support_promoted': False,
              'source_status': 'working_tree_development_probe',
              'registration': 'not_performed', 'maintainer_review_required': True,
              'physical_status': 'not_executed', 'candidate_status': 'not_qualified',
              'commands': recorder.commands, 'probes': []}
    try:
        contract = validate_contract(json.loads(args.contract.read_text()))
        report['contract'] = arch_compile.digest(args.contract)
        report['backend'] = contract['backend']
        build = actual_build(args.build_dir)
        binaries = planned_binaries(build, args.target)
        builds = [build]
        firmware = None
        if contract['backend']['kind'] != 'host':
            if not all((args.firmware_build, args.firmware_target,
                        args.firmware, args.cpu_resolution, args.compiler)):
                raise ValueError('MCU backend needs an actual firmware build, '
                                 'target, ELF, production CPU resolution and tools')
            firmware_build = actual_build(args.firmware_build)
            builds.append(firmware_build)
            firmware = args.firmware.resolve()
            if (not firmware.is_relative_to(firmware_build) or
                    not args.cpu_resolution.resolve().is_relative_to(firmware_build) or
                    not re.fullmatch(r'[A-Za-z0-9_]+', args.firmware_target)):
                raise ValueError('firmware inputs must belong to nominated build')
        elif any((args.firmware_build, args.firmware_target, args.firmware,
                  args.cpu_resolution, args.compiler)):
            raise ValueError('host backend cannot claim firmware evidence')
        cmake = shutil.which('cmake')
        if cmake is None:
            raise ValueError('actual CMake tool required')
        report['cmake'] = arch_compile.digest(Path(cmake))
        recorder.execute([cmake, '--version'], 'cmake-version')
        before = source_inputs(args.contract, builds)
        if contract['kernel'] is not None:
            lock = (ROOT / contract['kernel']['lock']).resolve()
            if not lock.is_relative_to(ROOT) or not lock.is_file():
                raise ValueError('kernel source lock must exist inside source tree')
            before[str(lock)] = arch_compile.digest(lock)
        report['source_inputs'] = before
        recorder.execute([cmake, '--build', str(build), '--parallel', '2',
                          '--target', *args.target, '--verbose'], 'probe-build')
        completed = set()
        for binary in binaries:
            identity = arch_compile.digest(binary)
            discovered = discover_cases(recorder.execute(
                [str(binary), '--gtest_list_tests'], binary.name + '-discovery'))
            if completed & discovered:
                raise ValueError('contract case identity duplicated across targets')
            xml = output / (binary.name + '.xml')
            started = time.time_ns()
            recorder.execute([
                str(binary), '--gtest_filter=*', '--gtest_repeat=1',
                '--gtest_also_run_disabled_tests', '--gtest_output=xml:' + str(xml)],
                binary.name + '-execute')
            evidence = validate_google_report(
                xml, not_before_ns=started, expected_cases=len(discovered))
            actual = {item['classname'] + '.' + item['name']
                      for item in evidence['cases']}
            if actual != discovered or arch_compile.digest(binary) != identity:
                raise ValueError('actual cases or executable changed during probe')
            completed.update(actual)
            cost = inspect_elf(recorder, binary, binary.name)
            report['probes'].append({'target': binary.name, 'execution': evidence,
                                     'linked_cost': cost})
        verify_coverage(contract, completed)
        if firmware is not None:
            recorder.execute([cmake, '--build', str(firmware_build), '--parallel',
                              '2', '--target', args.firmware_target, '--verbose'],
                             'firmware-build')
            measured = inspect_elf(recorder, firmware, 'firmware', args.compiler)
            generated = json.loads(args.cpu_resolution.read_text())
            measured['cpu_profile'] = firmware_profile(
                generated, contract, measured['attributes'])
            verify_compile_profile(measured['cpu_profile'], json.loads(
                (firmware_build / 'compile_commands.json').read_text()))
            measured['cpu_resolution'] = arch_compile.digest(args.cpu_resolution)
            if contract['kernel'] is not None:
                missing = set(contract['kernel']['required_symbols']) - set(
                    measured['symbols'])
                if missing:
                    raise ValueError('real kernel API not retained: ' + str(missing))
            report['firmware'] = measured
        after = source_inputs(args.contract, builds)
        if contract['kernel'] is not None:
            after[str(lock)] = arch_compile.digest(lock)
        report['source_inputs_stable'] = before == after
        if before != after:
            raise ValueError('source or generated inputs changed during execution')
        report['completed_cases'] = sorted(completed)
        report['status'] = 'passed'
    except (OSError, ValueError, KeyError, TypeError) as error:
        report['error'] = str(error)
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'report': str(output / 'report.json'),
                      'error': report.get('error')}))
    return 0 if report['status'] == 'passed' else 1


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--contract', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--target', action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--firmware-build', type=Path)
    parser.add_argument('--firmware-target')
    parser.add_argument('--firmware', type=Path)
    parser.add_argument('--cpu-resolution', type=Path)
    parser.add_argument('--compiler')
    return run(parser.parse_args(argv))


if __name__ == '__main__':
    sys.exit(main())
