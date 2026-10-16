"""Check actual H743 reference ELF/Run-star closure, not physical acceptance.

The gate consumes executed objdump bytes and independent memory facts. It does
not measure oscillator accuracy, hardware poll latency, cache or DMA behavior.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.measurement.elf import Elf32
from tools.evidence.common import (EvidenceError, atomic_json, file_identity,
                                   fields, load_json, verify_file_identity)
from tools.measurement.soc_costs import decode_function
from tools.evidence.identity import git_source
try:
    from . import arch_compile, provider_assemblies
except ImportError:
    import arch_compile
    import provider_assemblies


def validate_profile(resolved):
    """Keep exact reviewed chip, Board, CPU ABI and fixed HSI source contracts."""
    if (resolved.get('part') != 'STM32H743ZIT6' or
            resolved.get('soc_family') != 'stm32h743' or
            resolved.get('board') not in {'stm32h743-nucleo-zi2',
                                        'stm32h743zi-hsi-reference'} or
            resolved.get('backend') not in {'baremetal', 'freertos'}):
        raise EvidenceError('Exact H743ZI reference part/board/backend required')
    expected_cpu = {'arch': 'cortex-m7', 'fpu': 'fpv5-d16', 'float_abi': 'hard',
        'dsp': True, 'dwt_cyccnt': True, 'mpu_version': 7, 'mpu_regions': 16,
        'icache_line_bytes': 32, 'dcache_line_bytes': 32, 'security': 'single',
        'sau': False, 'mve': 'none'}
    profile = resolved.get('clock_profile')
    if profile not in {'hsi32-fixed-cache-off', 'hsi32-fixed-cache-on',
                       'hsi64-fixed-cache-off', 'hsi64-fixed-cache-on'}:
        raise EvidenceError('Unreviewed H743 fixed clock/Cache profile')
    frequency = 32000000 if profile.startswith('hsi32-') else 64000000
    expected_clock = {'source': 'hsi', 'input_hz': 64000000,
        'core_hz': frequency, 'apb1_hz': frequency, 'apb2_hz': frequency}
    if (resolved.get('cpu') != expected_cpu or
            any(type(resolved['cpu'][key]) is not type(value)
                for key, value in expected_cpu.items()) or
            resolved.get('clock') != expected_clock or
            any(type(resolved['clock'][key]) is not type(value)
                for key, value in expected_clock.items()) or
            resolved.get('enum_abi') != 'short-enums'):
        raise EvidenceError('Exact H743 fixed HSI CPU and enum ABI required')
    validate_memory_facts(resolved)
    allowed = {('gpio', 'GPIOB', 'input'), ('gpio', 'GPIOB', 'output'),
               ('uart', 'USART3', 'irq-bytes'), ('uart', 'USART3', 'dma-tx'),
               ('uart', 'USART3', 'rx-dma-idle'),
               ('spi', 'SPI4', 'short-poll'), ('i2c', 'I2C1', 'poll-messages'),
               ('adc', 'ADC1', 'single-shot'), ('pwm', 'TIM3', 'fixed-pwm'),
               ('exti', 'EXTI0', 'edge-event'), ('exti', 'EXTI1', 'edge-event'),
               ('watchdog', 'IWDG1', 'independent'),
               ('flash', 'FLASH', 'poll-word')}
    for item in resolved.get('controllers', []):
        if (item.get('kind'), item.get('controller'), item.get('mode')) not in allowed:
            raise EvidenceError('Unmaintained H743 controller mode')


# Independent RM0433 Rev9 bus/memory facts, not copied from generated IR.
MEMORY_FACTS = {
    'itcm': (0x00000000, 65536, (False, False, False, True)),
    'dtcm': (0x20000000, 131072, (False, False, False, True)),
    'axi-sram': (0x24000000, 524288, (True, True, False, True)),
    'sram1': (0x30000000, 131072, (True, True, False, True)),
    'sram2': (0x30020000, 131072, (True, True, False, True)),
    'sram3': (0x30040000, 32768, (True, True, False, True)),
    'sram4': (0x38000000, 65536, (True, True, True, True)),
    'backup-sram': (0x38800000, 4096, (True, True, True, True))}


def validate_memory_facts(resolved):
    """Require exact physical domains/engine maps and explicit SRAM1 selection."""
    regions = resolved.get('memory', ())
    if resolved.get('part') != 'STM32H743ZIT6' or len(regions) != 8:
        raise ValueError('Exact H743ZI chip/eight physical domains required')
    domains = {region['id']: region for region in regions}
    if set(domains) != set(MEMORY_FACTS):
        raise ValueError('Missing/duplicate/expanded H743 memory domain')
    optional = resolved.get('memory_budgets', {}).get('regions', {})
    if set(optional) - {'sram1'}:
        raise ValueError('Unreviewed optional H743 initialization region')
    if 'sram1' in optional:
        limit = optional['sram1'].get('limit_bytes')
        if type(limit) is not int or not 0 < limit <= 131072 or limit % 4:
            raise ValueError('Explicit bounded aligned SRAM1 budget required')
    engines = ('DMA1', 'DMA2', 'BDMA', 'MDMA')
    for name, (origin, size, reaches) in MEMORY_FACTS.items():
        region = domains[name]
        expected = dict(zip(engines, reaches))
        actual = region.get('dma_engines')
        if (type(region.get('origin')) is not int or
                type(region.get('size')) is not int or
                region['origin'] != origin or region['size'] != size or
                region.get('linker') is not (name == 'axi-sram' or name in optional) or
                'dma' in region or not isinstance(actual, dict) or
                actual != expected or
                any(type(value) is not bool for value in actual.values())):
            raise ValueError('Physical/selected H743 domain differs: ' + name)


def validate_power_entry(elf, disassembly):
    """Reject every unreviewed instruction/operand and any RAM/call/stack use.

    This narrow proof checks the whole actual Thumb leaf, finite decreasing
    counter, MMIO readback and ready-only return. It is not an Arm CPU emulator.
    Changed instruction shapes require independent review before qualification.
    """
    functions = [item for item in elf.defined_symbols
                 if item['name'] == 'ExitRun0Mode' and item['type'] == 2]
    if len(functions) != 1 or not functions[0]['size']:
        raise ValueError('One nonempty actual power-entry function required')
    function = functions[0]
    start = function['value'] & ~1
    end = start + function['size']
    rows = []
    for match in re.finditer(
            r'^\s*([0-9a-f]+):\s+([0-9a-f]{4}(?: [0-9a-f]{4})?)\s+'
            r'([a-z][a-z0-9.]*)\s*(.*?)\s*$', disassembly, re.MULTILINE):
        address = int(match[1], 16)
        if start <= address < end:
            raw = b''.join(int(word, 16).to_bytes(2, 'little')
                           for word in match[2].split())
            rows.append((address, raw, match[3], match[4].split(';')[0].split('@')[0].strip()))
    if not rows:
        raise ValueError('Power entry has no actual decoded instructions')
    cursor = start
    section = next(item for item in elf.sections
                   if item['flags'] & 4 and item['address'] <= start <
                   end <= item['address'] + item['size'])
    # Fixed position-independent Thumb leaf reviewed against RM0433 and the
    # independent actual objdump audit. A matching textual mnemonic is not a
    # machine-code decoder: bind the entire reviewed opcode sequence as well.
    offset = section['offset'] + start - section['address']
    machine = elf.data[offset:offset + function['size']]
    reviewed_opcode_hash = '3d34b43bf7e706bd5c24df0eb0d522109bb39476f628bb9d892a3c286f160ba6'
    if len(machine) != 140 or hashlib.sha256(machine).hexdigest() != reviewed_opcode_hash:
        raise ValueError('Unreviewed actual power-entry opcode bytes')
    registers = {}
    reads, stores = [], []
    for address, raw, mnemonic, operands in rows:
        if address != cursor:
            raise ValueError('Power-entry instruction gap/overlap')
        offset = section['offset'] + address - section['address']
        if elf.data[offset:offset + len(raw)] != raw:
            raise ValueError('Objdump differs from actual ELF instruction bytes')
        cursor += len(raw)
        if mnemonic in {'movw', 'movt', 'movs'}:
            match = re.fullmatch(r'(r[012]), #([0-9]+)', operands)
            if not match:
                raise ValueError('Unreviewed power register/immediate')
            reg, value = match[1], int(match[2])
            if mnemonic == 'movt':
                registers[reg] = (registers.get(reg, 0) & 0xffff) | value << 16
            else:
                registers[reg] = value
        elif mnemonic in {'ldr', 'str'}:
            match = re.fullmatch(r'r1, \[r0(?:, #0)?\]', operands)
            if not match or registers.get('r0') not in {
                    0x5802480c, 0x58024804, 0x5c001000, 0x51008108}:
                raise ValueError('Power entry accesses stack/RAM/unknown MMIO')
            if mnemonic == 'str':
                if registers['r0'] not in {0x5802480c, 0x51008108}:
                    raise ValueError('Power entry writes outside reviewed PWR/AXI controls')
                stores.append(address)
            else:
                reads.append((address, registers['r0']))
        elif mnemonic not in {'bic.w', 'orr.w', 'and.w', 'cmp', 'cmp.w', 'tst.w',
                              'bne.n', 'bcs.n', 'b.n', 'subs', 'lsrs',
                              'cpsid', 'dsb', 'isb', 'bx'}:
            raise ValueError('Power entry contains a call/stack/unknown instruction')
    if cursor != end or len(stores) != 2:
        raise ValueError('Whole power entry and two reviewed MMIO-only stores required')
    operations = [(row[2], row[3]) for row in rows]
    # The proof is intentionally a closed instruction shape. Count/presence
    # alone would admit value mutations or a store without its following DSB.
    expected = [
        ('movw', 'r0, #18444'), ('movt', 'r0, #22530'),
        ('ldr', 'r1, [r0, #0]'), ('bic.w', 'r1, r1, #1'),
        ('orr.w', 'r1, r1, #2'), ('str', 'r1, [r0, #0]'),
        ('dsb', 'sy'), ('ldr', 'r1, [r0, #0]'),
        ('and.w', 'r1, r1, #3'), ('cmp', 'r1, #2'), ('bne.n', None),
        ('movw', 'r0, #18436'), ('movt', 'r0, #22530'),
        ('movw', 'r2, #16960'), ('movt', 'r2, #15'),
        ('ldr', 'r1, [r0, #0]'), ('tst.w', 'r1, #8192'), ('bne.n', None),
        ('subs', 'r2, #1'), ('bne.n', None), ('movs', 'r0, #1'),
        ('b.n', None), ('movs', 'r0, #2'), ('cpsid', 'i'),
        ('dsb', 'sy'), ('isb', 'sy'), ('b.n', None),
        ('movw', 'r0, #4096'), ('movt', 'r0, #23552'),
        ('ldr', 'r1, [r0, #0]'), ('lsrs', 'r1, r1, #16'),
        ('cmp.w', 'r1, #8192'), ('bcs.n', None),
        ('movw', 'r0, #33032'), ('movt', 'r0, #20736'),
        ('ldr', 'r1, [r0, #0]'), ('orr.w', 'r1, r1, #1'),
        ('str', 'r1, [r0, #0]'), ('dsb', 'sy'),
        ('ldr', 'r1, [r0, #0]'), ('tst.w', 'r1, #1'),
        ('bne.n', None), ('movs', 'r0, #3'), ('b.n', None),
        ('dsb', 'sy'), ('isb', 'sy'), ('bx', 'lr')]
    if len(operations) != len(expected) or any(
            op != actual_op or (arg is not None and arg != actual_arg)
            for (op, arg), (actual_op, actual_arg) in zip(expected, operations)):
        raise ValueError('Unreviewed power instruction/operand order')
    required = [('bic.w', 'r1, r1, #1'), ('orr.w', 'r1, r1, #2'),
                ('and.w', 'r1, r1, #3'), ('cmp', 'r1, #2'),
                ('tst.w', 'r1, #8192'), ('subs', 'r2, #1'),
                ('cpsid', 'i'), ('bx', 'lr')]
    if any(operations.count(item) != 1 for item in required):
        raise ValueError('Missing/expanded power guard, finite poll or return')
    if ([reg for _, reg in reads] != [0x5802480c, 0x5802480c, 0x58024804,
                                     0x5c001000, 0x51008108, 0x51008108]):
        raise ValueError('Required power/revision/AXI readback order missing')
    # Control flow is verified against decoded addresses, not source spelling.
    index = {address: i for i, (address, _, _, _) in enumerate(rows)}
    def target(i):
        return int(rows[i][3].split()[0], 16)
    poll = next(i for i, (_, reg) in enumerate(reads) if reg == 0x58024804)
    poll_address = reads[poll][0]
    test = next(i for i, row in enumerate(rows) if row[2] == 'tst.w')
    decrement = next(i for i, row in enumerate(rows) if row[2] == 'subs')
    compare = next(i for i, row in enumerate(rows) if row[2] == 'cmp')
    if (rows[test + 1][2] != 'bne.n' or
            rows[decrement + 1][2] != 'bne.n' or
            target(decrement + 1) != poll_address or
            rows[compare + 1][2] != 'bne.n'):
        raise ValueError('Power-entry ready/readback/backedge guards differ')
    revision = next(i for i, row in enumerate(rows) if row[2] == 'cmp.w')
    ready = next(i for i, operation in enumerate(operations)
                 if operation == ('movw', 'r0, #4096'))
    if index.get(target(test + 1)) != ready:
        raise ValueError('AXI admission is not exclusively power-ready guarded')
    success = index.get(target(revision + 1))
    axi = next(i for i, operation in enumerate(operations)
               if operation == ('tst.w', 'r1, #1'))
    if (success is None or operations[success:] != [('dsb', 'sy'), ('isb', 'sy'), ('bx', 'lr')]
            or index.get(target(axi + 1)) != success):
        raise ValueError('Return must follow new revision or proved AXI readback')
    bad = index.get(target(compare + 1))
    if bad is None or operations[bad] != ('movs', 'r0, #2'):
        raise ValueError('Readback failure reason is missing')
    counter_lo = next((int(arg.split('#')[1]) for op, arg in operations
                       if op == 'movw' and arg.startswith('r2,')), None)
    counter_hi = next((int(arg.split('#')[1]) for op, arg in operations
                       if op == 'movt' and arg.startswith('r2,')), None)
    if counter_lo is None or counter_hi is None or (counter_hi << 16 | counter_lo) != 1000000:
        raise ValueError('Finite fixed power observation budget differs')
    failure = decrement + 2
    if operations[failure] != ('movs', 'r0, #1') or rows[failure + 1][2] != 'b.n':
        raise ValueError('Exhausted counter does not fail closed')
    fail = index.get(target(failure + 1))
    if fail is None or operations[fail:fail + 3] != [('cpsid', 'i'), ('dsb', 'sy'), ('isb', 'sy')]:
        raise ValueError('Power failure does not mask IRQs and retain r0')
    if rows[fail + 3][2] != 'b.n' or target(fail + 3) != rows[fail + 3][0]:
        raise ValueError('Power failure can return or write RAM')
    if operations[axi + 2] != ('movs', 'r0, #3') or index.get(target(axi + 3)) != fail:
        raise ValueError('AXI readback failure does not enter the no-RAM fault park')
    return {'symbol': 'ExitRun0Mode', 'bytes': function['size'],
            'instructions': len(rows), 'mmio_stores': 2, 'ram_stores': 0,
            'nested_calls': 0, 'poll_limit': 1000000,
            'axi_workaround': {'register': 0x51008108, 'read_iss_override': 1,
                'revision_limit_exclusive': 0x2000, 'readback_required': True,
                'phase': 'after_power_ready_before_ram'},
            'evidence_kind': 'actual_elf_control_flow_not_physical_execution'}


# Reviewed actual GCC14.3/Os Thumb shapes. BL relocation words are checked
# separately from their normalized hash using machine-decoded branch targets.
# Reset has no RAM stores before its first supply call. SystemInit's only
# non-stack stores are SCB CPACR/VTOR/AIRCR, after the guarded supply return.
PRE_RAM_SHAPES = {
    'Reset_Handler': (60, {4: 'ExitRun0Mode', 8: 'SystemInit',
        50: '__libc_init_array', 54: 'main'},
        '57f35ca2469ff7340abe8c624b97767bd911bb419d099a927b5bab8cbf240a46'),
    'SystemInit': (56, {16: 'nx_arch_dsb', 20: 'nx_arch_isb'},
        '94c0f0fd0b00fb5e8e3d7ec781cb2a6ca520b4ee50f776efdad1ea43407d62a9'),
    'nx_arch_dsb': (6, {},
        '5527f63981c4771dc4413a1111bd5c6a417b80fe4949d414f0df6560b757a397'),
    'nx_arch_isb': (6, {},
        '712b33b2a976799524c3cf520969bceee85afd1bc652c50c35c806a5898d0cda')}


# Independently reviewed optional-SRAM1 call closure. Only the Flash descriptor
# pointer is relocated; its nine words are bound to actual linker sections below.
SRAM1_PRE_RAM_SHAPES = {
    **PRE_RAM_SHAPES,
    'SystemInit': (100, {16: 'nx_arch_dsb', 20: 'nx_arch_isb',
        60: 'nx_arch_dsb', 72: 'nx_arch_irq_save',
        78: 'nx_stm32h743_memory_boot'},
        '002a6140a2b8065a8fbeebb9f88a314fd5cb29a4f595a3ed8d763955cc804b87'),
    'nx_stm32h743_memory_boot': (72, {44: 'nx_stm32h743_memory_initialize'},
        'c66d6d303c458a8fbe7daf9d5ec84af481f029a3da4a433c8e1126605916ecc6'),
    'nx_stm32h743_memory_initialize': (190, {},
        '0e13dd1e3c9758a2465897d2a14848b0290c810b0670813b8d62a5e336eaa30a'),
    'nx_arch_irq_save': (16, {},
        '8b009a1adcdb5bee673c7a0c1b700186e8677c9bcf7fa006d8e7c77ac354c26a')}


def image_bytes(elf, address, size):
    for section in elf.sections:
        if (section['kind'] != 8 and section['flags'] & 2 and
                section['address'] <= address < address + size <=
                section['address'] + section['size']):
            offset = section['offset'] + address - section['address']
            return elf.data[offset:offset + size]
    raise EvidenceError('Requested initialized bytes leave the actual ELF')


def bl_target(address, encoded):
    """Decode Thumb BL's signed immediate, never trust printed operands."""
    first, second = struct.unpack('<HH', encoded)
    if first & 0xf800 != 0xf000 or second & 0xd000 != 0xd000:
        raise EvidenceError('Pre-RAM dependency is not an actual Thumb BL')
    sign = first >> 10 & 1
    i1 = 1 ^ (second >> 13 & 1) ^ sign
    i2 = 1 ^ (second >> 11 & 1) ^ sign
    offset = (sign << 24 | i1 << 23 | i2 << 22 | (first & 1023) << 12 |
              (second & 2047) << 1)
    if sign:
        offset -= 1 << 25
    return address + 4 + offset


def pre_ram_function(elf, name, disassembly, *, shapes=PRE_RAM_SHAPES):
    function = [s for s in elf.defined_symbols
                if s['name'] == name and s['type'] == 2]
    if len(function) != 1:
        raise EvidenceError('Unique pre-RAM function required: ' + name)
    symbol = function[0]
    start, size = symbol['value'] & ~1, symbol['size']
    expected_size, calls, expected_hash = shapes[name]
    if size != expected_size or not 0x08000000 <= start < start + size <= 0x08200000:
        raise EvidenceError('Pre-RAM function left the reviewed Flash shape')
    selected = []
    for line in disassembly.splitlines():
        match = re.match(r'^\s*([0-9a-f]+):', line)
        if match and start <= int(match[1], 16) < start + size:
            selected.append(line)
    decoded = decode_function(elf, name, ('\n'.join(selected) + '\n').encode('ascii'))
    code = bytearray(image_bytes(elf, start, size))
    for offset, target in calls.items():
        if bl_target(start + offset, code[offset:offset + 4]) != elf.symbol(target) & ~1:
            raise EvidenceError('Pre-RAM call order/target differs: ' + target)
        code[offset:offset + 4] = bytes(4)
    if name == 'nx_stm32h743_memory_boot':
        code[56:60] = bytes(4)
    if hashlib.sha256(code).hexdigest() != expected_hash:
        raise EvidenceError('Unreviewed pre-RAM opcode/literal shape: ' + name)
    return {'symbol': name, 'bytes': size,
            'static_instructions': decoded['static_instructions'],
            'dependencies': list(calls.values())}


def primary_ram_initialization(elf):
    """Bind reset copy/zero bounds and DATA LMA to actual alloc sections."""
    result = {}
    for name, kind, start_name, end_name in (
            ('.data', 1, '_sdata', '_edata'), ('.bss', 8, '_sbss', '_ebss')):
        start, end = elf.symbol(start_name), elf.symbol(end_name)
        selected = [s for s in elf.sections if s['name'] == name]
        if not selected:
            if start != end:
                raise EvidenceError('Reset initializes an absent RAM section')
            result[name] = {'address': start, 'bytes': 0}
            continue
        section = selected[0]
        if (section['kind'] != kind or section['flags'] != 3 or
                section['address'] != start or section['size'] != end - start):
            raise EvidenceError('Reset bounds differ from actual section: ' + name)
        result[name] = {'address': start, 'bytes': section['size']}
        if name == '.data' and section['size']:
            loads = [load for load in elf.loads if load['flags'] == 6 and
                load['offset'] <= section['offset'] <
                section['offset'] + section['size'] <=
                load['offset'] + load['files'] and
                load['virtual'] + section['offset'] - load['offset'] == start]
            if len(loads) != 1:
                raise EvidenceError('Unique actual DATA LOAD mapping required')
            load = loads[0]
            source = load['physical'] + section['offset'] - load['offset']
            if (source != elf.symbol('_sidata') or
                    not 0x08000000 <= source < source + section['size'] <= 0x08200000):
                raise EvidenceError('Reset DATA source differs from actual LOAD LMA')
            result[name]['load_address'] = source
    return result


def sram1_initialization(elf):
    """Bind the source-closed copy/zero descriptor to exact alloc/LOAD ranges."""
    boot = elf.symbol('nx_stm32h743_memory_boot') & ~1
    table, = struct.unpack('<I', image_bytes(elf, boot + 56, 4))
    if table % 4 or not 0x08000000 <= table < table + 36 <= 0x08200000:
        raise EvidenceError('SRAM1 boot descriptor must be aligned immutable Flash')
    words = struct.unpack('<9I', image_bytes(elf, table, 36))
    symbols = ('__nx_sram1_region_start', '__nx_sram1_region_end',
               '__nx_sram1_data_load', '__nx_sram1_data_start',
               '__nx_sram1_data_end', '__nx_sram1_bss_start',
               '__nx_sram1_bss_end', '__nx_sram1_noinit_start',
               '__nx_sram1_noinit_end')
    if words != tuple(elf.symbol(name) for name in symbols):
        raise EvidenceError('SRAM1 boot descriptor differs from exact linker symbols')
    begin, end, load, data, data_end, bss, bss_end, noinit, noinit_end = words
    if (begin != 0x30000000 or end != 0x30020000 or
            any(value % 4 for value in words) or
            not begin <= data <= data_end <= bss <= bss_end <= noinit <= noinit_end <= end):
        raise EvidenceError('SRAM1 boot regions leave reviewed aligned ordering')
    if not 0x08000000 <= load <= 0x08200000 or data_end - data > 0x08200000 - load:
        raise EvidenceError('SRAM1 DATA source must remain within physical Flash')
    result = {}
    for name, kind, start, finish in (
            ('.nx_sram1_data', 1, data, data_end),
            ('.nx_sram1_bss', 8, bss, bss_end),
            ('.nx_sram1_noinit', 8, noinit, noinit_end)):
        sections = [item for item in elf.sections if item['name'] == name]
        if start == finish:
            # GNU ld can retain an empty linker-symbol-only PROGBITS section
            # without SHF_ALLOC, including a declared NOLOAD output section.
            if len(sections) > 1 or (sections and (
                    sections[0]['address'] != start or sections[0]['size'] != 0 or
                    sections[0]['flags'] not in {1, 3} or
                    sections[0]['kind'] not in {1, 8})):
                raise EvidenceError('Empty SRAM1 linker section differs: ' + name)
            result[name] = {'address': start, 'bytes': 0}
            continue
        if len(sections) != 1:
            raise EvidenceError('One exact SRAM1 section required: ' + name)
        section = sections[0]
        if (section['kind'] != kind or section['flags'] != 3 or
                section['address'] != start or section['size'] != finish - start):
            raise EvidenceError('SRAM1 initialization differs from actual section: ' + name)
        result[name] = {'address': start, 'bytes': finish - start}
        if kind == 1 and finish > start:
            loads = [item for item in elf.loads if item['flags'] == 6 and
                item['offset'] <= section['offset'] <
                section['offset'] + section['size'] <= item['offset'] + item['files'] and
                item['virtual'] + section['offset'] - item['offset'] == start]
            if len(loads) != 1:
                raise EvidenceError('Unique SRAM1 DATA load mapping required')
            actual = loads[0]['physical'] + section['offset'] - loads[0]['offset']
            if actual != load or not 0x08000000 <= load < load + finish - start <= 0x08200000:
                raise EvidenceError('SRAM1 DATA source differs from actual LOAD LMA')
            result[name]['load_address'] = load
    return result


def validate_image(elf, resolved, disassembly):
    """Bind vectors and every pre-DATA call to reviewed actual Thumb bytes."""
    validate_profile(resolved)
    vectors = [s for s in elf.sections if s['name'] == '.isr_vector']
    if len(vectors) != 1:
        raise EvidenceError('One actual H743 vector section required')
    vector = vectors[0]
    if (vector['address'] != 0x08000000 or vector['size'] != 166 * 4 or
            vector['kind'] == 8 or vector['flags'] != 2):
        raise EvidenceError('Official H743 vector range/count differs')
    entries = struct.unpack('<166I', image_bytes(elf, vector['address'], vector['size']))
    if (entries[0] != elf.symbol('__nx_msp_end') or
            entries[0] != elf.symbol('_estack') or entries[0] % 8 or
            not 0x24000000 < entries[0] <= 0x24080000 or
            entries[1] != elf.symbol('Reset_Handler') or
            elf.entry != entries[1] or
            entries[44] != elf.symbol('TIM2_IRQHandler')):
        raise EvidenceError('Actual H743 reset/time/MSP vector differs')
    selected_uart = any(s['kind'] == 'uart' for s in resolved['controllers'])
    for item in resolved['controllers']:
        if item['kind'] == 'exti':
            line = int(item['controller'][4:])
            if entries[22 + line] != elf.symbol('EXTI' + str(line) + '_IRQHandler'):
                raise EvidenceError('Selected EXTI vector differs from actual handler')
        if item['kind'] == 'uart' and item['mode'] in {'dma-tx', 'rx-dma-idle'}:
            stream = 0 if item['mode'] == 'dma-tx' else 1
            if entries[27 + stream] != elf.symbol('DMA1_Stream' + str(stream) + '_IRQHandler'):
                raise EvidenceError('Selected DMA vector differs from actual handler')
    if selected_uart and entries[55] != elf.symbol('USART3_IRQHandler'):
        raise EvidenceError('Selected USART3 vector differs from actual handler')
    names = {s['name'] for s in elf.defined_symbols}
    if names.intersection({'malloc', 'calloc', 'realloc', 'free', '_malloc_r',
                           'SystemCoreClock', 'SystemCoreClockUpdate'}):
        raise EvidenceError('Unexpected heap or stock clock global retained')
    for name in ('ExitRun0Mode', 'SystemInit', 'TIM2_IRQHandler'):
        definitions = [s for s in elf.defined_symbols if s['name'] == name]
        if len(definitions) != 1 or definitions[0]['binding'] != 1 or definitions[0]['type'] != 2:
            raise EvidenceError('One strong owned H743 function required')
    power = validate_power_entry(elf, disassembly)
    sram1 = any(item['id'] == 'sram1' and item['linker']
                for item in resolved['memory'])
    shapes = SRAM1_PRE_RAM_SHAPES if sram1 else PRE_RAM_SHAPES
    closure = [pre_ram_function(elf, name, disassembly, shapes=shapes)
               for name in shapes]
    optional_initialized = sram1_initialization(elf) if sram1 else {}
    reset = elf.symbol('Reset_Handler') & ~1
    literals = struct.unpack('<6I', image_bytes(elf, reset + 60, 24))
    if literals != tuple(elf.symbol(name) for name in (
            '_estack', '_sdata', '_edata', '_sidata', '_sbss', '_ebss')):
        raise EvidenceError('Reset DATA/BSS/MSP literals differ from ELF symbols')
    for start_name, end_name in (('_sdata', '_edata'), ('_sbss', '_ebss')):
        start, end = elf.symbol(start_name), elf.symbol(end_name)
        if start % 4 or end % 4 or not 0x24000000 <= start <= end <= 0x24080000:
            raise EvidenceError('Primary DATA/BSS left aligned selected AXI SRAM')
    if not 0x08000000 <= elf.symbol('_sidata') < 0x08200000:
        raise EvidenceError('Primary DATA initializer left actual Flash')
    initialized = primary_ram_initialization(elf)
    return {'vector_count': 166, 'selected_uart': selected_uart,
        'power_entry': power, 'pre_ram_closure': closure,
        'primary_ram_initialization': initialized,
        'optional_sram1_initialization': optional_initialized,
        'pre_supply_stack_stores': 0, 'pre_ram_initialized_global_accesses': 0,
        'after_supply_c_stack_bytes': 100 if sram1 else 8, 'physical_status': 'not_executed',
        'limitations': 'Reviewed compiler shapes, not an Arm emulator or measured cycles'}


def tool_inputs(root):
    """Bind maintained automation code/configuration, excluding raw capsules."""
    paths = set()
    for directory in ('tools', 'scripts'):
        paths.update(path for path in (root / directory).rglob('*')
            if path.is_file() and 'evidence' not in path.relative_to(root).parts
            and path.suffix in {'.py', '.json', '.toml', '.cmake', '.yml', '.yaml'})
    return [file_identity(path) for path in sorted(paths)]


def matrix_artifact(identity):
    """Adapt the shared ARM recorder's exact byte-count identity once."""
    fields(identity, {'path', 'bytes', 'sha256'})
    return verify_file_identity({'path': identity['path'], 'size': identity['bytes'],
                                 'sha256': identity['sha256']})


ASSEMBLIES = (
    'empty.toml', 'io.toml', 'empty-freertos.toml', 'io-freertos.toml',
    'dma.toml', 'dma32.toml', 'dma-cache.toml', 'dma32-cache.toml',
    'dma-freertos.toml', 'dma32-freertos.toml', 'dma-cache-freertos.toml',
    'dma32-cache-freertos.toml', 'rx.toml', 'rx32.toml', 'rx-cache.toml',
    'rx32-cache.toml', 'rx-freertos.toml', 'rx32-freertos.toml',
    'rx-cache-freertos.toml', 'rx32-cache-freertos.toml',
    'peripherals.toml', 'peripherals32.toml', 'peripherals-freertos.toml',
    'peripherals32-freertos.toml', 'rx-sram1.toml', 'rx-sram1-cache.toml')


def run(root, compiler, output, *, parallel=2):
    root, output = root.resolve(), Path(output).absolute()
    output = provider_assemblies.fresh_output(output, root)
    tool_input = file_identity(__file__)
    report = {'schema_version': 1, 'kind': 'stm32h743_reference_matrix',
        'status': 'failed', 'scope': 'actual_platform_cmake_arm_elf',
        'physical_status': 'not_executed', 'cycles_status': 'not_measured',
        'tool': tool_input, 'images': []}
    try:
        source_before = git_source(root)
        report['tool_inputs'] = tool_inputs(root)
        fixture = root / 'tests/contracts/h743_assembly'
        assemblies = [fixture / name for name in ASSEMBLIES]
        if provider_assemblies.run(root, assemblies, compiler, output / 'matrix',
                                   family='stm32h743', parallel=parallel):
            raise EvidenceError('Actual fresh H743 assembly matrix rejected')
        matrix = load_json(output / 'matrix/report.json', max_bytes=64 * 1024 * 1024)
        recorder = provider_assemblies.Recorder(output / 'inspection', root, timeout=30)
        report['commands'] = recorder.commands
        tools = arch_compile.tools(compiler)
        for index, row in enumerate(matrix['fixtures']):
            elf_path = matrix_artifact(row['artifacts']['elf'])
            resolved = load_json(matrix_artifact(row['artifacts']['resolved']))
            raw = recorder.execute([tools['objdump'], '-d', '-z', str(elf_path)],
                                   f'image-{index}-pre-ram')
            checks = validate_image(Elf32(elf_path.read_bytes()), resolved, raw)
            report['images'].append({'elf': row['artifacts']['elf'],
                'resolved': row['artifacts']['resolved'], 'checks': checks,
                'pre_ram_raw': recorder.commands[-1]['stdout']})
        for row in matrix['fixtures']:
            provider_assemblies.verify_inputs(row['compiler_dependencies'])
            provider_assemblies.verify_inputs(list(row['artifacts'].values()))
        provider_assemblies.verify_inputs(matrix['source_inputs'])
        if tool_inputs(root) != report['tool_inputs']:
            raise EvidenceError('Maintained automation inputs changed during matrix')
        if git_source(root) != source_before:
            raise EvidenceError('Git source identity changed during H743 inspection')
        verify_file_identity(tool_input)
        report['matrix'] = file_identity(output / 'matrix/report.json')
        report['source'] = source_before
        report['status'] = 'passed'
    except (OSError, ValueError, KeyError, TypeError) as error:
        report['error'] = str(error)
    atomic_json(output / 'report.json', report)
    return 0 if report['status'] == 'passed' else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--compiler', default='arm-none-eabi-gcc')
    parser.add_argument('--parallel', type=int, default=2)
    parser.add_argument('--elf', type=Path)
    parser.add_argument('--disassembly', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.elf is None:
        if args.disassembly is not None:
            parser.error('--disassembly requires --elf inspection')
        try:
            return run(args.root, args.compiler, args.output, parallel=args.parallel)
        except (OSError, ValueError) as error:
            print('H743 reference gate rejected: ' + str(error), file=sys.stderr)
            return 1
    if args.disassembly is None:
        parser.error('--elf inspection requires executed --disassembly')
    inputs = (args.elf, args.disassembly)
    if any(args.output.resolve() == path.resolve() or
           (args.output.exists() and args.output.samefile(path))
           for path in inputs):
        raise ValueError('A report must not overwrite an evidence input')
    data = args.elf.read_bytes()
    report = validate_power_entry(Elf32(data), args.disassembly.read_text())
    report['elf_sha256'] = hashlib.sha256(data).hexdigest()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
