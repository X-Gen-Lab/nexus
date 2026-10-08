"""Check actual ARM firmware vectors, ownership sections and physical memory.

Static artifact validation only: this does not execute the MCU or establish HIL.
The parser reads ELF32 directly, so no host disassembler/toolchain is required.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from package_release import effective_config, ReleaseError


class FirmwareError(ValueError):
    pass


class Elf32:
    def __init__(self, contents):
        self.contents = contents
        header = self.unpack('<16sHHIIIIIHHHHHH', 0)
        if (header[0][:7] != b'\x7fELF\x01\x01\x01' or header[1] != 2 or
                header[2] != 40 or header[3] != 1 or header[8] != 52):
            raise FirmwareError('Expected a little-endian ARM ELF32 executable')
        self.entry = header[4]
        if not self.entry & 1 or header[7] & 0x600 != 0x400:
            raise FirmwareError('Firmware must have a Thumb entry and hard-float ABI')
        if header[9] != 32 or header[11] != 40 or not header[10] or not header[12]:
            raise FirmwareError('Missing or invalid ELF program/section tables')
        self.segments = [self.unpack('<IIIIIIII', header[5] + index * 32)
                         for index in range(header[10])]
        sections = [self.unpack('<IIIIIIIIII', header[6] + index * 40)
                    for index in range(header[12])]
        if header[13] >= len(sections):
            raise FirmwareError('Invalid section name table')
        names = self.section_data(sections[header[13]])
        self.sections = {}
        self.symbols = {}
        for section in sections:
            name = self.string(names, section[0])
            if name and name in self.sections:
                raise FirmwareError('Duplicate ELF section name')
            if name:
                self.sections[name] = section
            if section[1] != 2:
                continue
            if section[6] >= len(sections) or section[9] != 16 or section[5] % 16:
                raise FirmwareError('Invalid ELF symbol table')
            strings = self.section_data(sections[section[6]])
            for offset in range(section[4], section[4] + section[5], 16):
                symbol = self.unpack('<IIIBBH', offset)
                name = self.string(strings, symbol[0])
                if name and symbol[5] and symbol[3] >> 4 in (1, 2):
                    if name in self.symbols:
                        raise FirmwareError('Duplicate defined global symbol')
                    self.symbols[name] = symbol

    def unpack(self, layout, offset):
        size = struct.calcsize(layout)
        if offset < 0 or offset + size > len(self.contents):
            raise FirmwareError('Truncated ELF structure')
        return struct.unpack_from(layout, self.contents, offset)

    def section_data(self, section):
        offset, size = section[4:6]
        if section[1] == 8 or offset + size > len(self.contents):
            raise FirmwareError('Invalid file-backed ELF section')
        return self.contents[offset:offset + size]

    @staticmethod
    def string(table, offset):
        if offset >= len(table):
            raise FirmwareError('Invalid ELF string offset')
        end = table.find(b'\0', offset)
        if end < 0:
            raise FirmwareError('Unterminated ELF string')
        try:
            return table[offset:end].decode('ascii')
        except UnicodeDecodeError as error:
            raise FirmwareError('Non-ASCII ELF identity') from error

    def symbol(self, name, strong=False):
        symbol = self.symbols.get(name)
        if not symbol or (strong and symbol[3] >> 4 != 1):
            raise FirmwareError(f'Missing strong symbol: {name}')
        return symbol[1]

    def executable(self, address):
        address &= ~1
        return any(segment[0] == 1 and segment[6] & 1 and
                   segment[2] <= address < segment[2] + segment[4]
                   for segment in self.segments)


def target_contract(config):
    platform = config.get('CONFIG_PLATFORM_NAME')
    if platform == 'stm32':
        flash = config.get('CONFIG_STM32_FLASH_SIZE')
        if flash not in (0x80000, 0x100000):
            raise FirmwareError('Unsupported STM32F407 physical Flash identity')
        flash_end = 0x08000000 + flash
        storage_start = flash_end - 0x40000
        ram_end = 0x20020000
        vector_bytes, vector_name = 0x188, 'g_pfnVectors'
        irq = {}
        for index, symbol in enumerate(('USART1_IRQHandler', 'USART2_IRQHandler', 'USART3_IRQHandler')):
            if config.get(f'CONFIG_INSTANCE_STM32_UART_{index}'):
                irq[53 + index] = symbol
        if config.get('CONFIG_STM32_SPI1_ENABLE'):
            irq[51] = 'SPI1_IRQHandler'
            if config.get('CONFIG_STM32_SPI_USE_DMA'):
                irq[72] = 'DMA2_Stream0_IRQHandler'
                irq[75] = 'DMA2_Stream3_IRQHandler'
    elif platform == 'gd32f470' and config.get('CONFIG_GD32F470ZG'):
        flash_end, storage_start, ram_end = 0x08100000, 0x080FC000, 0x20030000
        vector_bytes, vector_name = 0x1AC, '__gVectors'
        irq = {44: 'TIMER1_IRQHandler'}
        if config.get('CONFIG_GD32_UART_ENABLE'):
            irq[53] = 'USART0_IRQHandler'
    else:
        raise FirmwareError('No maintained firmware contract for the effective platform')
    irq[15] = 'SysTick_Handler'
    if config.get('CONFIG_OSAL_BACKEND_NAME') == 'freertos':
        irq.update({11: 'SVC_Handler', 14: 'PendSV_Handler'})
    if (config.get('CONFIG_LINKER_RAM_START') != 0x20000000 or
            config.get('CONFIG_LINKER_RAM_SIZE') != ram_end - 0x20000000 or
            config.get('CONFIG_LINKER_FLASH_START') != 0x08000000 or
            config.get('CONFIG_LINKER_FLASH_SIZE') != flash_end - 0x08000000):
        raise FirmwareError('Effective configuration contradicts the physical memory contract')
    return storage_start, flash_end, ram_end, vector_bytes, vector_name, irq


def validate_image(contents, config):
    elf = Elf32(contents)
    storage, flash_end, ram_end, vector_bytes, vector_name, irq = target_contract(config)

    def retained_constant(section):
        if (not section or section[1] != 1 or section[2] & 1 or
                not section[2] & 2 or not section[5] or
                section[3] < 0x08000000 or section[3] + section[5] > storage):
            return False
        # Section metadata alone cannot prove the bytes enter the firmware.
        # Require the same address/offset mapping in a read-only Flash load.
        elf.section_data(section)
        return any(segment[0] == 1 and not segment[6] & 2 and
                   segment[2] <= section[3] and
                   section[3] + section[5] <= segment[2] + segment[4] and
                   segment[3] == segment[2] and
                   section[4] == segment[1] + section[3] - segment[2]
                   for segment in elf.segments)

    vectors = elf.sections.get('.isr_vector')
    if (not vectors or vectors[3] != 0x08000000 or vectors[5] != vector_bytes or
            not retained_constant(vectors)):
        raise FirmwareError('Real vendor vector table is missing, writable or has the wrong size')
    if elf.symbol(vector_name) != 0x08000000:
        raise FirmwareError('Vendor vector symbol is not at physical Flash start')
    words = struct.unpack('<' + 'I' * (vector_bytes // 4), elf.section_data(vectors))
    # Official startup deliberately permits a weak Reset_Handler override.
    # Its resolved vector/entry identity matters; required IRQs must be strong.
    reset = elf.symbol('Reset_Handler')
    if (words[0] != ram_end or elf.symbol('_estack') != ram_end or
            words[1] != elf.entry or (reset & ~1) != (elf.entry & ~1)):
        raise FirmwareError('Vector stack/reset does not match the retained startup and RAM')
    def firmware_function(address):
        return bool(address & 1 and 0x08000000 <= (address & ~1) < storage and
                    elf.executable(address))

    if not firmware_function(reset) or not firmware_function(elf.symbol('SystemInit', strong=True)):
        raise FirmwareError('Startup or silicon initialization is outside executable firmware')
    board_hooks = []
    if config.get('CONFIG_BOARD_NAME') in ('stm32f407zg-qiming-v31',
                                          'stm32f407ve-sky-qingchun'):
        if not firmware_function(elf.symbol('HAL_MspInit', strong=True)):
            raise FirmwareError('Customer board safe-output initialization is not retained')
        board_hooks.append('HAL_MspInit')
    strong_irqs = []
    for index, name in sorted(irq.items()):
        address = elf.symbol(name, strong=True)
        if (not words[index] & 1 or (words[index] & ~1) != (address & ~1) or
                not firmware_function(address)):
            raise FirmwareError(f'Interrupt vector does not bind its strong handler: {name}')
        strong_irqs.append(name)
    if (elf.symbol('__nexus_storage_start') != storage or
            elf.symbol('__nexus_storage_end') != flash_end):
        raise FirmwareError('Storage partition symbols do not match the physical part')
    abi = elf.sections.get('.nx_abi')
    abi_symbol = elf.symbols.get('nx_device_descriptor_bytes')
    if (not retained_constant(abi) or not abi_symbol or
            abi_symbol[2] != 4 or abi_symbol[1] < abi[3] or
            abi_symbol[1] + 4 > abi[3] + abi[5]):
        raise FirmwareError('Compiler-derived device descriptor ABI evidence is missing')
    descriptor_bytes, = struct.unpack_from('<I', elf.section_data(abi),
                                           abi_symbol[1] - abi[3])
    if not descriptor_bytes or descriptor_bytes % 4:
        raise FirmwareError('Compiler-derived device descriptor ABI is invalid')
    registry = elf.sections.get('.nx_device')
    if (not retained_constant(registry) or registry[5] % descriptor_bytes or
            elf.symbol('__nx_device_start') != registry[3] or
            elf.symbol('__nx_device_end') != registry[3] + registry[5]):
        raise FirmwareError('Constant complete device registration records were not retained')
    flash_used = ram_used = 0
    loads = [segment for segment in elf.segments if segment[0] == 1]
    if not loads:
        raise FirmwareError('Firmware has no loadable memory')
    for segment in loads:
        _, offset, address, physical, files, memory, flags, _ = segment
        if files > memory or offset + files > len(contents) or flags & 3 == 3:
            raise FirmwareError('Invalid load segment or writable executable memory')
        if files:
            if physical < 0x08000000 or physical + files > storage:
                raise FirmwareError('Firmware load bytes overlap storage or leave application Flash')
            flash_used = max(flash_used, physical + files - 0x08000000)
        if memory:
            if 0x08000000 <= address and address + memory <= storage:
                if flags & 2:
                    raise FirmwareError('A physical Flash segment is writable')
            elif 0x20000000 <= address and address + memory <= ram_end:
                ram_used = max(ram_used, address + memory - 0x20000000)
            else:
                raise FirmwareError('Default profile allocates an unsupported memory domain')
    return {'entry': elf.entry, 'vector_bytes': vector_bytes,
            'strong_interrupts': strong_irqs,
            'board_initializers': board_hooks,
            'device_descriptor_bytes': descriptor_bytes,
            'device_records': registry[5] // descriptor_bytes,
            'application_flash_bytes': storage - 0x08000000,
            'flash_loaded_bytes': flash_used, 'main_ram_bytes': ram_end - 0x20000000,
            'main_ram_reserved_bytes': ram_used, 'storage_start': storage,
            'storage_end': flash_end, 'writable_executable_segments': 0}


def inspect_build(build):
    fragment = build / 'generated/effective.config'
    config = effective_config(fragment.read_text())
    images = sorted((build / 'bin').glob('*.elf'))
    if not images:
        raise FirmwareError('No linked ARM firmware images')
    report = {'schema_version': 1, 'kind': 'arm-static-link-contract',
              'hardware_verified': False, 'platform': config['CONFIG_PLATFORM_NAME'],
              'product': config['CONFIG_PRODUCT_NAME'],
              'config_sha256': hashlib.sha256(fragment.read_bytes()).hexdigest(), 'images': []}
    for path in images:
        contents = path.read_bytes()
        report['images'].append({'file': path.name,
                                'sha256': hashlib.sha256(contents).hexdigest(),
                                **validate_image(contents, config)})
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    try:
        args.report.unlink(missing_ok=True)
        report = inspect_build(args.build_dir.resolve())
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError, KeyError, struct.error, ReleaseError) as error:
        parser.exit(1, f'Firmware static contract rejected: {error}\n')
    print(f"Validated {len(report['images'])} ARM images; physical execution remains unverified")


if __name__ == '__main__':
    main()
