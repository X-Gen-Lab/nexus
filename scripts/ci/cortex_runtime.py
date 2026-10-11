"""Build actual Core/Arch/OS Runtime targets for reviewed CPU software facts.

The fixture uses the production CMake graph and one authored TOML per variant.
Linked APIs and context instructions establish software closure, never startup,
board wiring, FPU exception behavior, cache coherency or physical qualification.
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
from tools.configure import cpu as cpu_contract


ARCH_MECHANISMS = frozenset({
    'nx_arch_dcache_clean', 'nx_arch_dcache_invalidate',
    'nx_arch_dcache_clean_invalidate', 'nx_arch_instruction_sync',
    'nx_arch_mpu_v7_encode', 'nx_arch_mpu_v8_encode',
    'nx_arch_mpu_v7_program', 'nx_arch_mpu_v8_program',
    'nx_arch_mpu_v8_attribute_set', 'nx_arch_features',
    'nx_arch_security_state', 'nx_arch_sau_encode', 'nx_arch_sau_write',
    'nx_arch_sau_clear',
    'nx_arch_wait_for_interrupt', 'nx_arch_idle_if_unchanged',
})
CORE_SYMBOLS = frozenset({
    'nx_request_initialize', 'nx_request_prepare', 'nx_request_admit',
    'nx_request_transition', 'nx_request_state', 'nx_request_settle',
    'nx_request_result', 'nx_request_slot_bind', 'nx_request_slot_lookup',
    'nx_request_slot_release', 'nx_clock32_initialize', 'nx_clock32_observe',
    'nx_deadline_after', 'nx_deadline_expired', 'nx_wait_until',
})
FREERTOS_SYMBOLS = frozenset({
    'nx_freertos_isr_allowed', 'nx_freertos_notify_init',
    'nx_freertos_notify_port', 'nx_freertos_notify_destroy',
    'nx_freertos_task_start', 'nx_freertos_task_join',
    'nx_freertos_queue_init', 'nx_freertos_queue_send',
    'nx_freertos_queue_receive', 'nx_freertos_queue_destroy',
    'nx_freertos_queue_send_until', 'nx_freertos_queue_receive_until',
    'nx_freertos_direct_notify_init', 'nx_freertos_direct_notify_port',
    'nx_freertos_direct_notify_destroy', 'nx_freertos_permanent_task_start',
    'nx_freertos_queue_waiter_init', 'nx_freertos_queue_waiter_destroy',
    'nx_freertos_closable_queue_init',
    'nx_freertos_closable_queue_send_until',
    'nx_freertos_closable_queue_receive_until',
    'nx_freertos_closable_queue_close', 'nx_freertos_closable_queue_destroy',
    'SVC_Handler', 'PendSV_Handler', 'SysTick_Handler', 'vTaskStartScheduler',
})


def matrix() -> tuple[dict, ...]:
    result = []
    for item in compiler_gate.software_profiles():
        for backend in ('baremetal', 'freertos'):
            facts = item['facts']
            resolved = cpu_contract.resolve(facts['cpu'], facts['irq'], backend)
            result.append({'name': item['name'] + '-' + backend,
                           'facts': facts, 'backend': backend,
                           'profile': resolved.to_dict()})
    return tuple(result)


def write_inputs(directory: Path, variant: dict) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'cpu.json').write_text(
        json.dumps(variant['facts'], indent=2, sort_keys=True) + '\n',
        encoding='utf-8')
    assembly = directory / 'runtime.toml'
    assembly.write_text('schema_version = 1\ncpu_facts = "cpu.json"\n'
                        f'backend = "{variant["backend"]}"\n'
                        'optimization = "Os"\n', encoding='utf-8')
    return assembly


def source_inputs(root: Path) -> list[dict]:
    paths = set()
    for layer in ('core', 'arch', 'io', 'os', 'components'):
        paths.update(path for path in (root / layer).rglob('*')
                     if path.is_file() and (path.suffix in {'.c', '.h', '.cmake'}
                                            or path.name == 'CMakeLists.txt'))
    paths.update(root / name for name in (
        'tools/configure/cpu.py', 'tools/configure/runtime.py',
        'tools/configure/ir.py', 'tools/configure/kernel.py',
        'tools/configure/providers/common.py',
        'cmake/platform/Runtime.cmake', 'cmake/platform/Options.cmake',
        'cmake/platform/SDK.cmake', 'cmake/toolchains/arm-gcc.cmake',
        'os/freertos/prepare_m7_integer.py',
        'os/freertos/mpu/prepare_port.py',
        'scripts/ci/cortex_runtime.py', 'scripts/ci/arch_compile.py',
        'tests/contracts/cortex_runtime/CMakeLists.txt',
        'tests/contracts/cortex_runtime/main.c',
        'tests/contracts/cortex_runtime/linker.ld'))
    return [compiler_gate.digest(path) for path in sorted(paths)]


def verify_source_inputs(identities: list[dict]) -> None:
    for identity in identities:
        if compiler_gate.digest(Path(identity['path'])) != identity:
            raise ValueError('Runtime source changed during the compiler matrix')


def symbols(text: str) -> set[str]:
    return {match.group(1) for line in text.splitlines() if
            (match := re.fullmatch(
                r'\s*[0-9a-fA-F]+\s+[A-Za-z]\s+(\S+)\s*', line))}


def required_symbols(backend: str) -> frozenset[str]:
    result = compiler_gate.PUBLIC_SYMBOLS | ARCH_MECHANISMS | CORE_SYMBOLS
    if backend == 'freertos':
        return result | FREERTOS_SYMBOLS
    if backend == 'baremetal':
        return result | {'nx_baremetal_notify_init', 'nx_baremetal_notify_port'}
    raise ValueError('Unsupported runtime backend')


def check_retention(text: str, backend: str) -> None:
    missing = required_symbols(backend) - symbols(text)
    if missing:
        raise ValueError(f'Missing retained symbols: {sorted(missing)}')


def check_runtime_dependencies(text: str) -> None:
    forbidden = {name for name in symbols(text) if
                 name.startswith(('__atomic_', '__sync_'))}
    if forbidden:
        raise ValueError(f'Unexpected atomic runtime helper: {sorted(forbidden)}')


def function_body(text: str, name: str) -> str:
    match = re.search(rf'<{re.escape(name)}>:\n(.*?)(?=\n[^\n]*<[^>]+>:\n|\Z)',
                      text, flags=re.DOTALL)
    if not match:
        raise ValueError(f'Retained function body missing: {name}')
    return match.group(1).lower()


def check_m7_integer_errata(variant: dict, text: str) -> None:
    """Audit retained BASEPRI raises, including incoming PRIMASK identity."""
    irq = variant['profile']['irq']
    expected_mask = irq['syscall_priority'] << (8 - irq['priority_bits'])
    register = r'(?:r\d+|ip|sl|fp)'
    for name in ('vPortEnterCritical', 'SysTick_Handler', 'PendSV_Handler'):
        values = {}
        disabled = False
        pending = None
        barriers = 0
        completed = 0
        body = function_body(text, name)
        for line in body.splitlines():
            instruction = re.match(
                r'\s*[0-9a-f]+:\s+(?:[0-9a-f]{4,8}\s+)+'
                r'([a-z][a-z0-9.]+)\s+(.*)', line)
            if not instruction:
                continue
            mnemonic, operands = instruction.groups()
            operation = mnemonic.split('.')[0]
            operands = operands.split('@', 1)[0].strip()
            if operation == 'mrs':
                match = re.fullmatch(rf'({register}),\s*(\w+)', operands)
                if match:
                    values[match.group(1)] = ('incoming_primask' if
                                             match.group(2) == 'primask' else None)
            elif operation in {'mov', 'movs'}:
                match = re.fullmatch(rf'({register}),\s*#(0x[0-9a-f]+|\d+)',
                                     operands)
                if match:
                    values[match.group(1)] = int(match.group(2), 0)
                else:
                    destination = operands.split(',', 1)[0]
                    values.pop(destination, None)
            elif operation == 'cpsid' and operands == 'i':
                disabled = 'incoming_primask' in values.values()
            elif operation == 'cpsie' and operands == 'i':
                raise ValueError(f'M7 integer errata globally unmasks IRQ: {name}')
            elif operation == 'msr':
                target, source = re.split(r',\s*', operands)
                if target == 'basepri' and values.get(source) != 0:
                    if values.get(source) != expected_mask or not disabled:
                        raise ValueError(f'M7 integer errata unguarded BASEPRI: {name}')
                    pending, barriers = source, 0
                elif target == 'primask' and pending is not None:
                    if (values.get(source) != 'incoming_primask' or
                            barriers != 2):
                        raise ValueError(f'M7 integer errata lost incoming mask: {name}')
                    completed += 1
                    pending, disabled = None, False
            elif operation == 'dsb' and pending is not None:
                barriers = 1
            elif operation == 'isb' and pending is not None:
                barriers = 2 if barriers == 1 else 0
            elif operation in {'bl', 'blx'} and pending is not None:
                raise ValueError(f'M7 integer errata calls before mask restore: {name}')
            elif operation not in {'cmp', 'tst', 'push', 'pop', 'bx'} and not (
                    operation.startswith(('b', 'str', 'it'))):
                destination = re.match(rf'({register})\b', operands)
                if destination:
                    values.pop(destination.group(1), None)
        if not completed or pending is not None:
            raise ValueError(f'M7 integer errata guard is incomplete: {name}')


def check_kernel_context(variant: dict, text: str) -> None:
    if variant['profile']['irq']['kernel_port'] == 'nexus/ARM_CM7_integer':
        check_m7_integer_errata(variant, text)
    facts = variant['facts']['cpu']
    extended = facts['fpu'] != 'none' or facts['mve'] != 'none'
    save = re.search(r'\bvstm\w*\s+[^\n]*s16\s*-\s*s31', text.lower())
    restore = re.search(r'\bvldm\w*\s+[^\n]*s16\s*-\s*s31', text.lower())
    if extended:
        # VPR is restored by the architectural extended exception frame. The
        # kernel keeps the high shared FP/MVE bank and tests EXC_RETURN[4].
        frame = re.search(r'\btst(?:\.w)?\s+[^\n]*#(?:16|0x10)\b', text.lower())
        if not (save and restore and frame):
            raise ValueError('Missing coprocessor context save/restore/frame')
    elif save or restore:
        raise ValueError('unexpected coprocessor context on integer-only port')


def coprocessor_register_writes(text: str) -> set[int]:
    """Resolve actual port read/OR/write sequences, including literal pointers.

    This bounded instruction audit follows constants and MMIO values in retained
    initialization functions. It accepts GCC's literal pools and base+offset
    forms, rejects inert constants, lost pointers and missing enable bits, and
    makes no claim about execution or physical register state.
    """
    literals = {int(address, 16): int(value, 16) for address, value in
                re.findall(r'^\s*([0-9a-f]+):\s+[0-9a-f]+\s+\.word\s+'
                           r'0x([0-9a-f]+)', text.lower(), re.MULTILINE)}
    selected = {'xportstartscheduler', 'vportenablevfp', 'prvsetupfpu',
                'vportsvchandler_c'}
    required = {0xe000ed88: 0x00f00000, 0xe000ef34: 0xc0000000}
    values = {}
    writes = set()
    active = False
    register = r'(?:r\d+|ip|sl|fp)'
    integer = r'-?(?:0x[0-9a-f]+|\d+)'
    for line in text.lower().splitlines():
        label = re.fullmatch(r'[0-9a-f]+ <([^>]+)>:', line.strip())
        if label:
            active = label.group(1).split('.')[0] in selected
            values.clear()
            continue
        instruction = re.match(
            r'\s*[0-9a-f]+:\s+(?:[0-9a-f]{4,8}\s+)+'
            r'([a-z][a-z0-9.]+)\s+(.*)', line)
        if not active or not instruction:
            continue
        mnemonic, operands = instruction.groups()
        operation = mnemonic.split('.')[0]
        destination = re.match(rf'({register})\b', operands)
        destination = destination.group(1) if destination else None
        memory = re.match(rf'({register}),\s*\[({register}|pc)'
                          rf'(?:,\s*#({integer}))?\]', operands)
        move = re.fullmatch(rf'({register}),\s*#({integer})(?:\s*@.*)?',
                            operands)
        combine = re.fullmatch(rf'({register}),\s*({register}),\s*#'
                               rf'({integer})(?:\s*@.*)?', operands)
        if operation in {'mov', 'movs', 'movw'} and move:
            values[move.group(1)] = int(move.group(2), 0)
        elif operation == 'movt' and move:
            previous = values.get(move.group(1))
            values[move.group(1)] = ((previous & 0xffff) |
                                     (int(move.group(2), 0) << 16)) if (
                                         isinstance(previous, int)) else None
        elif operation == 'ldr' and memory:
            target, base, offset = memory.groups()
            if base == 'pc':
                reference = re.search(r'@\s*\(?([0-9a-f]+)\b', operands)
                values[target] = literals.get(int(reference.group(1), 16)) if (
                    reference) else None
            else:
                pointer = values.get(base)
                address = pointer + int(offset or '0', 0) if (
                    isinstance(pointer, int)) else None
                values[target] = (address, 0) if address in required else None
        elif operation == 'orr' and combine:
            target, source, bits = combine.groups()
            previous = values.get(source)
            values[target] = (previous[0], previous[1] | int(bits, 0)) if (
                isinstance(previous, tuple)) else None
        elif operation == 'str' and memory:
            source, base, offset = memory.groups()
            pointer, value = values.get(base), values.get(source)
            address = pointer + int(offset or '0', 0) if (
                isinstance(pointer, int)) else None
            if (address in required and isinstance(value, tuple) and
                    value[0] == address and
                    value[1] & required[address] == required[address]):
                writes.add(address)
        elif operation in {'bl', 'blx'}:
            # AAPCS preserves r4-r11, including a scheduler's PPB base pointer.
            for name in ('r0', 'r1', 'r2', 'r3', 'ip'):
                values.pop(name, None)
        elif operation not in {'cmp', 'tst', 'push', 'pop', 'bx'} and not (
                operation.startswith(('b', 'str', 'it'))):
            if destination:
                values.pop(destination, None)
    return writes


def check_coprocessor_initialization(variant: dict, text: str) -> None:
    facts = variant['facts']['cpu']
    if facts['fpu'] == 'none' and facts['mve'] == 'none':
        return
    if coprocessor_register_writes(text) != {0xe000ed88, 0xe000ef34}:
        raise ValueError('Missing coprocessor initialization register accesses')


def check_kernel_sources(root: Path, build: Path, variant: dict,
                         objects: list[dict]) -> None:
    port = variant['profile']['irq']['kernel_port']
    if port is None:
        return
    if port == 'nexus/ARM_CM7_integer':
        directory = build / 'nexus-runtime/os/freertos/m7-integer-port'
    else:
        directory = root / 'ext/freertos/portable' / port
    required = {str(directory / 'port.c')}
    if port == 'GCC/ARM_CM0' or port.endswith('/non_secure'):
        required.add(str(directory / 'portasm.c'))
    actual = {item['source']['path'] for item in objects}
    if not required <= actual:
        raise ValueError('Resolved kernel port source/context objects are missing')
    competing = {name for name in actual if
                 Path(name).name in {'port.c', 'portasm.c'} and
                 Path(name).parent != directory}
    if competing:
        raise ValueError('Actual graph has a contradictory kernel port source')


def check_probes(variant: dict, text: str) -> None:
    facts = variant['facts']['cpu']
    if facts['fpu'] != 'none':
        try:
            body = function_body(text, 'nexus_runtime_float_probe')
        except ValueError as error:
            raise ValueError('Missing actual floating probe') from error
        if not re.search(r'\bvadd\.f32\b', body):
            raise ValueError('Missing actual floating probe instruction')
    if facts['mve'] != 'none':
        body = function_body(text, 'nexus_runtime_vector_probe')
        if not re.search(r'\bvadd\.i32\b', body):
            raise ValueError('Missing actual MVE integer probe instruction')
    if facts['mve'] == 'float':
        body = function_body(text, 'nexus_runtime_vector_float_probe')
        if not re.search(r'\bvadd\.f32\b[^\n]*\bq[0-7]\b', body):
            raise ValueError('Missing actual MVE floating probe instruction')


def compiled_objects(root: Path, build: Path, variant: dict) -> list[dict]:
    entries = json.loads((build / 'compile_commands.json').read_text())
    result = []
    for entry in entries:
        argv = entry.get('arguments') or shlex.split(entry['command'])
        if '-o' not in argv:
            raise ValueError('Actual CMake compile entry lacks object output')
        obj = Path(entry['directory']) / argv[argv.index('-o') + 1]
        if not obj.is_file():
            continue  # EXCLUDE_FROM_ALL targets are not built consumers.
        for option in variant['profile']['compile_options']:
            if option not in argv:
                raise ValueError(f'Actual object lost CPU ABI option: {option}')
        abi_option = re.compile(
            r'^(?:-m(?:cpu|arch|fpu|float-abi|abi)=.*|'
            r'-m(?:thumb|arm|cmse|no-cmse)|-f(?:no-)?short-enums)$')
        actual_abi = sorted(arg for arg in argv if abi_option.fullmatch(arg))
        canonical_abi = sorted(arg for arg in
                               variant['profile']['compile_options']
                               if abi_option.fullmatch(arg))
        if actual_abi != canonical_abi:
            raise ValueError('Actual object has competing or duplicate CPU ABI options')
        result.append({'source': compiler_gate.digest(Path(entry['file'])),
                       'object': compiler_gate.digest(obj), 'argv': argv})
    required = {'core/src/request.c', 'core/src/time.c',
                'arch/cortex_m/nx_arch_cortex_m.c',
                'arch/cortex_m/nx_arch_cache.c',
                'arch/cortex_m/nx_arch_mpu.c',
                'arch/cortex_m/nx_arch_security.c',
                'arch/cortex_m/nx_arch_sleep.c', 'os/wait.c'}
    sources = set()
    generated_port = build / 'nexus-runtime/os/freertos/m7-integer-port/port.c'
    for item in result:
        source = Path(item['source']['path'])
        if source.is_relative_to(root):
            sources.add(source.relative_to(root).as_posix())
        elif (variant['profile']['irq']['kernel_port'] ==
              'nexus/ARM_CM7_integer' and source == generated_port):
            item['generated_by'] = compiler_gate.digest(
                root / 'os/freertos/prepare_m7_integer.py') if (
                    root / 'os/freertos/prepare_m7_integer.py').is_file() else None
        else:
            raise ValueError('Actual Runtime graph contains an unexpected external source')
    if not required <= sources:
        raise ValueError('Production Runtime source closure is incomplete')
    return result


def execute_variant(root: Path, recorder, selected_tools, variant: dict) -> dict:
    directory = recorder.output / variant['name']
    assembly = write_inputs(directory / 'inputs', variant)
    build = directory / 'build'
    cmake = shutil.which('cmake')
    ninja = shutil.which('ninja')
    if cmake is None or ninja is None:
        raise ValueError('Actual CMake and Ninja tools are required')
    recorder.execute([
        cmake, '-S', str(root / 'tests/contracts/cortex_runtime'), '-B',
        str(build), '-G', 'Ninja',
        '-DCMAKE_TOOLCHAIN_FILE=' + str(root / 'cmake/toolchains/arm-gcc.cmake'),
        '-DCMAKE_C_COMPILER=' + selected_tools['gcc'],
        '-DPython3_EXECUTABLE=' + sys.executable,
        '-DNEXUS_RUNTIME_ASSEMBLY=' + str(assembly)],
        variant['name'] + '-configure')
    recorder.execute([cmake, '--build', str(build), '--parallel', '2',
                      '--target', 'nexus_cortex_runtime'],
                     variant['name'] + '-build')
    elf = build / 'nexus_cortex_runtime.elf'
    generated = build / 'nexus-runtime/generated/resolved-cpu.json'
    resolved = json.loads(generated.read_text())
    if resolved['cpu_profile'] != variant['profile']:
        raise ValueError('Actual Runtime resolution differs from authored facts')
    compiler_gate.check_arch_symbols(recorder.execute(
        [selected_tools['nm'], '-u', str(elf)], variant['name'] + '-undefined'))
    names = recorder.execute([selected_tools['nm'], '--defined-only', str(elf)],
                             variant['name'] + '-symbols', nonempty=True)
    check_retention(names, variant['backend'])
    check_runtime_dependencies(names)
    text = recorder.execute([selected_tools['objdump'], '-d', str(elf)],
                            variant['name'] + '-disassembly', nonempty=True)
    check_probes(variant, text)
    if variant['backend'] == 'freertos':
        check_kernel_context(variant, text)
        check_coprocessor_initialization(variant, text)
    attributes = recorder.execute([selected_tools['readelf'], '-A', str(elf)],
                                  variant['name'] + '-attributes', nonempty=True)
    hard = bool(re.search(r'Tag_ABI_VFP_args:\s*VFP registers', attributes))
    if hard != (variant['facts']['cpu']['float_abi'] == 'hard'):
        raise ValueError('Linked Runtime ELF has unexpected floating ABI')
    objects = compiled_objects(root, build, variant)
    check_kernel_sources(root, build, variant, objects)
    return {'name': variant['name'], 'cpu': variant['facts']['cpu']['arch'],
            'backend': variant['backend'], 'status': 'runtime_link_passed',
            'full_platform_status': 'not_qualified',
            'physical_status': 'not_executed',
            'facts': compiler_gate.digest(assembly.parent / 'cpu.json'),
            'assembly': compiler_gate.digest(assembly),
            'resolved': compiler_gate.digest(generated),
            'elf': compiler_gate.digest(elf),
            'map': compiler_gate.digest(build / 'nexus_cortex_runtime.map'),
            'database': compiler_gate.digest(build / 'compile_commands.json'),
            'compiled_objects': objects,
            'retained_api_count': len(required_symbols(variant['backend'])),
            'kernel_port': variant['profile']['irq']['kernel_port']}


def run(root: Path, compiler: str, output: Path) -> int:
    root = root.resolve()
    recorder = compiler_gate.Recorder(output)
    report = {'schema': 1, 'status': 'failed',
              'scope': 'production_runtime_cpu_software_reference_matrix',
              'full_platform_status': 'not_qualified',
              'physical_status': 'not_executed', 'variants': [],
              'commands': recorder.commands}
    try:
        selected_tools = compiler_gate.tools(compiler)
        for name, path in selected_tools.items():
            recorder.execute([path, '--version'], name + '-version', nonempty=True)
        report['tools'] = {name: compiler_gate.digest(Path(path))
                           for name, path in selected_tools.items()}
        report['fixture'] = {
            name: compiler_gate.digest(root / 'tests/contracts/cortex_runtime' / name)
            for name in ('CMakeLists.txt', 'main.c', 'linker.ld')}
        report['runtime_entry'] = compiler_gate.digest(
            root / 'cmake/platform/Runtime.cmake')
        report['source_inputs'] = source_inputs(root)
        expected = matrix()
        report['expected_variants'] = len(expected)
        for variant in expected:
            report['variants'].append(execute_variant(
                root, recorder, selected_tools, variant))
            verify_source_inputs(report['source_inputs'])
            (recorder.output / 'report.json').write_text(
                json.dumps(report, indent=2, sort_keys=True) + '\n',
                encoding='utf-8')
        report['source_inputs_stable'] = True
        report['status'] = 'passed'
    except (OSError, UnicodeError, ValueError, KeyError) as error:
        report['error'] = str(error)
    (recorder.output / 'report.json').write_text(
        json.dumps(report, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    return 0 if report['status'] == 'passed' else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument('--compiler', default='arm-none-eabi-gcc')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if compiler is not None:
        os.environ['PATH'] = str(Path(compiler).parent) + os.pathsep + (
            os.environ.get('PATH', ''))
    return run(args.root, args.compiler, args.output)


if __name__ == '__main__':
    raise SystemExit(main())
