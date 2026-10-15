"""Build the actual two-image CMSE ports with explicit caller-owned contexts.

The synthetic link regions prove software closure and gateway consumption. No
firmware runs, and no SAU/IDAU, silicon memory map or physical result is inferred.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shlex
import shutil
import sys

import arch_compile as compiler_gate
import cortex_runtime
from tools.configure import cpu, kernel


GATEWAYS = ('SecureContext_Init', 'SecureContext_AllocateContext',
            'SecureContext_FreeContext', 'SecureContext_LoadContext',
            'SecureContext_SaveContext', 'SecureInit_DePrioritizeNSExceptions',
            'SecureInit_EnableNSFPUAccess')
NS_REQUIRED = ('SVC_Handler', 'PendSV_Handler', 'SysTick_Handler',
               'vTaskStartScheduler', 'vApplicationGetIdleTaskMemory',
               'task_tcb', 'idle_tcb')


def symbols(text: str) -> dict[str, tuple[int, int]]:
    return {name: (int(address, 16), int(size)) for address, size, name in re.findall(
        r'^\s*\d+:\s*([0-9a-fA-F]+)\s+(\d+)\s+\w+\s+\w+\s+\w+\s+\w+\s+(\S+)\s*$',
        text, re.MULTILINE)}


def no_allocation(table: dict) -> None:
    forbidden = {'malloc', 'calloc', 'realloc', 'free', 'pvPortMalloc',
                 'vPortFree', 'xSecureContexts'}
    if forbidden & table.keys() or any('KernelObjectPool' in name for name in table):
        raise ValueError('Split-world image contains allocation or a context pool')


def check_secure(table: dict[str, tuple[int, int]], nsc: int, size: int) -> None:
    no_allocation(table)
    if nsc % 32 or size < 7 * 8:
        raise ValueError('The real NSC output has invalid veneer geometry')
    addresses = set()
    for name in GATEWAYS:
        address, extent = table.get(name, (0, 0))
        address &= ~1
        if not extent or address < nsc or address >= nsc + size or extent > nsc + size - address:
            raise ValueError('An actual exported gateway is outside its NSC section')
        addresses.add(address)
    if len(addresses) != len(GATEWAYS):
        raise ValueError('Distinct gateways must export distinct actual SG veneers')
    for name in ('SecureContext_LoadContextAsm', 'SecureContext_SaveContextAsm'):
        if name not in table or not table[name][0]:
            raise ValueError('The actual matching Secure context ASM is missing')


def check_nonsecure(table: dict[str, tuple[int, int]], secured: dict) -> None:
    no_allocation(table)
    if any(name not in table or not table[name][0] for name in NS_REQUIRED):
        raise ValueError('Actual NS kernel, context handlers or caller Idle storage are missing')
    for name in GATEWAYS:
        if name not in table or table[name][0] != secured[name][0]:
            raise ValueError('The NS image did not consume the exact Secure gateway import')


def check_context_helpers(undefined: str) -> None:
    if re.search(r'\b(?:__aeabi_)?mem(?:cpy|set|move)\w*\b', undefined):
        raise ValueError('Fixed-size context/seal operations must compile without memory helpers')


def matrix() -> tuple[dict, ...]:
    unique = {}
    for variant in compiler_gate.software_profiles():
        value = variant['facts']['cpu']
        if not cpu.PROFILES[value['arch']].security:
            continue
        key = tuple(value[name] for name in ('arch', 'fpu', 'float_abi', 'mve', 'dsp'))
        unique[key] = value
    result = []
    for key, value in sorted(unique.items()):
        name = '-'.join(str(item) for item in key)
        pair = {'name': name}
        for domain, backend in (('secure', 'baremetal'), ('nonsecure', 'freertos')):
            features = {name: value[name] for name in ('fpu', 'float_abi', 'mve', 'dsp')}
            facts = compiler_gate.reference_facts(value['arch'], **features,
                                                 security=domain, sau=domain == 'secure')
            profile = cpu.resolve(facts['cpu'], facts['irq'], backend)
            selected = kernel.resolve({'security_model': 'split'}, profile,
                                      facts['clock_hz']) if domain == 'nonsecure' else None
            profile = kernel.bind(profile, selected)
            pair[domain] = {'name': name + '-' + domain, 'facts': facts,
                            'backend': backend, 'profile': profile.to_dict(),
                            'kernel': selected.to_dict() if selected else None}
        result.append(pair)
    return tuple(result)


def compile_objects(build: Path, root: Path, variant: dict, stage: str) -> list[dict]:
    database = json.loads((build / 'compile_commands.json').read_text())
    sources = {Path(item['file']).resolve() for item in database}
    architecture = variant['facts']['cpu']['arch'].removeprefix('cortex-m')
    if stage == 'secure':
        required = {root / 'os/freertos/secure/context.c', root / 'os/freertos/secure/init.c',
                    root / f'ext/freertos/portable/GCC/ARM_CM{architecture}/secure/secure_context_port.c'}
        forbidden = {'secure_context.c', 'secure_heap.c', 'secure_init.c'}
        if any(path.name in forbidden for path in sources):
            raise ValueError('An upstream heap/pool or unreviewed dual-mask init was compiled')
    else:
        required = {root / 'ext/freertos/tasks.c', root / 'ext/freertos/queue.c',
                    root / 'ext/freertos/list.c',
                    root / f'ext/freertos/portable/GCC/ARM_CM{architecture}/non_secure/port.c',
                    root / f'ext/freertos/portable/GCC/ARM_CM{architecture}/non_secure/portasm.c'}
    objects = []
    for item in database:
        argv = item.get('arguments') or shlex.split(item['command'])
        if any(flag not in argv for flag in variant['profile']['compile_options']):
            raise ValueError('An object used a different CPU or ABI from its authored facts')
        output = Path(item['directory']) / argv[argv.index('-o') + 1]
        if not output.exists():
            # The database includes optional targets which this consumer does
            # not link. Their absent objects cannot qualify any implementation.
            continue
        objects.append({'source': compiler_gate.digest(Path(item['file'])),
                        'object': compiler_gate.digest(output), 'argv': argv})
    if not required <= {Path(item['source']['path']) for item in objects}:
        raise ValueError('The actual split-world compiled source graph is incomplete')
    return objects


def execute(root: Path, recorder, tools: dict, pair: dict, sdk: Path | None,
            allow_fixture: bool = False) -> dict:
    cmake = shutil.which('cmake')
    if cmake is None or shutil.which('ninja') is None:
        raise ValueError('Actual CMake and Ninja are required')
    directory = recorder.output / pair['name']
    artifacts = {}
    for stage in ('secure', 'nonsecure'):
        variant = pair[stage]
        assembly = cortex_runtime.write_inputs(directory / stage / 'inputs', variant)
        if stage == 'nonsecure':
            with assembly.open('a') as stream:
                stream.write('\n[os]\nsecurity_model = "split"\n')
        build = directory / stage / 'build'
        argv = [cmake, '-S', str(root / 'tests/contracts/os_secure_runtime'), '-B', str(build),
                '-G', 'Ninja', '-DCMAKE_TOOLCHAIN_FILE=' + str(root / 'cmake/toolchains/arm-gcc.cmake'),
                '-DCMAKE_C_COMPILER=' + tools['gcc'], '-DPython3_EXECUTABLE=' + sys.executable,
                '-DNEXUS_RUNTIME_ASSEMBLY=' + str(assembly), '-DNEXUS_SECURE_STAGE=' + stage]
        if sdk:
            argv += ['-DNEXUS_SOURCE_SDK_PREFIX=' + str(sdk), '-DCMAKE_PREFIX_PATH=' + str(sdk)]
            if allow_fixture:
                argv.append('-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON')
        if stage == 'nonsecure':
            argv.append('-DNEXUS_SECURE_IMPORT_LIBRARY=' +
                        str(directory / 'secure/build/secure-import.o'))
        recorder.execute(argv, pair['name'] + '-' + stage + '-configure')
        recorder.execute([cmake, '--build', str(build), '--parallel', '2',
                          '--target', 'nexus_' + stage + '_contract'],
                         pair['name'] + '-' + stage + '-build')
        elf = build / ('nexus_' + stage + '_contract.elf')
        compiler_gate.check_arch_symbols(recorder.execute([tools['nm'], '-u', str(elf)],
                                                          pair['name'] + '-' + stage + '-undefined'))
        table = symbols(recorder.execute([tools['readelf'], '-sW', str(elf)],
                                         pair['name'] + '-' + stage + '-symbols', nonempty=True))
        disassembly = recorder.execute([tools['objdump'], '-d', str(elf)],
                                       pair['name'] + '-' + stage + '-disassembly', nonempty=True)
        if stage == 'secure':
            sections = recorder.execute([tools['objdump'], '-h', str(elf)],
                                        pair['name'] + '-nsc-section', nonempty=True)
            match = re.search(r'^\s*\d+\s+\.gnu\.sgstubs\s+([0-9a-f]+)\s+([0-9a-f]+)',
                              sections, re.MULTILINE)
            if not match or not re.search(r'\bsg\b', disassembly):
                raise ValueError('The actual Secure image contains no SG veneers')
            size, address = (int(item, 16) for item in match.groups())
            check_secure(table, address, size)
            secured = table
            imported = directory / 'secure/build/secure-import.o'
            import_table = symbols(recorder.execute([tools['readelf'], '-sW', str(imported)],
                                                   pair['name'] + '-real-import', nonempty=True))
            if any(name not in import_table or import_table[name][0] != table[name][0]
                   for name in GATEWAYS):
                raise ValueError('The actual CMSE import object disagrees with the Secure image')
            if 'cpsie' in disassembly:
                raise ValueError('Secure runtime contains an unconditional mask enable')
        else:
            check_nonsecure(table, secured)
            if not re.search(r'\bsvc\b', disassembly):
                raise ValueError('The actual NS context port contains no SVC instructions')
        objects = compile_objects(build, sdk / 'share/nexus/src' if sdk else root,
                                  variant, stage)
        if stage == 'secure':
            context_object = next(item['object']['path'] for item in objects
                                  if Path(item['source']['path']).parts[-3:] ==
                                  ('freertos', 'secure', 'context.c'))
            check_context_helpers(recorder.execute([tools['nm'], '-u', context_object],
                                                    pair['name'] + '-context-helper-audit'))
        resolved = build / 'nexus-runtime/generated/resolved-cpu.json'
        value = json.loads(resolved.read_text())
        if value['cpu_profile'] != variant['profile'] or value['kernel_profile'] != variant['kernel']:
            raise ValueError('Actual split-world resolution differs from authored facts/policy')
        artifacts[stage] = {'elf': compiler_gate.digest(elf),
                            'map': compiler_gate.digest(build / ('nexus_' + stage + '_contract.map')),
                            'resolved': compiler_gate.digest(resolved), 'compiled_objects': objects}
    return {'name': pair['name'], 'status': 'real_cmse_split_image_link_passed',
            'physical_status': 'not_executed', 'images': artifacts,
            'import': compiler_gate.digest(directory / 'secure/build/secure-import.o')}


def run(root: Path, compiler: str, output: Path, sdk: Path | None = None,
        allow_fixture: bool = False) -> int:
    if output.exists() and any(output.iterdir()):
        raise ValueError('Split-world qualification output must be fresh')
    recorder = compiler_gate.Recorder(output)
    report = {'schema': 1, 'status': 'failed', 'scope': 'real_cmse_two_image_link',
              'physical_status': 'not_executed', 'variants': [], 'commands': recorder.commands}
    try:
        tools = compiler_gate.tools(compiler)
        inputs = cortex_runtime.source_inputs(root)
        additional = [Path(__file__), root / 'tools/configure/kernel.py']
        additional.extend((root / 'tests/contracts/os_secure_runtime').glob('*'))
        for core in (23, 33, 55, 85):
            for directory in (root / f'ext/freertos/portable/GCC/ARM_CM{core}/secure',
                              root / f'ext/freertos/portable/GCC/ARM_CM{core}/non_secure'):
                additional.extend(path for path in directory.glob('*')
                                  if path.suffix in {'.c', '.h'})
        additional.extend(root / 'ext/freertos' / name
                          for name in ('tasks.c', 'queue.c', 'list.c'))
        additional.extend((root / 'ext/freertos/include').glob('*.h'))
        inputs += [compiler_gate.digest(path) for path in additional if path.is_file()]
        report['source_inputs'] = inputs
        report['tools'] = {name: compiler_gate.digest(Path(path)) for name, path in tools.items()}
        if sdk:
            report['source_sdk_manifest'] = compiler_gate.digest(sdk / 'share/nexus/src/.nexus-source-sdk.json')
            report['allows_development_fixture'] = allow_fixture
        for pair in matrix():
            report['variants'].append(execute(root, recorder, tools, pair, sdk, allow_fixture))
        cortex_runtime.verify_source_inputs(inputs)
        report['status'] = 'passed'
    except (OSError, ValueError) as error:
        report['error'] = str(error)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'secure-matrix.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'variants': len(report['variants']),
                      'report': str(output / 'secure-matrix.json')}))
    return 0 if report['status'] == 'passed' else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source-sdk-prefix', type=Path)
    parser.add_argument('--allow-source-sdk-fixture', action='store_true')
    arguments = parser.parse_args()
    return run(Path(__file__).resolve().parents[2], arguments.compiler, arguments.output,
               arguments.source_sdk_prefix, arguments.allow_source_sdk_fixture)


if __name__ == '__main__':
    raise SystemExit(main())
