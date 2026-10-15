"""Measure linked Wait ABI models; no cycle, product or hardware qualification."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import arch_compile


def parse_sizes(text: str) -> dict:
    """Use one actual GNU size row; initialized data consumes both memories."""
    lines = text.strip().splitlines()
    if len(lines) != 2 or lines[0].split() != [
            'text', 'data', 'bss', 'dec', 'hex', 'filename']:
        raise ValueError('invalid or empty GNU size output')
    fields = lines[1].split(maxsplit=5)
    if len(fields) != 6:
        raise ValueError('invalid GNU size measurement row')
    code, data, bss, decimal = map(int, fields[:4])
    hexadecimal = int(fields[4], 16)
    if (min(code, data, bss) <= 0 or
            code + data + bss != decimal or decimal != hexadecimal):
        raise ValueError('inconsistent or empty GNU size measurement')
    return {'text_bytes': code, 'data_bytes': data, 'bss_bytes': bss,
            'flash_bytes': code + data, 'ram_bytes': data + bss}


def parse_symbols(text: str) -> dict:
    """A stripped image cannot prove face or readonly table allocation."""
    symbols = {}
    for line in text.splitlines():
        match = re.fullmatch(
            r'[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+([A-Za-z])\s+(\S+)', line)
        if not match:
            raise ValueError('invalid or stripped GNU nm measurement')
        size, kind, name = match.groups()
        if name in symbols:
            raise ValueError('duplicate measured symbol: ' + name)
        symbols[name] = {'size': int(size, 16), 'kind': kind}
    if not symbols:
        raise ValueError('empty measured symbol set')
    return symbols


def dispatch_instructions(text: str) -> dict:
    """Count decoded instructions, excluding embedded literal pool words."""
    result = {}
    for name in ('call_arm', 'call_wait', 'call_wake'):
        matches = re.findall(r'<'+name+r'>:\n(.*?)(?:\n\n|\Z)',
                             text, re.DOTALL)
        if len(matches) != 1:
            raise ValueError('missing or duplicate dispatch body: ' + name)
        mnemonics = re.findall(
            r'^\s*[0-9a-fA-F]+:\s+'
            r'(?:[0-9a-fA-F]{4}(?:\s+[0-9a-fA-F]{4})?|[0-9a-fA-F]{8})'
            r'\s+(\S+)', matches[0], re.MULTILINE)
        count = sum(not word.startswith('.') for word in mnemonics)
        if count == 0:
            raise ValueError('empty decoded dispatch body: ' + name)
        result[name] = count
    return result


def check_layout(symbols: dict, count: int, shared: bool) -> None:
    """Prove the reviewed four-pointer ABI and the experimental two-pointer ABI."""
    if (type(count) is not int or count not in (1, 2, 4, 8) or
            type(shared) is not bool):
        raise ValueError('invalid face model argument')
    ports = symbols.get('ports')
    if ports != {'size': count * (8 if shared else 16), 'kind': 'd'}:
        raise ValueError('measured face layout differs from reviewed ABI')
    if shared:
        if symbols.get('ops') != {'size': 12, 'kind': 'r'}:
            raise ValueError('shared ops must be a measured readonly table')
    elif 'ops' in symbols:
        raise ValueError('direct ABI model retained a shared ops table')


def prepare_output(root: Path, output: Path) -> Path:
    """Never delete or overwrite evidence, source files or symlink destinations."""
    requested = output.absolute()
    if requested.is_symlink() or any(parent.is_symlink()
                                     for parent in requested.parents):
        raise ValueError('output must not traverse a symlink')
    root, output = root.resolve(), requested.resolve()
    if (output == root or root.is_relative_to(output) or
            (output.is_relative_to(root) and
             not output.is_relative_to(root / 'build'))):
        raise ValueError('output may not contain or modify platform source')
    if output.exists():
        raise ValueError('output must be fresh; existing evidence is immutable')
    output.mkdir(parents=True)
    return output


def matrix() -> tuple[dict, ...]:
    """Resolve all flags through production CPU facts; these are software models."""
    variants = []
    for cpu, choices in (('cortex-m0plus', {}), ('cortex-m4', {}),
                         ('cortex-m33', {'dsp': False})):
        facts = arch_compile.reference_facts(cpu, **choices)
        profile = arch_compile.cpu_contract.resolve(
            facts['cpu'], facts['irq'], 'baremetal').to_dict()
        for optimization in ('Os', 'O2'):
            for count in (1, 2, 4, 8):
                for shared in (False, True):
                    variants.append({
                        'name': f'{cpu}-{optimization}-{count}-'
                                + ('shared' if shared else 'direct'),
                        'facts': facts, 'profile': profile,
                        'optimization': optimization, 'instances': count,
                        'shared_ops': shared})
    return tuple(variants)


def source_inputs(root: Path) -> dict:
    """Bind the C fixture, public ABI and all imported profile authorities."""
    names = (
        'scripts/ci/os_wait_cost.py', 'scripts/ci/arch_compile.py',
        'tools/configure/cpu.py', 'tools/configure/kernel.py',
        'tools/configure/ir.py', 'tools/configure/providers/common.py',
        'os/include/nexus/os/wait.h', 'core/include/nexus/core/time.h',
        'core/include/nexus/core/status.h',
        'tests/contracts/os_wait_cost/probe.c',
        'tests/contracts/os_wait_cost/linker.ld')
    return {name: arch_compile.digest(root / name) for name in names}


def run(root: Path, compiler: str, output: Path) -> int:
    """Link all 48 models and retain actual tools, ELF and inspection outputs."""
    root = root.resolve()
    try:
        if root != Path(__file__).resolve().parents[2]:
            raise ValueError('root must match the executing source authority')
        output = prepare_output(root, output)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2
    recorder = arch_compile.Recorder(output)
    report = {'schema_version': 1, 'status': 'failed',
              'scope': 'linked_wait_abi_cost_models',
              'source_status': 'working_tree_measurement',
              'physical_status': 'not_executed',
              'full_platform_status': 'not_qualified',
              'cycle_status': 'not_measured', 'rows': [],
              'commands': recorder.commands}
    try:
        before = source_inputs(root)
        report['source_inputs'] = before
        selected = arch_compile.tools(compiler)
        size = Path(selected['gcc']).with_name('arm-none-eabi-size')
        if not size.is_file() or not shutil.which(str(size)):
            raise ValueError('associated GNU size tool is missing')
        selected['size'] = str(size)
        report['tools'] = {name: arch_compile.digest(Path(path))
                           for name, path in selected.items()}
        for name, tool in selected.items():
            recorder.execute([tool, '--version'], name + '-version',
                             nonempty=True)
        checked_profiles = set()
        fixture = root / 'tests/contracts/os_wait_cost'
        for variant in matrix():
            profile = variant['profile']
            cpu = variant['facts']['cpu']
            name = variant['name']
            flags = [*profile['compile_options'], '-std=c11',
                     '-' + variant['optimization'], '-Wall', '-Wextra', '-Werror',
                     '-ffunction-sections', '-fdata-sections']
            if cpu['arch'] not in checked_profiles:
                macros = recorder.execute(
                    [selected['gcc'], *flags, '-E', '-dM', '-x', 'c', '-'],
                    cpu['arch'] + '-macros', input_text='', nonempty=True)
                arch_compile.check_macros(cpu['arch'], macros, **{
                    key: cpu[key] for key in
                    ('fpu', 'float_abi', 'mve', 'security', 'dsp')})
                checked_profiles.add(cpu['arch'])
            elf = output / (name + '.elf')
            recorder.execute([
                selected['gcc'], *flags, '-nostdlib', '-Wl,--gc-sections',
                '-I' + str(root / 'os/include'),
                '-I' + str(root / 'core/include'),
                '-DCOUNT=' + str(variant['instances']),
                '-DSHARED=' + str(int(variant['shared_ops'])),
                '-T' + str(fixture / 'linker.ld'),
                str(fixture / 'probe.c'), '-o', str(elf)], name + '-link')
            sizes = parse_sizes(recorder.execute(
                [selected['size'], str(elf)], name + '-size', nonempty=True))
            symbols = parse_symbols(recorder.execute(
                [selected['nm'], '--print-size', '--size-sort', str(elf)],
                name + '-symbols', nonempty=True))
            check_layout(symbols, variant['instances'], variant['shared_ops'])
            arch_compile.check_arch_symbols(recorder.execute(
                [selected['nm'], '-u', str(elf)], name + '-undefined'))
            disassembly = recorder.execute(
                [selected['objdump'], '-d', str(elf)],
                name + '-disassembly', nonempty=True)
            recorder.execute([selected['readelf'], '-A', str(elf)],
                             name + '-attributes', nonempty=True)
            report['rows'].append({**variant, **sizes,
                'ports_bytes': symbols['ports']['size'],
                'ops_bytes': symbols.get('ops', {}).get('size', 0),
                'dispatch_instructions': dispatch_instructions(disassembly),
                'elf': arch_compile.digest(elf)})
        report['source_inputs_stable'] = before == source_inputs(root)
        if not report['source_inputs_stable'] or len(report['rows']) != 48:
            raise ValueError('source changed or cost matrix was incomplete')
        report['default_abi'] = {
            'selected': 'direct_callbacks',
            'reason': 'No extra operations-table load on the notification path; '
                      'shared tables remain an explicit multi-instance tradeoff',
            'claim': 'Model sizes and decoded instructions, not execution cycles'}
        report['status'] = 'passed'
    except (OSError, UnicodeError, ValueError) as error:
        report['error'] = str(error)
    (output / 'report.json').write_text(
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
