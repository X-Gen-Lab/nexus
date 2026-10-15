"""Compile the reviewed M7 cold-cache leaf and verify its machine-code contract.

This is a software instruction/ABI gate, not execution on an MCU or cache HIL.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shlex

import arch_compile


LEAF = 'nx_arch_cache_clean_disable_leaf'
EXPORTS = {'nx_arch_cache_cold_enable', 'nx_arch_cache_cold_disable'}
BOUNDARY = {'nx_arch_is_privileged', 'nx_arch_exception_number',
            'nx_arch_irq_masks', 'nx_arch_dsb', 'nx_arch_isb'}


def check_leaf(disassembly: str) -> None:
    """Reject an unreviewed stack, call, loop or barrier instruction sequence."""
    bodies = re.findall(r'^([0-9a-f]+) <' + LEAF + r'>:\n(.*?)(?=^\w+ <|\Z)',
                        disassembly, re.MULTILINE | re.DOTALL)
    if len(bodies) != 1:
        raise ValueError('one exact reviewed cache leaf must be present')
    instructions = []
    for line in bodies[0][1].splitlines():
        match = re.fullmatch(
            r'\s*([0-9a-f]+):\s+(?:[0-9a-f]{4}\s+){1,2}'
            r'([a-z][a-z.0-9]*)\s*(.*?)\s*', line)
        if not match:
            if line.strip():
                raise ValueError('unrecognized cache leaf disassembly')
            continue
        address, operation, operands = match.groups()
        operation = operation.split('.')[0]
        operands = re.sub(r'\s+', ' ', operands.split('@')[0].strip())
        instructions.append((int(address, 16), operation, operands))
        if operation == 'bx' and operands == 'lr':
            break
    # MOVW zero-extends its immediate; it does not retain a previous MOVT.
    # Track the actual address construction before validating the fixed leaf
    # sequence so a missing high half cannot write an unrelated memory alias.
    r2 = None
    for _, opcode, operand in instructions:
        immediate = re.fullmatch(r'r2, #(0x[0-9a-f]+|[0-9]+)', operand)
        if opcode == 'movw' and immediate:
            r2 = int(immediate[1], 0) & 0xffff
        elif opcode == 'movt' and immediate:
            if r2 is None:
                raise ValueError('cache MMIO address has no initialized low half')
            r2 = (r2 & 0xffff) | ((int(immediate[1], 0) & 0xffff) << 16)
        elif opcode in ('str', 'ldr') and '[r2, #0]' in operand:
            wanted = 0xe000ef74 if opcode == 'str' and operand.startswith(
                'r0,') else 0xe000ed14
            if r2 != wanted:
                raise ValueError('cache MMIO address construction is incorrect')
    expected = [
        ('dsb', 'sy'), ('mov', 'ip, r0'), ('movw', 'r2, #61300'),
        ('movt', 'r2, #57344'), ('mov', 'r3, #3221225472'),
        ('orr', 'r0, ip, r3'), ('str', 'r0, [r2, #0]'),
        ('subs', 'r3, r3, #1073741824'), ('bcs', None),
        ('subs', 'ip, ip, #32'), ('bpl', None), ('dsb', 'sy'),
        ('movw', 'r2, #60692'), ('movt', 'r2, #57344'),
        ('str', 'r1, [r2, #0]'),
        ('dsb', 'sy'), ('isb', 'sy'), ('ldr', 'r0, [r2, #0]'), ('bx', 'lr')]
    if len(instructions) != len(expected):
        raise ValueError('cache leaf instruction count changed')
    for (_, opcode, operand), (wanted_opcode, wanted_operand) in zip(
            instructions, expected):
        if opcode != wanted_opcode or (wanted_operand is not None and
                                        operand != wanted_operand):
            raise ValueError('unreviewed cache leaf instruction or operand')
    for branch, destination in ((8, 5), (10, 4)):
        target = re.fullmatch(r'([0-9a-f]+) <[^>]+>', instructions[branch][2])
        if not target or int(target[1], 16) != instructions[destination][0]:
            raise ValueError('cache leaf loop destination changed')


def dependencies(path: Path) -> tuple[Path, ...]:
    """Resolve the compiler's actual non-system dependency closure."""
    text = path.read_text(encoding='utf-8').replace('\\\n', '')
    if ': ' not in text:
        raise ValueError('compiler dependency output is malformed')
    values = tuple(Path(value).resolve() for value in
                   shlex.split(text.split(': ', 1)[1]))
    if not values or any(not value.is_file() for value in values):
        raise ValueError('compiler input dependency is absent')
    return tuple(sorted(set(values)))


def run(root: Path, compiler: str, output: Path) -> int:
    root = root.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError('cache compiler evidence output must be empty')
    recorder = arch_compile.Recorder(output)
    report = {'schema': 1, 'status': 'failed', 'scope': 'm7_cold_cache_leaf',
              'physical_status': 'not_executed', 'profiles': [],
              'commands': recorder.commands}
    inputs = {}
    try:
        selected = arch_compile.tools(compiler)
        report['tools'] = {key: arch_compile.digest(Path(value))
                           for key, value in selected.items()}
        for key, value in selected.items():
            recorder.execute([value, '--version'], key + '-version', nonempty=True)
        source = root / 'arch/cortex_m/nx_arch_cache_control.c'
        for path in [source, Path(__file__),
                     root / 'arch/cortex_m/private/cache_control.h',
                     root / 'arch/cortex_m/private/mechanisms.h',
                     root / 'arch/include/nexus/arch/cache_control.h',
                     root / 'arch/include/nexus/arch/arch.h',
                     root / 'arch/include/nexus/arch/features.h']:
            inputs[str(path.resolve())] = arch_compile.digest(path)
        variants = [('cortex-m7', optimization, True)
                    for optimization in ('O0', 'Os', 'O2')]
        variants.extend((cpu, 'O2', False) for cpu in arch_compile.PROFILES
                        if cpu != 'cortex-m7')
        variants.append(('cortex-m7', 'O2', False))
        for cpu, optimization, reviewed in variants:
            name = f'{cpu}-{optimization}-' + ('reviewed' if reviewed else 'absent')
            obj = recorder.output / (name + '.o')
            depfile = recorder.output / (name + '.d')
            facts = arch_compile.reference_facts(cpu)
            if cpu == 'cortex-m7' and not reviewed:
                facts['cpu']['dcache_line_bytes'] = 0
                facts['cpu']['icache_line_bytes'] = 0
            line_bytes = facts['cpu']['dcache_line_bytes']
            resolved = arch_compile.cpu_contract.resolve(facts['cpu'],
                                                        facts['irq'], 'baremetal')
            flags = [*resolved.compile_options, '-std=c11', '-' + optimization,
                     '-Wall', '-Wextra', '-Werror', '-Wpedantic', '-Wconversion',
                     '-Wshadow', '-I' + str(root / 'arch/include'),
                     '-DNEXUS_ARCH_CACHE_CONTROL_M7=' + str(int(cpu == 'cortex-m7')),
                     '-DNEXUS_ARCH_DCACHE_LINE_BYTES=' + str(line_bytes),
                     '-DNEXUS_ARCH_ICACHE_LINE_BYTES=' + str(line_bytes),
                     '-DNEXUS_ARCH_MPU_VERSION=0', '-DNEXUS_ARCH_SECURITY_STATE=0',
                     '-DNEXUS_ARCH_HAS_SAU=0', '-DNEXUS_ARCH_HAS_DWT_CYCCNT=0']
            recorder.execute([selected['gcc'], *flags, '-MMD', '-MF', str(depfile),
                              '-c', str(source), '-o', str(obj)], name + '-compile')
            for path in dependencies(depfile):
                actual = arch_compile.digest(path)
                if str(path) not in inputs or actual != inputs[str(path)]:
                    raise ValueError('unexpected or changed cache compiler input')
            undefined = arch_compile.undefined_symbols(recorder.execute(
                [selected['nm'], '-u', str(obj)], name + '-undefined'))
            if undefined != (BOUNDARY if reviewed else set()):
                raise ValueError('cache control has unexpected runtime dependencies')
            exports = recorder.execute([selected['nm'], '--defined-only',
                                        '--extern-only', str(obj)], name + '-exports',
                                       nonempty=True)
            if {line.split()[-1] for line in exports.splitlines()} != EXPORTS:
                raise ValueError('cache control public symbol surface changed')
            assembly = recorder.execute([selected['objdump'], '-d', str(obj)],
                                         name + '-disassembly', nonempty=True)
            if reviewed:
                check_leaf(assembly)
            elif LEAF in assembly:
                raise ValueError('unsupported CPU contains a cold cache leaf')
            report['profiles'].append({'name': name, 'status': 'passed',
                                       'object': arch_compile.digest(obj),
                                       'dependencies': arch_compile.digest(depfile)})
        if any(arch_compile.digest(Path(path)) != identity
               for path, identity in inputs.items()):
            raise ValueError('cache source changed during compiler evidence capture')
        if any(arch_compile.digest(Path(identity['path'])) != identity
               for identity in report['tools'].values()):
            raise ValueError('cache compiler tool changed during execution')
        report['inputs'] = list(inputs.values())
        report['status'] = 'passed'
    except (OSError, UnicodeError, ValueError) as error:
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
    return run(args.root, args.compiler, args.output)


if __name__ == '__main__':
    raise SystemExit(main())
