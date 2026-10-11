"""Link the actual locked ThreadX kernel on three Boards and two IRQ policies.

Six positive ELF/resource checks and a precise missing external-Tick rejection
are software evidence. They never establish physical execution, IRQ/FPU timing,
stack peaks, an installed SDK or a complete clean product qualification.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import sys

import arch_compile as compiler_gate

REQUIRED_SYMBOLS = frozenset({
    'PendSV_Handler', '__tx_PendSVHandler', 'SysTick_Handler',
    '_tx_timer_interrupt', '_tx_thread_schedule', 'nx_threadx_kernel_enter',
    'nx_threadx_task_start', 'nx_threadx_permanent_task_start',
    'nx_threadx_task_join', 'nx_threadx_queue_send_until',
    'nx_threadx_closable_queue_close', 'nx_threadx_notify_init'})


def matrix():
    rows = [{'name': board + '-' + mask, 'board': board, 'mask': mask,
             'rejection': False} for board in ('qiming', 'sky', 'liangshan')
            for mask in ('basepri', 'primask')]
    rows.append({'name': 'qiming-external-missing', 'board': 'qiming',
                 'mask': 'basepri', 'rejection': True})
    return tuple(rows)


def output_path(output):
    output = Path(output).absolute()
    if any(path.is_symlink() for path in (output, *output.parents)):
        raise ValueError('ThreadX output must not contain symlinks')
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError('ThreadX output must be fresh')
    return output


def write_inputs(root, directory, row):
    source = root / 'tools/configure/assemblies' / (
        row['board'] + '-freertos-empty.toml')
    text = source.read_text(encoding='utf-8')
    if text.count('backend = "freertos"') != 1 or '[os]' in text:
        raise ValueError('Reviewed empty assembly has unexpected policy')
    directory.mkdir(parents=True, exist_ok=True)
    assembly = directory / 'assembly.toml'
    text = text.replace('backend = "freertos"', 'backend = "threadx"')
    text += '\n[os]\ninterrupt_mask = ' + json.dumps(row['mask']) + '\n'
    if row['rejection']:
        text += 'tick_source = "external"\n'
    assembly.write_text(text, encoding='utf-8')
    return assembly


def check_rejection(returncode, diagnostic, elf_exists):
    if (returncode != 1 or elf_exists or
            not re.search(r'^FAILED:.*nexus_threadx_runtime\.elf\s*$',
                          diagnostic, re.MULTILINE) or
            not re.search(r'undefined reference to[^\n]*nx_threadx_external_tick_setup',
                          diagnostic)):
        raise ValueError('Expected precise external Tick ELF link rejection')


def check_symbols(text):
    symbols = {}
    for line in text.splitlines():
        match = re.fullmatch(r'\s*([0-9a-fA-F]+)\s+([A-Za-z])\s+(\S+)\s*', line)
        if match:
            symbols[match[3]] = (int(match[1], 16), match[2])
    if not REQUIRED_SYMBOLS <= symbols.keys():
        raise ValueError('Missing retained actual kernel/adapter symbols')
    if (symbols['PendSV_Handler'][1] != 'T' or
            symbols['__tx_PendSVHandler'][1] != 'T' or
            symbols['PendSV_Handler'][0] != symbols['__tx_PendSVHandler'][0] or
            symbols['SysTick_Handler'][1] != 'T'):
        raise ValueError('PendSV must use the same strong official entry')
    if {'malloc', 'calloc', 'realloc', 'free', 'pthread_create',
            'sem_clockwait'} & symbols.keys():
        raise ValueError('Unexpected dynamic or host service in MCU image')


def source_inputs(root):
    paths = set()
    for layer in ('core', 'arch', 'io', 'os', 'soc', 'boards', 'tools/configure',
                  'tests/contracts/os_threadx_runtime'):
        paths.update(path for path in (root / layer).rglob('*')
                     if path.is_file() and '__pycache__' not in path.parts and
                     (path.suffix in {'.c', '.h', '.S', '.s', '.py', '.json',
                                      '.toml', '.cmake'} or
                      path.name == 'CMakeLists.txt'))
    for name in ('CMakeLists.txt', 'dependencies/threadx.lock.json',
                 'dependencies/platform-compatibility.json',
                 'tools/maintenance/threadx_lock.py',
                 'cmake/platform/Configure.cmake', 'cmake/platform/Options.cmake',
                 'cmake/platform/Firmware.cmake', 'cmake/toolchains/arm-gcc.cmake',
                 'scripts/ci/os_threadx.py'):
        if (root / name).is_file():
            paths.add(root / name)
    return [compiler_gate.digest(path) for path in sorted(paths)]


def compiled_kernel(root, build, row, resolved):
    lock = json.loads((root / 'dependencies/threadx.lock.json').read_text())
    expected = {str(root / 'ext/threadx' / path)
                for path in lock['common'] + lock['ports']['cortex_m4']}
    database = json.loads((build / 'compile_commands.json').read_text())
    actual = []
    objects = []
    encoded = resolved['kernel_profile']['definitions']['NEXUS_THREADX_BASEPRI']
    for entry in database:
        source = str(Path(entry['file']).resolve())
        if source not in expected:
            continue
        arguments = entry.get('arguments') or shlex.split(entry['command'])
        uses_basepri = '-DTX_PORT_USE_BASEPRI' in arguments
        if uses_basepri != (row['mask'] == 'basepri'):
            raise ValueError('C/ASM policy differs from authored IRQ selection')
        if uses_basepri and '-DTX_PORT_BASEPRI=' + str(encoded) not in arguments:
            raise ValueError('C/ASM encoded kernel ceiling differs')
        if '-DTX_INCLUDE_USER_DEFINE_FILE' not in arguments:
            raise ValueError('Kernel compilation omits reviewed user policy')
        index = arguments.index('-o')
        output = Path(arguments[index + 1])
        if not output.is_absolute():
            output = Path(entry['directory']) / output
        objects.append(compiler_gate.digest(output))
        actual.append(source)
    if set(actual) != expected or len(actual) != len(expected):
        raise ValueError('Actual compiled kernel graph differs from source lock')
    return objects


def execute_variant(root, recorder, tools, row):
    directory = recorder.output / row['name']
    assembly = write_inputs(root, directory / 'inputs', row)
    build = directory / 'build'
    recorder.execute([tools['cmake'], '-S', str(root / 'tests/contracts/os_threadx_runtime'),
                      '-B', str(build), '-G', 'Ninja',
                      '-DCMAKE_TOOLCHAIN_FILE=' + str(root / 'cmake/toolchains/arm-gcc.cmake'),
                      '-DCMAKE_C_COMPILER=' + tools['gcc'],
                      '-DPython3_EXECUTABLE=' + sys.executable,
                      '-DNEXUS_ASSEMBLY_FILE=' + str(assembly)], row['name'] + '-configure')
    command = [tools['cmake'], '--build', str(build), '--target',
               'nexus_threadx_runtime', '--parallel', '1']
    elf = build / 'bin/nexus_threadx_runtime.elf'
    if row['rejection']:
        try:
            recorder.execute(command, row['name'] + '-expected-rejection')
        except ValueError:
            if recorder.commands[-1]['returncode'] is None:
                raise
        outcome = recorder.commands[-1]
        diagnostic = '\n'.join(Path(outcome[key]['path']).read_text()
                               for key in ('stdout', 'stderr'))
        check_rejection(outcome['returncode'], diagnostic, elf.exists())
        return {**row, 'status': 'expected_rejection_observed',
                'qualifies_positive_link': False, 'physical_status': 'not_executed',
                'assembly': compiler_gate.digest(assembly)}
    recorder.execute(command, row['name'] + '-build')
    names = recorder.execute([tools['nm'], '--defined-only', str(elf)],
                             row['name'] + '-symbols', nonempty=True)
    check_symbols(names)
    report_path = build / 'bin/nexus_threadx_runtime.resources.json'
    resources = json.loads(report_path.read_text())
    if resources['status'] != 'passed' or resources['physical_status'] != 'not_executed':
        raise ValueError('Actual ELF budget/physical status is inconsistent')
    resolved_path = build / 'nexus/generated/resolved.json'
    resolved = json.loads(resolved_path.read_text())
    if (resolved['backend'] != 'threadx' or
            resolved['kernel_profile']['interrupt_mask'] != row['mask']):
        raise ValueError('Actual resolution differs from authored ThreadX policy')
    objects = compiled_kernel(root, build, row, resolved)
    return {**row, 'status': 'arm_link_passed', 'physical_status': 'not_executed',
            'assembly': compiler_gate.digest(assembly), 'elf': compiler_gate.digest(elf),
            'bin': compiler_gate.digest(build / 'bin/nexus_threadx_runtime.bin'),
            'map': compiler_gate.digest(build / 'bin/nexus_threadx_runtime.map'),
            'resources': resources, 'resource_report': compiler_gate.digest(report_path),
            'resolved': compiler_gate.digest(resolved_path),
            'compiled_kernel_objects': objects,
            'database': compiler_gate.digest(build / 'compile_commands.json')}


def run(root, compiler, output):
    root, output = root.resolve(), output_path(output)
    if output == root or output in root.parents:
        raise ValueError('Qualification output cannot replace source')
    recorder = compiler_gate.Recorder(output)
    report = {'schema': 1, 'kind': 'threadx-production-arm-matrix',
              'status': 'failed', 'physical_status': 'not_executed',
              'full_platform_status': 'not_qualified', 'variants': [],
              'expected_rejections': [], 'commands': recorder.commands}
    try:
        selected = compiler_gate.tools(compiler)
        for tool in ('cmake', 'ninja'):
            path = shutil.which(tool)
            if path is None:
                raise ValueError('Missing actual build tool: ' + tool)
            selected[tool] = str(Path(path).resolve())
        report['tools'] = {name: compiler_gate.digest(Path(path))
                           for name, path in selected.items()}
        report['source_inputs'] = source_inputs(root)
        for row in matrix():
            result = execute_variant(root, recorder, selected, row)
            report['expected_rejections' if row['rejection'] else 'variants'].append(result)
            if source_inputs(root) != report['source_inputs']:
                raise ValueError('Source changed during actual matrix')
            (output / 'report.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
        report['source_inputs_stable'] = True
        report['status'] = 'passed'
    except (OSError, ValueError, KeyError, UnicodeError) as error:
        report['error'] = str(error)
    (output / 'report.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    return 0 if report['status'] == 'passed' else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--compiler', default='arm-none-eabi-gcc')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    path = shutil.which(args.compiler)
    if path:
        os.environ['PATH'] = str(Path(path).parent) + os.pathsep + os.environ.get('PATH', '')
    return run(args.root, args.compiler, args.output)


if __name__ == '__main__':
    raise SystemExit(main())
