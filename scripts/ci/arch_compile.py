"""Execute and retain Cortex-M primitive compiler evidence, never HIL claims.

Nine maintained primitive profiles cover exact FPU/MVE/security ABI choices.
This probes Arch code generation, not SoC startup or physical platforms.
Missing tools, changed target facts and hidden runtime dependencies fail closed.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from types import MappingProxyType

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.configure import cpu as cpu_contract


@dataclass(frozen=True)
class Profile:
    arch_macro: str
    mainline: bool
    atomic_lock_free: int


PROFILES = MappingProxyType({
    name: Profile(item.macro, item.basepri, 1 if item.atomic == 'irq' else 2)
    for name, item in cpu_contract.PROFILES.items() if name != 'native'})

PUBLIC_SYMBOLS = frozenset({
    'nx_arch_irq_save', 'nx_arch_irq_restore', 'nx_arch_irq_is_masked',
    'nx_arch_irq_masks',
    'nx_arch_exception_number', 'nx_arch_is_privileged', 'nx_arch_in_isr',
    'nx_arch_dmb', 'nx_arch_dsb', 'nx_arch_isb', 'nx_arch_cycle_snapshot',
})

ATOMIC_SOURCE = """#include <stdatomic.h>
_Static_assert(sizeof(unsigned) == 4, "32-bit atomic probe required");
unsigned snapshot(const unsigned* p) {
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}
void publish(unsigned* p, unsigned value) {
    __atomic_store_n(p, value, __ATOMIC_RELEASE);
}
unsigned increment(unsigned* p) {
    return __atomic_fetch_add(p, 1, __ATOMIC_RELEASE);
}
"""


def profile(cpu: str) -> Profile:
    try:
        return PROFILES[cpu]
    except KeyError as error:
        raise ValueError(f'unsupported CPU: {cpu!r}') from error


def reference_facts(cpu: str, **choices) -> dict:
    """Declare software reference facts; never infer an actual chip or board."""
    profile(cpu)
    supported = cpu_contract.PROFILES[cpu]
    facts = {'arch': cpu, 'fpu': 'none', 'float_abi': 'soft',
             'dwt_cyccnt': supported.basepri, 'mpu_version': supported.mpu,
             'icache_line_bytes': 32 if supported.cache else 0,
             'dcache_line_bytes': 32 if supported.cache else 0,
             'security': 'single', 'sau': False, 'mve': 'none',
             'dsp': supported.dsp[-1]}
    facts.update(choices)
    return {'schema_version': 1, 'cpu': facts,
            'irq': {'priority_bits': 4 if supported.basepri else 2,
                    'external_count': supported.irq_count},
            'clock_hz': 48000000}


def software_profiles() -> tuple[dict, ...]:
    """Fixed review matrix, resolved by the sole production CPU authority."""
    variants = []

    def add(cpu, **choices):
        facts = reference_facts(cpu, **choices)
        value = facts['cpu']
        name = '-'.join(value[field] for field in
                        ('arch', 'fpu', 'float_abi', 'mve', 'security')) + (
                            '-dsp' if value['dsp'] else '-nodsp')
        # Validation owns ISA, optional features and the actual ABI flag list.
        resolved = cpu_contract.resolve(value, facts['irq'], 'baremetal')
        variants.append({'name': name, 'facts': facts,
                         'profile': resolved.to_dict()})

    for cpu, supported in cpu_contract.PROFILES.items():
        if cpu == 'native':
            continue
        add(cpu)
        for fpu in supported.fpus:
            for abi in ('hard', 'softfp'):
                add(cpu, fpu=fpu, float_abi=abi)
        if supported.mve:
            add(cpu, mve='integer', float_abi='softfp')
            for abi in ('hard', 'softfp'):
                add(cpu, fpu='auto', mve='integer', float_abi=abi)
                add(cpu, fpu='auto', mve='float', float_abi=abi)
        if supported.security:
            add(cpu, security='secure', sau=True)
            add(cpu, security='nonsecure')
        for dsp in supported.dsp[:-1]:
            for variant in tuple(variants):
                if variant['facts']['cpu']['arch'] == cpu:
                    choices = dict(variant['facts']['cpu'])
                    del choices['arch']
                    choices['dsp'] = dsp
                    add(cpu, **choices)
    return tuple(variants)


def check_macros(cpu: str, text: str, *, fpu='none', float_abi='soft',
                 mve='none', security='single', dsp=None) -> None:
    selected = profile(cpu)
    macros = {}
    for line in text.splitlines():
        match = re.fullmatch(r'#define\s+(\w+)\s+(.*)', line)
        if match:
            name, value = match.groups()
            if name in macros:
                raise ValueError(f'duplicate compiler macro: {name}')
            macros[name] = value
    arch_macros = {item.arch_macro for item in PROFILES.values()}
    if (macros.get(selected.arch_macro) != '1' or
            (arch_macros & macros.keys()) != {selected.arch_macro} or
            macros.get('__ARM_ARCH_PROFILE') != '77'):
        raise ValueError(f'{cpu}: wrong architecture macro')
    hard = macros.get('__ARM_PCS_VFP') == '1'
    if (hard != (float_abi == 'hard') or
            macros.get('__SIZEOF_INT__') != '4' or
            (float_abi == 'soft' and macros.get('__SOFTFP__') != '1')):
        raise ValueError(f'{cpu}: wrong {float_abi}-float ABI or integer width')
    floating = int(macros.get('__ARM_FP', '0'), 0)
    if (bool(floating) != (fpu != 'none') or
            (fpu != 'none' and not floating & 4) or
            (fpu == 'fpv5-d16' and not floating & 8)):
        raise ValueError(f'{cpu}: wrong FPU instruction capability')
    vector = int(macros.get('__ARM_FEATURE_MVE', '0'), 0)
    if vector != {'none': 0, 'integer': 1, 'float': 3}[mve]:
        raise ValueError(f'{cpu}: wrong MVE instruction capability')
    if dsp is None:
        dsp = cpu_contract.PROFILES[cpu].dsp[-1]
    if (type(dsp) is not bool or
            int(macros.get('__ARM_FEATURE_DSP', '0'), 0) != int(dsp)):
        raise ValueError(f'{cpu}: wrong DSP instruction capability')
    secure = bool(int(macros.get('__ARM_FEATURE_CMSE', '0'), 0) & 2)
    if secure != (security == 'secure'):
        raise ValueError(f'{cpu}: wrong Secure compiler state')
    if (macros.get('__GCC_ATOMIC_INT_LOCK_FREE') !=
            str(selected.atomic_lock_free)):
        raise ValueError(f'{cpu}: unexpected atomic lock-free capability')
    if '__GNUC__' not in macros:
        raise ValueError(f'{cpu}: GNU compiler identity missing')


def undefined_symbols(text: str) -> set[str]:
    symbols = set()
    for line in text.splitlines():
        match = re.fullmatch(r'\s*(?:[0-9a-fA-F]+\s+)?([Uwv])\s+(\S+)\s*', line)
        if not match:
            raise ValueError(f'unrecognized undefined-symbol output: {line!r}')
        symbols.add(match.group(2))
    return symbols


def check_arch_symbols(text: str) -> None:
    symbols = undefined_symbols(text)
    if symbols:
        raise ValueError(f'Arch external dependency: {sorted(symbols)}')


def check_exports(text: str) -> None:
    exported = set()
    for line in text.splitlines():
        match = re.fullmatch(r'[0-9a-fA-F]+\s+T\s+(\S+)', line.strip())
        if match:
            exported.add(match.group(1))
        else:
            raise ValueError(f'Arch unexpected public symbol scope: {line!r}')
    if exported != PUBLIC_SYMBOLS:
        raise ValueError(f'Arch incomplete public symbol scope: {sorted(exported)}')


def check_disabled_counter(disassembly: str) -> None:
    match = re.search(
        r'<nx_arch_cycle_snapshot>:\n(.*?)(?=\n[^\n]*<[^>]+>:\n|\Z)',
        disassembly, flags=re.DOTALL)
    if not match or re.search(
            r'\b(?:ldr|str|ldm|stm|push|pop)\w*\b', match.group(1).lower()):
        raise ValueError('unavailable cycle counter has memory accesses')


def check_atomic_symbols(cpu: str, text: str) -> None:
    expected = ({'__atomic_fetch_add_4'} if
                profile(cpu).atomic_lock_free == 1 else set())
    observed = undefined_symbols(text)
    if observed != expected:
        raise ValueError(f'{cpu}: unexpected atomic helper scope: '
                         f'{sorted(observed)}; expected {sorted(expected)}')


def check_instructions(cpu: str, text: str) -> None:
    # Apply the same policy to compiler assembly and machine disassembly.
    # Global textual presence does not establish instruction timing or runtime
    # hardware execution; the report explicitly restricts this evidence scope.
    lowered = text.lower()
    required = (
        r'\bmrs\s+[^,\n]+,\s*primask\b',
        r'\bmsr\s+primask\s*,', r'\bcpsid\s+i\b',
        r'\bmrs\s+[^,\n]+,\s*ipsr\b',
        r'\bmrs\s+[^,\n]+,\s*control\b',
        r'\bdmb\b', r'\bdsb\b', r'\bisb\b',
    )
    for expression in required:
        if not re.search(expression, lowered):
            raise ValueError(f'{cpu}: missing required instruction: {expression}')
    for register in ('basepri', 'faultmask'):
        observed = bool(re.search(
            rf'\bmrs\s+[^,\n]+,\s*{register}\b', lowered))
        if observed != profile(cpu).mainline:
            raise ValueError(f'{cpu}: wrong mask register scope: {register}')
        if re.search(rf'\bmsr\s+{register}\s*,', lowered):
            raise ValueError(f'{cpu}: unexpected mask write: {register}')
    if re.search(r'\bmsr\s+basepri_max\s*,', lowered):
        raise ValueError(f'{cpu}: unexpected mask write: basepri_max')
    if re.search(r'\b(?:bl|blx)(?:\.w)?\s+', lowered):
        raise ValueError(f'{cpu}: Arch primitive contains a runtime call')


def digest(path: Path) -> dict:
    data = path.read_bytes()
    return {'path': str(path.resolve()), 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest()}


class Recorder:
    """Run argv directly and retain raw output even when execution fails."""

    def __init__(self, output: Path):
        self.output = output.resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.commands = []

    def execute(self, argv: list[str], label: str, *,
                input_text: str | None = None, nonempty: bool = False,
                reject: bool = False) -> str:
        prefix = self.output / f'{len(self.commands):03d}-{label}'
        record = {'argv': argv, 'cwd': str(Path.cwd()), 'returncode': None}
        self.commands.append(record)
        if input_text is not None:
            path = prefix.with_suffix('.stdin')
            path.write_bytes(input_text.encode('utf-8'))
            record['stdin'] = digest(path)
        try:
            result = subprocess.run(
                argv, input=(input_text.encode('utf-8') if
                             input_text is not None else None),
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                timeout=60, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            record['error'] = str(error)
            raise ValueError(f'command execution failed: {argv[0]}: {error}') from error
        record['returncode'] = result.returncode
        for key, data in (('stdout', result.stdout), ('stderr', result.stderr)):
            path = prefix.with_suffix(f'.{key}')
            path.write_bytes(data)
            record[key] = digest(path)
        if (reject and result.returncode != 1) or (
                not reject and result.returncode != 0):
            raise ValueError(f'command failed expected outcome: {argv}; '
                             f'exit {result.returncode}')
        if nonempty and not result.stdout.strip():
            raise ValueError(f'command output empty: {argv}')
        if reject and not result.stderr.strip():
            raise ValueError(f'negative compile has no diagnostic: {argv}')
        return result.stdout.decode('utf-8', errors='strict')


def tools(compiler: str) -> dict[str, str]:
    resolved = shutil.which(compiler)
    if resolved is None:
        raise ValueError(f'compiler missing or not executable: {compiler}')
    path = Path(resolved).resolve()
    if path.name != 'arm-none-eabi-gcc':
        raise ValueError('compiler must be arm-none-eabi-gcc')
    result = {'gcc': str(path)}
    for name in ('nm', 'objdump', 'readelf'):
        selected = path.with_name('arm-none-eabi-' + name)
        if not selected.is_file() or not shutil.which(str(selected)):
            raise ValueError(f'associated compiler tool missing: {selected}')
        result[name] = str(selected)
    return result


def run(root: Path, compiler: str, output: Path) -> int:
    root = root.resolve()
    report = {'schema': 1, 'status': 'failed',
              'scope': 'cortex_m_primitives_and_atomic_compiler_probe',
              'full_platform_status': 'not_qualified',
              'physical_status': 'not_executed', 'profiles': []}
    recorder = Recorder(output)
    report['commands'] = recorder.commands
    try:
        selected_tools = tools(compiler)
        report['tools'] = {
            name: digest(Path(path)) for name, path in selected_tools.items()}
        for name, path in selected_tools.items():
            recorder.execute([path, '--version'], name + '-version',
                             nonempty=True)
        source = root / 'arch/cortex_m/nx_arch_cortex_m.c'
        header = root / 'arch/include/nexus/arch/arch.h'
        if not source.is_file() or not header.is_file():
            raise ValueError('maintained Cortex-M Arch source/header missing')
        report['source'] = digest(source)
        report['header'] = digest(header)
        report['private_boundary'] = digest(source.parent / 'private/compiler.h')
        report['gate'] = digest(Path(__file__))
        probe = recorder.output / 'atomic-probe.c'
        probe.write_text(ATOMIC_SOURCE, encoding='utf-8')
        report['atomic_probe'] = digest(probe)
        for variant in software_profiles():
            cpu_facts = variant['facts']['cpu']
            cpu = cpu_facts['arch']
            facts = profile(cpu)
            name = variant['name']
            resolved = variant['profile']
            flags = [*resolved['compile_options'],
                     '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
            macros = recorder.execute(
                [selected_tools['gcc'], *flags, '-E', '-dM', '-x', 'c', '-'],
                name + '-macros', input_text='', nonempty=True)
            check_macros(cpu, macros, **{
                key: cpu_facts[key] for key in
                ('fpu', 'float_abi', 'mve', 'security', 'dsp')})
            result = {'cpu': cpu, 'name': name, 'status': 'running',
                      'resolved_profile': resolved,
                      'full_platform_status': 'not_qualified',
                      'atomic_int_lock_free': facts.atomic_lock_free,
                      'atomic_rmw_scope': ('compiler_builtin_inline' if
                                           facts.atomic_lock_free == 2 else
                                           'compiler_builtin_external_helper_requires_nexus_irq_port'),
                      'dwt_profiles': []}
            report['profiles'].append(result)
            arch_flags = [*flags, '-I' + str(root / 'arch/include'),
                          '-I' + str(source.parent)]
            for dwt in (0, 1):
                filename = recorder.output / f'{name}-dwt{dwt}'
                current = [selected_tools['gcc'], *arch_flags,
                           f'-DNEXUS_ARCH_HAS_DWT_CYCCNT={dwt}']
                obj = filename.with_suffix('.o')
                if dwt and not facts.mainline:
                    recorder.execute([*current, '-c', str(source), '-o',
                                      str(obj)], name + '-dwt1-rejection',
                                     reject=True)
                    result['dwt_profiles'].append(
                        {'dwt': dwt, 'status': 'rejected_as_required'})
                    continue
                asm = filename.with_suffix('.s')
                recorder.execute([*current, '-c', str(source), '-o', str(obj)],
                                 name + f'-dwt{dwt}-compile')
                recorder.execute([*current, '-S', str(source), '-o', str(asm)],
                                 name + f'-dwt{dwt}-assembly')
                check_arch_symbols(recorder.execute(
                    [selected_tools['nm'], '-u', str(obj)],
                    name + f'-dwt{dwt}-undefined'))
                check_exports(recorder.execute(
                    [selected_tools['nm'], '--defined-only', '--extern-only',
                     str(obj)], name + f'-dwt{dwt}-exports', nonempty=True))
                disassembly = recorder.execute(
                    [selected_tools['objdump'], '-d', str(obj)],
                    name + f'-dwt{dwt}-disassembly', nonempty=True)
                check_instructions(cpu, disassembly)
                check_instructions(cpu, asm.read_text())
                if not dwt:
                    check_disabled_counter(disassembly)
                attributes = recorder.execute(
                    [selected_tools['readelf'], '-A', str(obj)],
                    name + f'-dwt{dwt}-attributes', nonempty=True)
                hard = bool(re.search(
                    r'Tag_ABI_VFP_args:\s*VFP registers', attributes))
                if hard != (cpu_facts['float_abi'] == 'hard'):
                    raise ValueError(f'{cpu}: object has unexpected float ABI')
                result['dwt_profiles'].append(
                    {'dwt': dwt, 'status': 'compiled',
                     'object': digest(obj), 'assembly': digest(asm)})
            for suffix, arguments in (
                    ('missing', []),
                    ('invalid', ['-DNEXUS_ARCH_HAS_DWT_CYCCNT=2']),
                    ('unknown', ['-DNEXUS_ARCH_HAS_DWT_CYCCNT=unknown'])):
                recorder.execute(
                    [selected_tools['gcc'], *arch_flags, *arguments, '-c',
                     str(source), '-o', str(recorder.output /
                                           f'{name}-dwt-{suffix}.o')],
                    name + '-dwt-' + suffix + '-rejection', reject=True)
            atom = recorder.output / (name + '-atomics.o')
            recorder.execute([selected_tools['gcc'], *flags, '-c', str(probe),
                              '-o', str(atom)], name + '-atomic-compile')
            check_atomic_symbols(cpu, recorder.execute(
                [selected_tools['nm'], '-u', str(atom)],
                name + '-atomic-undefined'))
            recorder.execute([selected_tools['objdump'], '-d', str(atom)],
                             name + '-atomic-disassembly', nonempty=True)
            result['atomic_object'] = digest(atom)
            result['status'] = 'primitive_compile_passed'
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
