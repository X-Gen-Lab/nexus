"""Measure production OS objects and retained ARM instructions, never cycles.

One authored Runtime TOML selects each CPU, kernel policy and optimization.
Object symbols, actual Idle resources, linker maps and disassembly establish
software costs. No fixture is Board startup or physical timing acceptance.
"""
from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import re
import shutil
import sys

import arch_compile as compiler_gate
import cortex_runtime
import os_profiles
from tools.configure import cpu, kernel


OBJECTS = frozenset({
    'tcb', 'wait_port', 'notify', 'direct_notify', 'raw_queue',
    'closable_queue', 'queue_waiter', 'joinable_task', 'permanent_task',
})
STORAGE = OBJECTS | {
    'join_stack', 'permanent_stack', 'raw_payload', 'closable_payload',
}
PUBLIC_HOT_PATHS = (
    'nx_freertos_queue_send', 'nx_freertos_queue_receive',
    'nx_freertos_queue_send_until', 'nx_freertos_queue_receive_until',
    'nx_freertos_task_start', 'nx_freertos_task_join',
    'nx_freertos_permanent_task_start', 'nx_freertos_closable_queue_close',
    'nx_freertos_closable_queue_send_until',
    'nx_freertos_closable_queue_receive_until',
)


def matrix() -> tuple[dict, ...]:
    """Reuse the reviewed policy resolver rather than configure kernel macros."""
    baseline = {item['choices']['profile']: item for item in os_profiles.matrix()
                if item['name'] in {'standard', 'minimal'}}
    result = []
    for arch in ('cortex-m0', 'cortex-m4'):
        for policy in ('standard', 'minimal'):
            for optimization in ('Os', 'O2'):
                item = copy.deepcopy(baseline[policy])
                if arch == 'cortex-m0':
                    item['facts'] = compiler_gate.reference_facts(arch)
                    profile = cpu.resolve(item['facts']['cpu'],
                                          item['facts']['irq'], 'freertos')
                    selected = kernel.resolve(item['choices'], profile,
                                              item['facts']['clock_hz'])
                    item['profile'] = kernel.bind(profile, selected).to_dict()
                    item['kernel'] = selected.to_dict()
                item['name'] = f'{arch}-{policy}-{optimization}'
                item['optimization'] = optimization
                result.append(item)
    return tuple(result)


def write_inputs(directory: Path, variant: dict) -> Path:
    assembly = os_profiles.write_inputs(directory, variant)
    text = assembly.read_text(encoding='utf-8')
    if text.count('optimization = "Os"') != 1:
        raise ValueError('Optimization must have exactly one authored authority')
    text = text.replace('optimization = "Os"',
                        'optimization = ' + json.dumps(variant['optimization']))
    assembly.write_text(text, encoding='utf-8')
    return assembly


def storage_symbols(text: str) -> dict[str, int]:
    result = {}
    for size, name in re.findall(
            r'^\s*[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+[bBdD]\s+'
            r'nx_os_(\w+)__bytes\s*$', text, re.MULTILINE):
        if name in result or int(size, 16) <= 0:
            raise ValueError('Duplicate or empty actual OS storage symbol')
        result[name] = int(size, 16)
    return result


def idle_symbols(text: str) -> dict[str, int]:
    result = {}
    for name, pattern in (('tcb', r'xIdleTaskTCB(?:\.\d+)?'),
                          ('stack', r'uxIdleTaskStack(?:\.\d+)?')):
        matches = re.findall(r'^\s*[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+'
                             r'[bBdD]\s+' + pattern + r'\s*$',
                             text, re.MULTILINE)
        if len(matches) != 1 or int(matches[0], 16) <= 0:
            raise ValueError('Missing or ambiguous actual kernel Idle storage')
        result[name] = int(matches[0], 16)
    return result


def resource_accounting(sizes: dict, storage: dict, idle: dict,
                        policy: dict) -> dict:
    if (not OBJECTS <= sizes.keys() or not STORAGE <= storage.keys() or
            sizes.get('stack_word') != 4 or sizes.get('semaphore', 0) <= 0):
        raise ValueError('Incomplete actual target-ABI storage identities')
    for name in OBJECTS:
        if sizes[name] != storage[name] or sizes[name] <= 0:
            raise ValueError('Actual storage differs from sizeof: ' + name)
    tcb, latch = sizes['tcb'], sizes['semaphore']
    expected = {'wait_port': 16, 'notify': latch + 12,
                'direct_notify': 16, 'raw_queue': latch + 8,
                'closable_queue': latch + 24, 'queue_waiter': latch + 12,
                'joinable_task': tcb + latch + 16, 'permanent_task': tcb + 4}
    if any(sizes[name] != value for name, value in expected.items()):
        raise ValueError('Actual OS resource formula differs from public storage')
    if (storage['join_stack'] != 128 * sizes['stack_word'] or
            storage['permanent_stack'] != 128 * sizes['stack_word'] or
            storage['raw_payload'] != 3 * 4 or
            storage['closable_payload'] != 3 * 4):
        raise ValueError('Actual caller stack or payload storage is inconsistent')
    if (idle['tcb'] != tcb or
            idle['stack'] != policy['idle_stack_words'] * sizes['stack_word']):
        raise ValueError('Actual Idle storage disagrees with authored kernel policy')
    return {'probe_storage_total': sum(storage.values()),
            'idle_total': idle['tcb'] + idle['stack'],
            'direct_notify_supported': policy.get('task_notifications', True),
            'direct_notify_saved': sizes['notify'] - sizes['direct_notify']
            if policy.get('task_notifications', True) else None,
            'permanent_task_saved': sizes['joinable_task'] - sizes['permanent_task'],
            'closable_queue_increment': sizes['closable_queue'] - sizes['raw_queue'],
            'waiter_increment_per_concurrent_call': sizes['queue_waiter'],
            'scope': 'actual_32bit_abi_and_identical_software_consumer',
            'physical_status': 'not_executed'}


def instruction_summary(body: str) -> dict:
    nodes, calls, masks = [], [], []
    for line in body.splitlines():
        match = re.match(r'\s*([0-9a-f]+):\s+(?:[0-9a-f]{4,8}\s+)+'
                         r'([a-z][a-z0-9.]*)\s*(.*)', line)
        if not match:
            continue
        address, mnemonic, operands = match.groups()
        operation = mnemonic.split('.')[0]
        target = re.search(r'([0-9a-fA-F]+)\s+<([^>]+)>', operands)
        nodes.append({'address': int(address, 16), 'operation': operation,
                      'operands': operands,
                      'target_address': int(target.group(1), 16) if target else None,
                      'target': target.group(2).split('+', 1)[0] if target else None})
        if operation in {'cpsid', 'cpsie', 'mrs', 'msr', 'dsb', 'isb', 'dmb'}:
            masks.append(mnemonic + ' ' + operands.split('@', 1)[0].strip())
    if not nodes:
        raise ValueError('Actual retained instruction body is empty')
    addresses = {node['address'] for node in nodes}
    for node in nodes:
        if (node['operation'] in {'bl', 'blx'} or
                node['operation'] == 'b' and
                node['target_address'] not in addresses):
            if node['target']:
                calls.append(node['target'])
    return {'instructions': len(nodes), 'calls': calls,
            'mask_and_barrier_instructions': masks,
            'scope': 'static_instruction_shape_not_runtime_cycles',
            '_instructions': nodes}


def check_baseline_restore(summary: dict) -> None:
    """Follow both local branch alternatives; linear call counts are unsound."""
    if '_instructions' not in summary:
        borrowed = 0
        for name in summary['calls']:
            if name == 'nx_arch_irq_save':
                borrowed += 1
            elif name == 'nx_arch_irq_restore':
                borrowed -= 1
            elif name.startswith(('xQueue', 'xTaskGenericNotify',
                                  'ulTaskGenericNotify')) and borrowed:
                raise ValueError('Baseline must restore borrowed mask before kernel call')
            if borrowed < 0:
                raise ValueError('Baseline IRQ restore has no preceding save')
        if borrowed:
            raise ValueError('Baseline did not restore borrowed IRQ mask')
        return
    nodes = summary['_instructions']
    lookup = {node['address']: index for index, node in enumerate(nodes)}
    if len(lookup) != len(nodes):
        raise ValueError('Actual instruction addresses are ambiguous')
    conditional = {'beq', 'bne', 'bcs', 'bhs', 'bcc', 'blo', 'bmi', 'bpl',
                   'bvs', 'bvc', 'bhi', 'bls', 'bge', 'blt', 'bgt', 'ble',
                   'cbz', 'cbnz'}
    work, seen = [(0, False)], set()
    while work:
        index, borrowed = work.pop()
        if (index, borrowed) in seen:
            continue
        seen.add((index, borrowed))
        node = nodes[index]
        operation, target = node['operation'], node['target']
        linked_call = operation in {'bl', 'blx'}
        if linked_call:
            if target == 'nx_arch_irq_save':
                if borrowed:
                    raise ValueError('Baseline must restore before a second borrowed save')
                borrowed = True
            elif target == 'nx_arch_irq_restore':
                if not borrowed:
                    raise ValueError('Baseline IRQ restore has no preceding save')
                borrowed = False
            elif borrowed and (target is None or target.startswith(
                    ('xQueue', 'xTaskGenericNotify', 'ulTaskGenericNotify'))):
                raise ValueError('Baseline must restore borrowed mask before kernel call')
        returns = (operation == 'bx' or
                   operation in {'pop', 'ldm', 'ldmia'} and
                   re.search(r'\bpc\b', node['operands']))
        if returns:
            if borrowed:
                raise ValueError('Baseline return did not restore borrowed IRQ mask')
            continue
        if operation in {'tbb', 'tbh'}:
            raise ValueError('Baseline mask audit cannot resolve a computed branch')
        branch = operation == 'b' or operation in conditional
        if branch:
            destination = lookup.get(node['target_address'])
            if destination is None:
                if borrowed:
                    raise ValueError('Baseline external branch did not restore IRQ mask')
            else:
                work.append((destination, borrowed))
            if operation == 'b':
                continue
        if index + 1 < len(nodes):
            work.append((index + 1, borrowed))
        elif borrowed:
            raise ValueError('Baseline body ended without IRQ restore')


def check_static_image(text: str) -> None:
    cortex_runtime.check_runtime_dependencies(text)
    names = cortex_runtime.symbols(text)
    forbidden = {'malloc', 'calloc', 'realloc', 'free', 'pvPortMalloc', 'vPortFree'}
    if names & forbidden:
        raise ValueError('Dynamic allocation entered the static OS image')


def retained_body(text: str, name: str) -> str:
    match = re.search(rf'<{re.escape(name)}>:\n(.*?)(?=\n[^\n]*<[^>]+>:\n|\Z)',
                      text, flags=re.DOTALL)
    if not match:
        raise ValueError('Retained OS function body missing: ' + name)
    return match.group(1)


def hot_paths(disassembly: str, baseline: bool, notifications: bool) -> dict:
    result = {}
    labels = re.findall(r'^[0-9a-f]+ <([^>]+)>:\s*$', disassembly,
                        re.MULTILINE)
    for name in PUBLIC_HOT_PATHS:
        result[name] = instruction_summary(
            retained_body(disassembly, name))
    for prefix in ('notify_wake', 'notify_wait', 'closable_transfer',
                   'queue_until', 'queue_broadcast', 'task_context_allowed',
                   'deadline_ticks', 'nx_atomic_u32_',
                   'vTaskSuspendAll', 'xTaskResumeAll',
                   'vPortEnterCritical', 'vPortExitCritical'):
        for name in labels:
            if name.startswith(prefix) and name not in result:
                result[name] = instruction_summary(
                    retained_body(disassembly, name))
    if notifications:
        for prefix in ('direct_wake', 'direct_wait'):
            matches = [name for name in labels if name.startswith(prefix)]
            if not matches:
                raise ValueError('The retained direct notification path is missing')
            for name in matches:
                result[name] = instruction_summary(
                    retained_body(disassembly, name))
    if baseline:
        for summary in result.values():
            check_baseline_restore(summary)
        save = retained_body(disassembly, 'nx_arch_irq_save').lower()
        restore = retained_body(disassembly, 'nx_arch_irq_restore').lower()
        if not (re.search(r'\bmrs\b[^\n]*\bprimask\b', save) and
                re.search(r'\bcpsid\s+i\b', save) and
                re.search(r'\bmsr\b[^\n]*\bprimask\b', restore)):
            raise ValueError('Baseline actual save/restore primitives are missing')
        if re.search(r'\bcpsie\s+i\b', restore):
            raise ValueError('Baseline restore unexpectedly globally unmasks IRQ')
    for summary in result.values():
        summary.pop('_instructions')
    return result


def profile_comparisons(variants: list[dict]) -> list[dict]:
    grouped = {}
    for item in variants:
        key = (item['cpu'], item['optimization'])
        group = grouped.setdefault(key, {})
        if item['profile'] in group:
            raise ValueError('Duplicate resource CPU/optimization/profile identity')
        group[item['profile']] = item
    result = []
    for (arch, optimization), group in sorted(grouped.items()):
        if group.keys() != {'standard', 'minimal'}:
            raise ValueError('Resource comparison requires the same CPU and optimization')
        standard = group['standard']['resources']
        minimal = group['minimal']['resources']
        saved = standard['image']['static_ram'] - minimal['image']['static_ram']
        tcb = standard['sizes']['tcb'] - minimal['sizes']['tcb']
        if saved <= 0 or tcb <= 0:
            raise ValueError('Minimal profile has no measured RAM/TCB savings')
        result.append({'cpu': arch, 'optimization': optimization,
                       'fixture_ram_saved': saved, 'per_tcb_saved': tcb,
                       'physical_status': 'not_executed'})
    return result


def source_inputs(root: Path) -> list[dict]:
    result = {item['path']: item for item in os_profiles.source_inputs(root)}
    for path in (Path(__file__), root / 'tests/contracts/os_resources/main.c',
                 root / 'tests/contracts/os_resources/CMakeLists.txt',
                 root / 'tests/contracts/os_resources/linker.ld'):
        identity = compiler_gate.digest(path)
        result[identity['path']] = identity
    return [result[name] for name in sorted(result)]


def check_optimization(objects: list[dict], optimization: str) -> None:
    if not objects:
        raise ValueError('Actual resource object closure is empty')
    for item in objects:
        choices = [arg for arg in item['argv'] if re.fullmatch(
            r'-O(?:0|1|2|3|s|g|z|fast)', arg)]
        if choices != ['-' + optimization] or any(
                arg.startswith('-flto') for arg in item['argv']):
            raise ValueError('Actual object has a competing optimization or LTO policy')


def execute_variant(root: Path, recorder, selected_tools: dict,
                    variant: dict) -> dict:
    directory = recorder.output / variant['name']
    assembly = write_inputs(directory / 'inputs', variant)
    build = directory / 'build'
    cmake, ninja = shutil.which('cmake'), shutil.which('ninja')
    if cmake is None or ninja is None:
        raise ValueError('Actual CMake and Ninja executables are required')
    recorder.execute([
        cmake, '-S', str(root / 'tests/contracts/os_resources'), '-B', str(build),
        '-G', 'Ninja', '-DCMAKE_TOOLCHAIN_FILE=' +
        str(root / 'cmake/toolchains/arm-gcc.cmake'),
        '-DCMAKE_C_COMPILER=' + selected_tools['gcc'],
        '-DPython3_EXECUTABLE=' + sys.executable,
        '-DNEXUS_RUNTIME_ASSEMBLY=' + str(assembly)],
        variant['name'] + '-configure')
    recorder.execute([cmake, '--build', str(build), '--parallel', '2',
                      '--target', 'nexus_os_resources'],
                     variant['name'] + '-build')
    resolved_path = build / 'nexus-runtime/generated/resolved-cpu.json'
    resolved = json.loads(resolved_path.read_text(encoding='utf-8'))
    if (resolved['cpu_profile'] != variant['profile'] or
            resolved['kernel_profile'] != variant['kernel']):
        raise ValueError('Actual resource runtime disagrees with authored policy')
    elf = build / 'nexus_os_resources.elf'
    compiler_gate.check_arch_symbols(recorder.execute(
        [selected_tools['nm'], '-u', str(elf)], variant['name'] + '-undefined'))
    symbols = recorder.execute([selected_tools['nm'], '-S', '--defined-only',
                                str(elf)], variant['name'] + '-symbols',
                               nonempty=True)
    check_static_image(symbols)
    os_profiles.check_optional_symbols(variant, symbols)
    sizes = os_profiles.object_sizes(symbols)
    storage, idle = storage_symbols(symbols), idle_symbols(symbols)
    accounting = resource_accounting(sizes, storage, idle, variant['kernel'])
    image = os_profiles.image_size(recorder.execute(
        [selected_tools['size'], str(elf)], variant['name'] + '-size',
        nonempty=True))
    accounted = accounting['probe_storage_total'] + accounting['idle_total']
    if accounted > image['static_ram']:
        raise ValueError('Named OS storage exceeds the actual RAM section totals')
    accounting['other_fixture_and_kernel_ram'] = image['static_ram'] - accounted
    disassembly = recorder.execute([selected_tools['objdump'], '-d', str(elf)],
                                   variant['name'] + '-disassembly', nonempty=True)
    paths = hot_paths(disassembly, variant['profile']['irq']['mask_kind'] == 'primask',
                     variant['kernel']['task_notifications'])
    cortex_runtime.check_kernel_context(variant, disassembly)
    cortex_runtime.check_coprocessor_initialization(variant, disassembly)
    objects = cortex_runtime.compiled_objects(root, build, variant)
    check_optimization(objects, variant['optimization'])
    cortex_runtime.check_kernel_sources(root, build, variant, objects)
    return {'name': variant['name'], 'status': 'resource_and_instruction_passed',
            'cpu': variant['facts']['cpu']['arch'],
            'optimization': variant['optimization'],
            'profile': variant['kernel']['profile'],
            'kernel_port': variant['profile']['irq']['kernel_port'],
            'resources': {'sizes': sizes, 'storage': storage, 'idle': idle,
                          'accounting': accounting, 'image': image},
            'hot_paths': paths, 'compiled_objects': objects,
            'assembly': compiler_gate.digest(assembly),
            'facts': compiler_gate.digest(assembly.parent / 'cpu.json'),
            'resolved': compiler_gate.digest(resolved_path),
            'elf': compiler_gate.digest(elf),
            'map': compiler_gate.digest(build / 'nexus_os_resources.map'),
            'database': compiler_gate.digest(build / 'compile_commands.json'),
            'physical_status': 'not_executed',
            'cycles_status': 'not_measured'}


def run(root: Path, compiler: str, output: Path) -> int:
    root, output = root.resolve(), output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError('OS resource qualification output must be fresh')
    recorder = compiler_gate.Recorder(output)
    report = {'schema': 1, 'status': 'failed',
              'scope': 'production_os_resource_and_retained_instruction_matrix',
              'physical_status': 'not_executed', 'cycles_status': 'not_measured',
              'variants': [], 'commands': recorder.commands}
    try:
        selected_tools = compiler_gate.tools(compiler)
        selected_tools['size'] = str(
            Path(selected_tools['gcc']).with_name('arm-none-eabi-size'))
        for name in ('cmake', 'ninja'):
            executable = shutil.which(name)
            if executable is None:
                raise ValueError('Actual build tool is missing: ' + name)
            selected_tools[name] = executable
        selected_tools['python'] = sys.executable
        for name, path in selected_tools.items():
            recorder.execute([path, '--version'], name + '-version',
                             nonempty=True)
        report['tools'] = {name: compiler_gate.digest(Path(path))
                           for name, path in selected_tools.items()}
        report['source_inputs'] = source_inputs(root)
        variants = matrix()
        report['expected_variants'] = len(variants)
        for variant in variants:
            report['variants'].append(execute_variant(
                root, recorder, selected_tools, variant))
            cortex_runtime.verify_source_inputs(report['source_inputs'])
            (output / 'report.json').write_text(
                json.dumps(report, indent=2, sort_keys=True) + '\n')
        report['comparisons'] = profile_comparisons(report['variants'])
        if any(compiler_gate.digest(Path(selected_tools[name])) != identity
               for name, identity in report['tools'].items()):
            raise ValueError('Actual OS resource tool changed during execution')
        report['tools_stable'] = True
        report['source_inputs_stable'] = True
        report['status'] = 'passed'
    except (OSError, ValueError, KeyError, UnicodeError) as error:
        report['error'] = str(error)
    (output / 'report.json').write_text(
        json.dumps(report, indent=2, sort_keys=True) + '\n')
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
        os.environ['PATH'] = (str(Path(compiler).parent) + os.pathsep +
                              os.environ.get('PATH', ''))
    return run(args.root, args.compiler, args.output)


if __name__ == '__main__':
    raise SystemExit(main())
