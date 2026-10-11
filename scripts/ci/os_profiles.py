"""Qualify authored kernel policies and mandatory hooks with actual ARM links.

The matrix uses the production Runtime graph. Positive links, expected missing
hook rejections and measured resource costs are distinct evidence. No firmware
is executed and no profile establishes physical timing or board qualification.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys

import arch_compile as compiler_gate
import cortex_runtime
from tools.configure import cpu, kernel


def matrix() -> tuple[dict, ...]:
    variants = []

    def add(name, *, arch='cortex-m4', choices=None, rejection=False,
            external=False, counter=False, priority_bits=None, **features):
        if arch == 'cortex-m4':
            features = {'fpu': 'fpv4-sp-d16', 'float_abi': 'hard', **features}
        facts = compiler_gate.reference_facts(arch, **features)
        if priority_bits is not None:
            facts['irq']['priority_bits'] = priority_bits
        policy = {'profile': 'standard', **(choices or {})}
        profile = cpu.resolve(facts['cpu'], facts['irq'], 'freertos')
        selected = kernel.resolve(policy, profile, facts['clock_hz'])
        profile = kernel.bind(profile, selected)
        variants.append({'name': name, 'facts': facts, 'choices': policy,
                         'backend': 'freertos', 'profile': profile.to_dict(),
                         'kernel': selected.to_dict(),
                         'expected_rejection': rejection,
                         'external_provider': external,
                         'counter_provider': counter})

    for profile in ('standard', 'minimal', 'diagnostic', 'lowpower'):
        add(profile, choices={'profile': profile})
    for missing in (False, True):
        add('external-tick' + ('-missing' if missing else ''),
            choices={'tick_source': 'external', 'tick_hz': 333},
            external=not missing, rejection=missing)
        add('runtime-counter' + ('-missing' if missing else ''),
            choices={'runtime_stats': True}, counter=not missing,
            rejection=missing)
    add('m0-minimal', arch='cortex-m0', choices={'profile': 'minimal'})
    add('m23-lowpower', arch='cortex-m23', choices={'profile': 'lowpower'})
    add('m55-integer-mve-diagnostic', arch='cortex-m55', mve='integer',
        float_abi='softfp', choices={'profile': 'diagnostic'})
    add('m85-fp-mve-minimal', arch='cortex-m85', mve='float', fpu='auto',
        float_abi='hard', choices={'profile': 'minimal'})
    add('m7-integer-diagnostic', arch='cortex-m7', choices={'profile': 'diagnostic'})
    add('priority8-explicit-policy', priority_bits=8,
        choices={'max_priorities': 32, 'syscall_priority': 17,
                 'notification_slots': 2, 'idle_stack_words': 256,
                 'max_task_name_len': 32, 'tick_hz': 2000})
    return tuple(variants)


def write_inputs(directory: Path, variant: dict) -> Path:
    assembly = cortex_runtime.write_inputs(directory, variant)
    with assembly.open('a', encoding='utf-8') as stream:
        stream.write('\n[os]\n')
        for key, value in sorted(variant['choices'].items()):
            stream.write(key + ' = ' + json.dumps(value) + '\n')
    return assembly


def check_rejection(variant: dict, returncode: int, diagnostic: str,
                    elf_exists: bool) -> None:
    required = ({'nx_freertos_external_tick_setup'} if
                variant['choices'].get('tick_source') == 'external' else
                {'nx_freertos_runtime_counter_start',
                 'nx_freertos_runtime_counter_now'})
    if (not variant['expected_rejection'] or returncode != 1 or elf_exists or
            any(not re.search(r'undefined reference[^\n]*\b' + symbol + r'\b',
                              diagnostic) for symbol in required)):
        raise ValueError('Missing precise expected hook rejection')


def check_optional_symbols(variant: dict, text: str) -> None:
    names = cortex_runtime.symbols(text)
    policy = variant['kernel']
    if policy['tick_source'] == 'external':
        if (not re.search(r'\bT\s+vPortSetupTimerInterrupt\s*$', text,
                          flags=re.MULTILINE) or
                'nx_freertos_external_tick_setup' not in names):
            raise ValueError('Missing strong external Tick setup and provider')
    elif 'nx_freertos_external_tick_setup' in names:
        raise ValueError('A disabled feature retained an external Tick provider')
    counters = {'nx_freertos_runtime_counter_start',
                'nx_freertos_runtime_counter_now'}
    if policy['runtime_stats']:
        if not counters <= names:
            raise ValueError('Missing retained runtime counter provider')
    elif counters & names:
        raise ValueError('A disabled feature retained a runtime counter')
    if not policy['task_notifications'] and any(
            name.startswith(('xTaskGenericNotify', 'ulTaskGenericNotify',
                             'xTaskNotifyStateClear', 'ulTaskNotifyValueClear'))
            for name in names):
        raise ValueError('A disabled feature retained kernel task notifications')
    if policy['tickless'] and not {
            'nx_freertos_suppress_ticks_and_sleep', 'nx_freertos_lowpower_port',
            'nx_arch_wait_for_interrupt', 'fixture_pause_tick',
            'fixture_resume_tick'} <= names:
        raise ValueError('Missing actual explicit lowpower port and sleep hook')
    if policy['trace'] and not {'nx_freertos_trace_event',
                               'nx_diagnostic_ring_write'} <= names:
        raise ValueError('Missing actual bounded diagnostic trace sink')


def object_sizes(text: str) -> dict:
    result = {name: int(value, 16) for value, name in re.findall(
        r'^\s*([0-9a-fA-F]+)\s+A\s+__nexus_os_size_(\w+)\s*$',
        text, re.MULTILINE)}
    if not {'tcb', 'wait_port'} <= result.keys() or any(value <= 0 for value in result.values()):
        raise ValueError('Actual absolute OS object size symbols are missing')
    return result


def image_size(text: str) -> dict:
    lines = text.splitlines()
    if len(lines) != 2 or not re.match(r'\s*text\s+data\s+bss\s+dec\s+hex', lines[0]):
        raise ValueError('Actual GNU size report is malformed')
    match = re.match(r'\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([0-9a-f]+)\s+.+$', lines[1])
    if not match:
        raise ValueError('Actual GNU size values are malformed')
    code, data, bss, total, hexadecimal = match.groups()
    code, data, bss, total = map(int, (code, data, bss, total))
    if not code or not bss or total != code + data + bss or total != int(hexadecimal, 16):
        raise ValueError('Actual GNU size totals are inconsistent')
    return {'text': code, 'data': data, 'bss': bss,
            'static_ram': data + bss, 'flash_load': code + data}


def compare_costs(variants: list[dict]) -> dict:
    baseline = next(item['costs'] for item in variants if item['name'] == 'standard')
    minimal = next(item['costs'] for item in variants if item['name'] == 'minimal')
    ram = baseline['image']['static_ram'] - minimal['image']['static_ram']
    tcb = baseline['object_sizes']['tcb'] - minimal['object_sizes']['tcb']
    if ram <= 0 or tcb <= 0:
        raise ValueError('The minimal profile has no measured RAM/TCB savings')
    return {'baseline': 'standard', 'comparison': 'minimal',
            'static_ram_saved': ram, 'per_tcb_bytes_saved': tcb,
            'scope': 'identical_software_fixture_and_m4f_abi',
            'physical_status': 'not_executed'}


def source_inputs(root: Path) -> list[dict]:
    inputs = {item['path']: item for item in cortex_runtime.source_inputs(root)}
    paths = [root / 'tools/configure/kernel.py', Path(__file__),
             root / 'tests/contracts/os_kernel_profile/CMakeLists.txt',
             root / 'tests/contracts/os_kernel_profile/main.c']
    kernel_root = root / 'ext/freertos'
    paths.extend(kernel_root / name for name in ('tasks.c', 'queue.c', 'list.c'))
    paths.extend((kernel_root / 'include').glob('*.h'))
    for variant in matrix():
        port = variant['profile']['irq']['kernel_port']
        if port != 'nexus/ARM_CM7_integer':
            paths.extend((kernel_root / 'portable' / port).glob('*.c'))
            paths.extend((kernel_root / 'portable' / port).glob('*.h'))
    for path in paths:
        identity = compiler_gate.digest(path)
        inputs[identity['path']] = identity
    return [inputs[name] for name in sorted(inputs)]


def execute_variant(root: Path, recorder, selected_tools: dict,
                    variant: dict) -> dict:
    directory = recorder.output / variant['name']
    assembly = write_inputs(directory / 'inputs', variant)
    build = directory / 'build'
    cmake, ninja = shutil.which('cmake'), shutil.which('ninja')
    if cmake is None or ninja is None:
        raise ValueError('Actual CMake and Ninja tools are required')
    recorder.execute([
        cmake, '-S', str(root / 'tests/contracts/os_kernel_profile'), '-B',
        str(build), '-G', 'Ninja',
        '-DCMAKE_TOOLCHAIN_FILE=' + str(root / 'cmake/toolchains/arm-gcc.cmake'),
        '-DCMAKE_C_COMPILER=' + selected_tools['gcc'],
        '-DPython3_EXECUTABLE=' + sys.executable,
        '-DNEXUS_RUNTIME_ASSEMBLY=' + str(assembly),
        '-DNEXUS_TEST_EXTERNAL_TICK_PROVIDER=' + ('ON' if variant['external_provider'] else 'OFF'),
        '-DNEXUS_TEST_RUNTIME_COUNTER_PROVIDER=' + ('ON' if variant['counter_provider'] else 'OFF')],
        variant['name'] + '-configure')
    elf = build / 'nexus_os_kernel_profile.elf'
    argv = [cmake, '--build', str(build), '--parallel', '2',
            '--target', 'nexus_os_kernel_profile']
    if variant['expected_rejection']:
        try:
            recorder.execute(argv, variant['name'] + '-expected-rejection')
        except ValueError:
            if recorder.commands[-1]['returncode'] is None:
                raise
        command = recorder.commands[-1]
        diagnostic = '\n'.join(Path(command[key]['path']).read_text()
                               for key in ('stdout', 'stderr'))
        check_rejection(variant, command['returncode'], diagnostic, elf.exists())
        if not re.search(r'^FAILED:.*nexus_os_kernel_profile\.elf\s*$',
                         diagnostic, flags=re.MULTILINE):
            raise ValueError('The expected hook rejection did not occur at the ELF link')
        objects = cortex_runtime.compiled_objects(root, build, variant)
        cortex_runtime.check_kernel_sources(root, build, variant, objects)
        command['expected_outcome'] = 'missing_hook_link_rejection'
        command['qualifies_positive_link'] = False
        return {'name': variant['name'], 'status': 'expected_rejection_observed',
                'qualifies_positive_link': False,
                'assembly': compiler_gate.digest(assembly),
                'facts': compiler_gate.digest(assembly.parent / 'cpu.json'),
                'compiled_objects': objects,
                'physical_status': 'not_executed'}
    recorder.execute(argv, variant['name'] + '-build')
    generated = build / 'nexus-runtime/generated/resolved-cpu.json'
    resolved = json.loads(generated.read_text())
    if (resolved['cpu_profile'] != variant['profile'] or
            resolved['kernel_profile'] != variant['kernel']):
        raise ValueError('Actual kernel resolution differs from authored policy')
    compiler_gate.check_arch_symbols(recorder.execute(
        [selected_tools['nm'], '-u', str(elf)], variant['name'] + '-undefined'))
    names = recorder.execute([selected_tools['nm'], '--defined-only', str(elf)],
                             variant['name'] + '-symbols', nonempty=True)
    required = {'SVC_Handler', 'PendSV_Handler', 'SysTick_Handler',
                'vTaskStartScheduler', 'nx_freertos_direct_notify_init',
                'nx_freertos_permanent_task_start'}
    if not required <= cortex_runtime.symbols(names):
        raise ValueError('Actual kernel/adapter contract symbols were discarded')
    cortex_runtime.check_runtime_dependencies(names)
    if any(name in cortex_runtime.symbols(names) for name in
           ('malloc', 'calloc', 'realloc', 'free', 'pvPortMalloc', 'vPortFree')):
        raise ValueError('Unexpected dynamic allocation in static kernel profile')
    check_optional_symbols(variant, names)
    disassembly = recorder.execute([selected_tools['objdump'], '-d', str(elf)],
                                   variant['name'] + '-disassembly', nonempty=True)
    cortex_runtime.check_kernel_context(variant, disassembly)
    cortex_runtime.check_coprocessor_initialization(variant, disassembly)
    attributes = recorder.execute([selected_tools['readelf'], '-A', str(elf)],
                                  variant['name'] + '-attributes', nonempty=True)
    hard = bool(re.search(r'Tag_ABI_VFP_args:\s*VFP registers', attributes))
    if hard != (variant['facts']['cpu']['float_abi'] == 'hard'):
        raise ValueError('Linked kernel profile has contradictory floating ABI')
    objects = cortex_runtime.compiled_objects(root, build, variant)
    cortex_runtime.check_kernel_sources(root, build, variant, objects)
    resources = recorder.execute([selected_tools['size'], str(elf)],
                                  variant['name'] + '-resources', nonempty=True)
    return {'name': variant['name'], 'status': 'kernel_profile_link_passed',
            'cpu': variant['facts']['cpu']['arch'],
            'profile': variant['kernel']['profile'],
            'kernel_port': variant['profile']['irq']['kernel_port'],
            'physical_status': 'not_executed',
            'assembly': compiler_gate.digest(assembly),
            'facts': compiler_gate.digest(assembly.parent / 'cpu.json'),
            'resolved': compiler_gate.digest(generated),
            'elf': compiler_gate.digest(elf),
            'map': compiler_gate.digest(build / 'nexus_os_kernel_profile.map'),
            'database': compiler_gate.digest(build / 'compile_commands.json'),
            'compiled_objects': objects,
            'costs': {'object_sizes': object_sizes(names),
                      'image': image_size(resources)}}


def run(root: Path, compiler: str, output: Path) -> int:
    root, output = root.resolve(), output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError('Kernel qualification output must be fresh')
    recorder = compiler_gate.Recorder(output)
    negative = compiler_gate.Recorder(output / 'expected-rejections')
    report = {'schema': 1, 'status': 'failed',
              'scope': 'authored_kernel_profile_link_and_resource_matrix',
              'full_platform_status': 'not_qualified',
              'physical_status': 'not_executed', 'variants': [],
              'expected_rejections': [], 'commands': recorder.commands,
              'expected_rejection_commands': negative.commands}
    try:
        selected_tools = compiler_gate.tools(compiler)
        selected_tools['size'] = str(Path(selected_tools['gcc']).with_name('arm-none-eabi-size'))
        for tool in ('cmake', 'ninja'):
            path = shutil.which(tool)
            if path is None:
                raise ValueError(f'Missing required build tool: {tool}')
            selected_tools[tool] = str(Path(path).resolve())
        selected_tools['python'] = str(Path(sys.executable).resolve())
        for name, path in selected_tools.items():
            recorder.execute([path, '--version'], name + '-version', nonempty=True)
        report['tools'] = {name: compiler_gate.digest(Path(path))
                           for name, path in selected_tools.items()}
        report['source_inputs'] = source_inputs(root)
        expected = matrix()
        report['expected_positive_variants'] = sum(not item['expected_rejection'] for item in expected)
        report['expected_rejection_variants'] = sum(item['expected_rejection'] for item in expected)
        for variant in expected:
            result = execute_variant(root, negative if variant['expected_rejection'] else recorder,
                                     selected_tools, variant)
            report['expected_rejections' if variant['expected_rejection'] else 'variants'].append(result)
            cortex_runtime.verify_source_inputs(report['source_inputs'])
            (output / 'report.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
        report['resource_comparison'] = compare_costs(report['variants'])
        report['source_inputs_stable'] = True
        report['status'] = 'passed'
    except (OSError, ValueError, KeyError, UnicodeError) as error:
        report['error'] = str(error)
    (output / 'report.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    return 0 if report['status'] == 'passed' else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--compiler', default='arm-none-eabi-gcc')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if compiler is not None:
        os.environ['PATH'] = str(Path(compiler).parent) + os.pathsep + os.environ.get('PATH', '')
    return run(args.root, args.compiler, args.output)


if __name__ == '__main__':
    raise SystemExit(main())
