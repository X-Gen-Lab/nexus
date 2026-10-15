"""Qualify the real static MPU kernel ports, SVC table and linker domains.

This CPU software fixture executes no firmware. Its exact regions establish
linkage and storage contracts, never physical MPU fault behavior or timing.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import sys

import arch_compile as compiler_gate
import cortex_runtime
from tools.configure import cpu, kernel


def symbols(text: str) -> dict[str, tuple[int, int]]:
    return {name: (int(address, 16), int(size)) for address, size, name in re.findall(
        r'^\s*\d+:\s*([0-9a-fA-F]+)\s+(\d+)\s+\w+\s+\w+\s+\w+\s+\w+\s+(\S+)\s*$',
        text, re.MULTILINE)}


def check_syscall_table(data: bytes, delay: int, ticks: int) -> None:
    if len(data) != 70 * 4:
        raise ValueError('Actual MPU syscall table has the wrong pinned ABI size')
    expected = [0] * 70
    expected[6], expected[13] = delay | 1, ticks | 1
    if tuple(expected) != struct.unpack('<70I', data):
        raise ValueError('MPU syscall table exposes unreviewed services or wrong implementations')


def check_domains(table: dict[str, tuple[int, int]]) -> None:
    groups = {
        'privileged_flash': ('nx_freertos_mpu_task_start', 'nx_freertos_mpu_task_delete',
                             'nx_freertos_mpu_layout', 'nx_freertos_permanent_task_start',
                             'MPU_vTaskDelayImpl', 'MPU_xTaskGetTickCountImpl'),
        'user_flash': ('nx_freertos_user_delay', 'nx_freertos_user_ticks', 'user_entry'),
        'syscall_flash': ('MPU_vTaskDelay', 'MPU_xTaskGetTickCount'),
        'privileged_ram': ('uxSystemCallImplementations', 'task', 'idle_task'),
    }
    for domain, names in groups.items():
        try:
            begin = table[f'__nexus_{domain}_start__'][0]
            end = table[f'__nexus_{domain}_end__'][0]
            values = [(name, table[name]) for name in names]
        except KeyError as error:
            raise ValueError('Actual MPU domain or retained symbol is missing') from error
        if begin >= end:
            raise ValueError('Actual MPU domain is empty')
        for name, (address, size) in values:
            address &= ~1 if domain.endswith('flash') else ~0
            if size <= 0 or address < begin or address >= end or size > end - address:
                raise ValueError(f'{name} is outside its required {domain} region')
    forbidden = {'malloc', 'calloc', 'realloc', 'free', 'pvPortMalloc', 'vPortFree'}
    if forbidden & table.keys() or any('KernelObjectPool' in name for name in table):
        raise ValueError('MPU software contains allocation or a maximum-object pool')
    veneers = {name for name in table if name.startswith('MPU_') and not
               name.endswith(('Impl', '_Priv', '_Unpriv'))}
    if veneers != {'MPU_vTaskDelay', 'MPU_xTaskGetTickCount'}:
        raise ValueError('Unreviewed MPU veneers are exposed')


def matrix() -> tuple[dict, ...]:
    cases = (
        ('m3', 'cortex-m3', {}),
        ('m4-integer', 'cortex-m4', {}),
        ('m4f', 'cortex-m4', {'fpu': 'fpv4-sp-d16', 'float_abi': 'hard'}),
        ('m23', 'cortex-m23', {}),
        ('m33-integer', 'cortex-m33', {}),
        ('m33f', 'cortex-m33', {'fpu': 'fpv5-sp-d16', 'float_abi': 'hard'}),
        ('m55-integer-mve', 'cortex-m55', {'mve': 'integer', 'float_abi': 'softfp'}),
        ('m85-fp-mve', 'cortex-m85', {'mve': 'float', 'fpu': 'auto', 'float_abi': 'hard'}),
        ('m33-region16', 'cortex-m33', {'mpu_regions': 16}),
        ('m23-nonsecure', 'cortex-m23', {'security': 'nonsecure'}),
        ('m33-nonsecure', 'cortex-m33', {'security': 'nonsecure'}),
        ('m33-secure', 'cortex-m33', {'security': 'secure', 'sau': True}),
    )
    variants = []
    for name, architecture, choices in cases:
        facts = compiler_gate.reference_facts(architecture, **{'mpu_regions': 8, **choices})
        profile = cpu.resolve(facts['cpu'], facts['irq'], 'freertos')
        options = {'profile': 'minimal', 'memory_protection': True,
                   'system_call_stack_words': 128}
        selected = kernel.resolve(options, profile, facts['clock_hz'])
        profile = kernel.bind(profile, selected)
        variants.append({'name': name, 'backend': 'freertos', 'facts': facts,
                         'choices': options, 'profile': profile.to_dict(),
                         'kernel': selected.to_dict()})
    return tuple(variants)


def function_instructions(disassembly: str, table: dict, name: str) -> str:
    try:
        address, size = table[name]
    except KeyError as error:
        raise ValueError('Actual retained instruction symbol is missing: ' + name) from error
    address &= ~1
    if size <= 0:
        raise ValueError('Actual instruction body is empty: ' + name)
    lines = []
    for line in disassembly.splitlines():
        match = re.match(r'\s*([0-9a-f]+):\s', line)
        if match and address <= int(match.group(1), 16) < address + size:
            lines.append(line)
    if not lines:
        raise ValueError('Actual retained body has no disassembly: ' + name)
    return '\n'.join(lines)


def check_bootstrap(disassembly: str, table: dict, version: int) -> None:
    """The real fallback cannot reach restore before one-shot admission."""
    dispatcher = 'vSVCHandler_C' if version == 7 else 'vPortSVCHandler_C'
    body = function_instructions(disassembly, table, dispatcher)
    admission = body.find('<nx_freertos_mpu_start_consume>')
    restore = body.find('RestoreContextOfFirstTask>')
    if admission < 0 or restore < 0 or admission >= restore:
        raise ValueError('Actual special SVC fallback lacks guard before restore')
    entry = function_instructions(disassembly, table, 'xPortStartScheduler')
    first_call = re.search(r'\bbl\s+[^\n]+', entry)
    if first_call is None or '<nx_freertos_mpu_start_arm>' not in first_call.group():
        raise ValueError('Actual scheduler entry writes before its cold bootstrap admission')
    label = table.get('__nexus_mpu_start_svc_return', (0, 0))[0] & ~1
    begin = table['__nexus_privileged_flash_start__'][0]
    end = table['__nexus_privileged_flash_end__'][0]
    if not begin + 2 <= label < end:
        raise ValueError('Actual bootstrap return label is outside protected flash')
    svc = 100 if version == 7 else 102
    if not re.search(rf'^\s*{label - 2:x}:\s+[^\n]*\bsvc\s+(?:#)?{svc}\b',
                     disassembly, re.MULTILINE):
        raise ValueError('Actual bootstrap origin is not immediately after pinned START SVC')
    assembly = 'SVC_Handler'
    body = function_instructions(disassembly, table, assembly)
    if not re.search(r'\bmov\s+r1,\s*lr\b', body) or f'<{dispatcher}>' not in body:
        raise ValueError('Actual special assembly does not forward raw EXC_RETURN')
    for name in ('nx_freertos_mpu_start_arm', 'nx_freertos_mpu_start_consume',
                 'nx_freertos_mpu_start_facts'):
        address, size = table[name]
        address &= ~1
        if size <= 0 or address < begin or size > end - address:
            raise ValueError('Actual bootstrap admission code is not protected')
    address, size = table.get('xNexusMpuStart', (0, 0))
    if size != 4 or address < table['__nexus_privileged_ram_start__'][0] or \
            address + size > table['__nexus_privileged_ram_end__'][0]:
        raise ValueError('Actual one-shot bootstrap lease is not exactly protected four bytes')


def execute(root: Path, recorder, tools: dict, variant: dict) -> dict:
    directory = recorder.output / variant['name']
    assembly = cortex_runtime.write_inputs(directory / 'inputs', variant)
    with assembly.open('a', encoding='utf-8') as stream:
        stream.write('\n[os]\n')
        for key, value in sorted(variant['choices'].items()):
            stream.write(key + ' = ' + json.dumps(value) + '\n')
    build = directory / 'build'
    cmake = shutil.which('cmake')
    if cmake is None or shutil.which('ninja') is None:
        raise ValueError('Actual CMake and Ninja are required')
    recorder.execute([cmake, '-S', str(root / 'tests/contracts/os_mpu'), '-B', str(build),
                      '-G', 'Ninja', '-DCMAKE_TOOLCHAIN_FILE=' +
                      str(root / 'cmake/toolchains/arm-gcc.cmake'),
                      '-DCMAKE_C_COMPILER=' + tools['gcc'],
                      '-DPython3_EXECUTABLE=' + sys.executable,
                      '-DNEXUS_RUNTIME_ASSEMBLY=' + str(assembly),
                      '-DNEXUS_TEST_MPU_VERSION=' + str(variant['facts']['cpu']['mpu_version'])],
                     variant['name'] + '-configure')
    recorder.execute([cmake, '--build', str(build), '--parallel', '2',
                      '--target', 'nexus_os_mpu'], variant['name'] + '-build')
    elf = build / 'nexus_os_mpu.elf'
    undefined = recorder.execute([tools['nm'], '-u', str(elf)], variant['name'] + '-undefined')
    compiler_gate.check_arch_symbols(undefined)
    symbolic = recorder.execute([tools['readelf'], '-sW', str(elf)],
                                variant['name'] + '-symbols', nonempty=True)
    table = symbols(symbolic)
    check_domains(table)
    header = recorder.execute([tools['objdump'], '-h', str(elf)],
                              variant['name'] + '-sections', nonempty=True)
    match = re.search(r'^\s*\d+\s+\.data\s+[0-9a-f]+\s+([0-9a-f]+)', header, re.MULTILINE)
    if not match:
        raise ValueError('Actual protected initialized data section is missing')
    data_file = directory / 'protected-data.bin'
    recorder.execute([tools['objcopy'], '--dump-section', '.data=' + str(data_file), str(elf)],
                     variant['name'] + '-extract-protected-data')
    table_address, table_size = table['uxSystemCallImplementations']
    offset = table_address - int(match.group(1), 16)
    data = data_file.read_bytes()
    if offset < 0 or offset + table_size > len(data):
        raise ValueError('Actual syscall table does not fit the protected data section')
    check_syscall_table(data[offset:offset + table_size],
                       table['MPU_vTaskDelayImpl'][0], table['MPU_xTaskGetTickCountImpl'][0])
    disassembly = recorder.execute([tools['objdump'], '-d', str(elf)],
                                   variant['name'] + '-disassembly', nonempty=True)
    check_bootstrap(disassembly, table, variant['facts']['cpu']['mpu_version'])
    for name, number in (('MPU_vTaskDelay', 6), ('MPU_xTaskGetTickCount', 13)):
        address = table[name][0] & ~1
        size = table[name][1]
        instructions = []
        for line in disassembly.splitlines():
            match = re.match(r"\s*([0-9a-f]+):\s", line)
            if match and address <= int(match.group(1), 16) < address + size:
                instructions.append(line)
        body = "\n".join(instructions)
        if not re.search(rf'\bsvc\s+(?:#)?{number}\b', body):
            raise ValueError('The actual retained narrow veneer lacks its pinned SVC')
    resolved = build / 'nexus-runtime/generated/resolved-cpu.json'
    value = json.loads(resolved.read_text())
    if value['cpu_profile'] != variant['profile'] or value['kernel_profile'] != variant['kernel']:
        raise ValueError('Actual MPU runtime resolution differs from its authored policy')
    database = json.loads((build / 'compile_commands.json').read_text())
    sources = {Path(item['file']).resolve() for item in database}
    if any(path.name == 'mpu_wrappers_v2.c' for path in sources):
        raise ValueError('The global-object-pool omnibus wrapper was compiled')
    for name in ('task.c', 'layout.c', 'syscalls.c', 'guard.c', 'guard_arm.c'):
        if (root / 'os/freertos/mpu' / name).resolve() not in sources:
            raise ValueError('The actual MPU implementation was not compiled')
    derived = build / 'nexus-runtime/os/freertos/mpu-port'
    if not (derived / 'veneers.c').exists():
        raise ValueError('Hashchecked narrow veneers were not derived')
    if (derived / 'port.c').resolve() not in sources:
        raise ValueError('The actual kernel bypassed its reviewed bootstrap derivation')
    if variant['facts']['cpu']['mpu_version'] == 8 and \
            (derived / 'portasm.c').resolve() not in sources:
        raise ValueError('The actual v8 assembly bypassed bootstrap EXC_RETURN forwarding')
    sizes = {name: table['__nexus_os_mpu_' + name][0]
             for name in ('task_size', 'tcb_size', 'syscall_stack_bytes')}
    if sizes['task_size'] <= sizes['tcb_size'] or sizes['syscall_stack_bytes'] != 512:
        raise ValueError('Actual protected task and syscall-stack sizes are inconsistent')
    return {'name': variant['name'], 'status': 'real_mpu_port_link_passed',
            'physical_status': 'not_executed', 'kernel_port': variant['profile']['irq']['kernel_port'],
            'sizes': sizes, 'elf': compiler_gate.digest(elf),
            'map': compiler_gate.digest(build / 'nexus_os_mpu.map'),
            'resolved': compiler_gate.digest(resolved),
            'derived_veneers': compiler_gate.digest(derived / 'veneers.c'),
            'derived_port': compiler_gate.digest(derived / 'port.c'),
            'bootstrap_return': ('0xfffffff9' if variant['profile']['definitions']
                                 ['NEXUS_CPU_SECURE_ONLY'] == 1 or
                                 variant['facts']['cpu']['mpu_version'] == 7 else '0xffffffb8'),
            'compile_database': compiler_gate.digest(build / 'compile_commands.json')}


def missing_idle(root: Path, recorder, tools: dict, variant: dict) -> dict:
    """An MPU image cannot silently acquire a platform-owned idle pool."""
    directory = recorder.output / 'missing-idle-provider'
    assembly = cortex_runtime.write_inputs(directory / 'inputs', variant)
    with assembly.open('a', encoding='utf-8') as stream:
        stream.write('\n[os]\nprofile="minimal"\nmemory_protection=true\n')
    build = directory / 'build'
    cmake = shutil.which('cmake')
    recorder.execute([cmake, '-S', str(root / 'tests/contracts/os_mpu'), '-B', str(build),
                      '-G', 'Ninja', '-DCMAKE_TOOLCHAIN_FILE=' +
                      str(root / 'cmake/toolchains/arm-gcc.cmake'),
                      '-DCMAKE_C_COMPILER=' + tools['gcc'],
                      '-DPython3_EXECUTABLE=' + sys.executable,
                      '-DNEXUS_RUNTIME_ASSEMBLY=' + str(assembly),
                      '-DNEXUS_TEST_MPU_VERSION=7',
                      '-DNEXUS_TEST_MPU_MISSING_IDLE_PROVIDER=ON'],
                     'missing-idle-configure')
    try:
        recorder.execute([cmake, '--build', str(build), '--parallel', '2',
                          '--target', 'nexus_os_mpu'], 'missing-idle-link')
    except ValueError:
        pass
    record = recorder.commands[-1]
    diagnostic = '\n'.join(Path(record[item]['path']).read_text()
                           for item in ('stdout', 'stderr'))
    if (record['returncode'] != 1 or (build / 'nexus_os_mpu.elf').exists() or
            not re.search(r'undefined reference[^\n]*vApplicationGetIdleTaskMemory', diagnostic) or
            not re.search(r'^FAILED:.*nexus_os_mpu\.elf', diagnostic, re.MULTILINE)):
        raise ValueError('Missing precise caller-owned idle-storage link rejection')
    record['expected_outcome'] = 'missing_caller_idle_storage_link_rejected'
    record['qualifies_positive_link'] = False
    return {'name': 'missing-idle-provider', 'status': 'expected_rejection_observed',
            'qualifies_positive_link': False, 'physical_status': 'not_executed'}


def source_inputs(root: Path) -> list[dict]:
    inputs = {item['path']: item for item in cortex_runtime.source_inputs(root)}
    paths = [Path(__file__), root / 'os/freertos/mpu/prepare_port.py']
    paths.extend(path for path in (root / 'tests/contracts/os_mpu').glob('*')
                 if path.is_file())
    paths.extend((root / 'ext/freertos/include').glob('*.h'))
    paths.extend(root / 'ext/freertos' / name for name in ('tasks.c', 'queue.c', 'list.c'))
    for variant in matrix():
        port = root / 'ext/freertos/portable' / variant['profile']['irq']['kernel_port']
        paths.extend(port.glob('*.c'))
        paths.extend(port.glob('*.h'))
    for path in paths:
        identity = compiler_gate.digest(path)
        inputs[identity['path']] = identity
    return [inputs[name] for name in sorted(inputs)]


def run(root: Path, compiler: str, output: Path) -> int:
    if output.exists() and any(output.iterdir()):
        raise ValueError('MPU qualification output must be fresh')
    recorder = compiler_gate.Recorder(output)
    report = {'schema': 1, 'status': 'failed', 'scope': 'real_mpu_kernel_link_and_domains',
              'physical_status': 'not_executed', 'variants': [], 'commands': recorder.commands}
    try:
        tools = compiler_gate.tools(compiler)
        report['source_inputs'] = source_inputs(root)
        report['toolchain'] = tools
        tools['objcopy'] = str(Path(tools['gcc']).with_name('arm-none-eabi-objcopy'))
        for variant in matrix():
            report['variants'].append(execute(root, recorder, tools, variant))
        report['expected_rejections'] = [missing_idle(root, recorder, tools, matrix()[2])]
        cortex_runtime.verify_source_inputs(report['source_inputs'])
        report['status'] = 'passed'
    except (OSError, ValueError) as error:
        report['error'] = str(error)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'mpu-matrix.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'variants': len(report['variants']),
                      'report': str(output / 'mpu-matrix.json')}))
    return 0 if report['status'] == 'passed' else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--output', type=Path, required=True)
    arguments = parser.parse_args()
    return run(Path(__file__).resolve().parents[2], arguments.compiler, arguments.output)


if __name__ == '__main__':
    raise SystemExit(main())
