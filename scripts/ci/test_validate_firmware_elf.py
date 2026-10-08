"""Fault models for the ELF checker; these do not execute ARM instructions."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from validate_firmware_elf import Elf32, FirmwareError, target_contract, validate_image
from package_release import effective_config

ROOT = Path(__file__).resolve().parents[2]
CONFIG = {
    'CONFIG_PLATFORM_NAME': 'gd32f470', 'CONFIG_PRODUCT_NAME': 'gd32f470-liangshan',
    'CONFIG_GD32F470ZG': True, 'CONFIG_GD32_UART_ENABLE': True,
    'CONFIG_OSAL_BACKEND_NAME': 'baremetal',
    'CONFIG_LINKER_RAM_START': 0x20000000, 'CONFIG_LINKER_RAM_SIZE': 0x30000,
    'CONFIG_LINKER_FLASH_START': 0x08000000, 'CONFIG_LINKER_FLASH_SIZE': 0x100000,
}


def fixture(defect='', config=None, descriptor_bytes=32):
    """Build structural data for any maintained profile; no ARM is executed."""
    config = CONFIG if config is None else config
    storage, flash_end, stack, vector_bytes, vector_name, irq = target_contract(config)
    data = bytearray(0x3500)
    addresses = {
        'Reset_Handler': 0x08000301, 'SystemInit': 0x08000311,
        vector_name: 0x08000000, '_estack': stack,
        '__nexus_storage_start': storage, '__nexus_storage_end': flash_end,
        '__nx_device_start': 0x08000500,
        '__nx_device_end': 0x08000500 + descriptor_bytes,
        'nx_device_descriptor_bytes': 0x080004F0,
    }
    for index, name in enumerate(irq.values()):
        addresses[name] = 0x08000321 + index * 16
    if config.get('CONFIG_BOARD_NAME') in ('stm32f407zg-qiming-v31',
                                          'stm32f407ve-sky-qingchun'):
        addresses['HAL_MspInit'] = 0x080003E1
    if defect in ('nonexecuting_irq', 'outside_flash_irq'):
        addresses['SysTick_Handler'] = 0x20000001 if defect == 'outside_flash_irq' else 0xDEAD0001
    if defect == 'nonfilebacked_irq':
        addresses['SysTick_Handler'] = 0x08000701
    if defect == 'reset_even':
        addresses['Reset_Handler'] &= ~1
    if defect == 'outside_flash_reset':
        addresses['Reset_Handler'] = 0x20000001
    if defect == 'storage_boundary':
        addresses['__nexus_storage_start'] += 4
    if defect == 'registry_boundary':
        addresses['__nx_device_end'] += 4
    words = [0] * (vector_bytes // 4)
    words[0], words[1] = stack, addresses['Reset_Handler']
    for index, name in irq.items():
        words[index] = addresses[name]
    if defect == 'stack':
        words[0] += 8
    if defect == 'reset_vector':
        words[1] += 4
    data[0x1000:0x1000 + vector_bytes] = struct.pack('<' + 'I' * len(words), *words)
    struct.pack_into('<I', data, 0x14F0, 0 if defect == 'invalid_abi' else descriptor_bytes)
    names = bytearray(b'\0')
    sections = [('', 0, 0, 0, 0, 0, 0, 0, 0, 0),
                ('.isr_vector', 1, 2, 0x08000000, 0x1000, vector_bytes, 0, 0, 4, 0),
                ('.text', 1, 6, 0x08000300, 0x1300, 0x100, 0, 0, 4, 0),
                ('.nx_device', 1, 2, 0x08000500, 0x1500, descriptor_bytes, 0, 0, 4, 0),
                ('.nx_abi', 1, 2, 0x080004F0, 0x14F0, 4, 0, 0, 4, 0),
                ('.shstrtab', 3, 0, 0, 0x2800, 0, 0, 0, 1, 0),
                ('.strtab', 3, 0, 0, 0x2900, 0, 0, 0, 1, 0),
                ('.symtab', 2, 0, 0, 0x2C00, 0, 6, 1, 4, 16)]
    strings = bytearray(b'\0')
    symbols = bytearray(b'\0' * 16)
    for name, address in addresses.items():
        name_offset = len(strings)
        strings.extend(name.encode() + b'\0')
        binding = 2 if name == 'Reset_Handler' else 1
        if defect == 'weak_irq' and name == 'SysTick_Handler':
            binding = 2
        is_abi = name == 'nx_device_descriptor_bytes'
        symbols.extend(struct.pack('<IIIBBH', name_offset, address, 4 if is_abi else 0,
                                   binding << 4 | (1 if is_abi else 2), 0, 4 if is_abi else 2))
    data[0x2900:0x2900 + len(strings)] = strings
    data[0x2C00:0x2C00 + len(symbols)] = symbols
    for index, section in enumerate(sections):
        name, *fields = section
        name_offset = len(names) if name else 0
        if name:
            names.extend(name.encode() + b'\0')
        if name == '.isr_vector' and defect == 'missing_vectors':
            fields[4] = 0
        if name == '.nx_device' and defect == 'partial_descriptor':
            fields[4] = descriptor_bytes - 1
        if name == '.nx_device' and defect == 'writable_registry':
            fields[1] |= 1
        if name == '.nx_device' and defect == 'unloaded_registry':
            fields[3] += 4
        if name == '.nx_abi' and defect == 'missing_abi':
            fields[4] = 0
        if name == '.nx_abi' and defect == 'truncated_abi':
            fields[3] = len(data)
        if name == '.strtab':
            fields[4] = len(strings)
        if name == '.symtab':
            fields[4] = len(symbols)
        struct.pack_into('<IIIIIIIIII', data, 0x3000 + index * 40, name_offset, *fields)
    data[0x2800:0x2800 + len(names)] = names
    struct.pack_into('<I', data, 0x3000 + 5 * 40 + 20, len(names))
    ident = b'\x7fELF\x01\x01\x01' + b'\0' * 9
    flags = 0x05000200 if defect == 'float_abi' else 0x05000400
    struct.pack_into('<16sHHIIIIIHHHHHH', data, 0, ident, 2, 40, 1,
                     addresses['Reset_Handler'], 52, 0x3000, flags,
                     52, 32, 2, 40, len(sections), 5)
    permissions = 7 if defect == 'rwx' else 5
    physical = storage if defect == 'storage_overlap' else 0x08000000
    struct.pack_into('<IIIIIIII', data, 52, 1, 0x1000, 0x08000000, physical,
                     0x700, 0x800 if defect == 'nonfilebacked_irq' else 0x700, permissions, 0x1000)
    ram_permissions = 5 if defect.startswith('outside_flash_') else 6
    struct.pack_into('<IIIIIIII', data, 84, 1, 0x2000, 0x20000000, 0x20000000,
                     0, 0x1000, ram_permissions, 0x1000)
    return bytes(data)


class FirmwareElfTests(unittest.TestCase):
    def test_complete_vendor_vectors_and_readonly_registry_are_accepted(self):
        report = validate_image(fixture(), CONFIG)
        self.assertEqual(report['vector_bytes'], 0x1AC)
        self.assertEqual(report['device_records'], 1)
        self.assertEqual(report['main_ram_reserved_bytes'], 4096)
        self.assertEqual(report['writable_executable_segments'], 0)
        self.assertIn('TIMER1_IRQHandler', report['strong_interrupts'])

    def test_hardware_linkage_and_memory_faults_are_rejected(self):
        for defect in ('stack', 'reset_vector', 'weak_irq', 'nonexecuting_irq',
                       'storage_boundary', 'registry_boundary', 'missing_vectors',
                       'partial_descriptor', 'writable_registry', 'float_abi',
                       'rwx', 'storage_overlap', 'missing_abi', 'invalid_abi',
                       'reset_even', 'outside_flash_reset', 'outside_flash_irq',
                       'unloaded_registry', 'truncated_abi', 'nonfilebacked_irq'):
            with self.subTest(defect=defect), self.assertRaises(FirmwareError):
                validate_image(fixture(defect), CONFIG)

    def test_descriptor_abi_comes_from_compiled_evidence(self):
        report = validate_image(fixture(descriptor_bytes=64), CONFIG)
        self.assertEqual(report['device_descriptor_bytes'], 64)
        self.assertEqual(report['device_records'], 1)

    def test_configuration_cannot_claim_a_different_physical_ram(self):
        with self.assertRaisesRegex(FirmwareError, 'contradicts'):
            validate_image(fixture(), {**CONFIG, 'CONFIG_LINKER_RAM_SIZE': 0x80000})

    def test_truncated_artifact_is_rejected_without_an_unbounded_read(self):
        for length in (0, 16, 51, 90, 0x3001):
            with self.subTest(length=length), self.assertRaises(FirmwareError):
                validate_image(fixture()[:length], CONFIG)

    def test_failed_cli_removes_a_previous_pass_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            (build / 'generated').mkdir()
            (build / 'bin').mkdir()
            lines = []
            for name, value in CONFIG.items():
                import json
                serialized = 'y' if value is True else json.dumps(value)
                lines.append(f'{name}={serialized}')
            (build / 'generated/effective.config').write_text('\n'.join(lines) + '\n')
            (build / 'bin/app.elf').write_bytes(fixture('missing_vectors'))
            report = build / 'report.json'
            report.write_text('{"status":"pass-from-old-build"}')
            result = subprocess.run([sys.executable, str(ROOT / 'scripts/ci/validate_firmware_elf.py'),
                                     '--build-dir', str(build), '--report', str(report)],
                                    text=True, capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Firmware static contract rejected', result.stderr)
            self.assertFalse(report.exists())


@unittest.skipUnless(os.environ.get('NEXUS_FIRMWARE_TEST_BUILD'),
                     'real ARM artifact mutation requires NEXUS_FIRMWARE_TEST_BUILD; ARM CI sets it')
class LinkedFirmwareRejectionTests(unittest.TestCase):
    def test_actual_linked_firmware_detects_vector_irq_segment_and_layout_corruption(self):
        build = Path(os.environ['NEXUS_FIRMWARE_TEST_BUILD'])
        config = effective_config((build / 'generated/effective.config').read_text())
        images = sorted((build / 'bin').glob('*.elf'))
        self.assertTrue(images, 'real ARM build must contain firmware')
        for image in images:
            contents = image.read_bytes()
            validate_image(contents, config)
            elf = Elf32(contents)
            header = elf.unpack('<16sHHIIIIIHHHHHH', 0)
            symbol_section = elf.sections['.symtab']
            strings = elf.section_data(elf.sections['.strtab'])
            mutations = {}
            missing_vectors = bytearray(contents)
            for index in range(header[12]):
                offset = header[6] + index * 40
                if elf.unpack('<IIIIIIIIII', offset) == elf.sections['.isr_vector']:
                    struct.pack_into('<I', missing_vectors, offset + 20, 0)
                    break
            else:
                self.fail('real artifact vector section header absent')
            mutations['missing_vectors'] = missing_vectors
            rwx = bytearray(contents)
            for index in range(header[10]):
                offset = header[5] + index * 32
                segment = elf.unpack('<IIIIIIII', offset)
                if segment[0] == 1 and segment[6] & 1:
                    struct.pack_into('<I', rwx, offset + 24, segment[6] | 2)
                    break
            else:
                self.fail('real artifact executable load absent')
            mutations['rwx'] = rwx
            for symbol_name, defect in (('SysTick_Handler', 'weak_irq'),
                                       ('__nexus_storage_start', 'storage_boundary')):
                corrupted = bytearray(contents)
                for offset in range(symbol_section[4], symbol_section[4] + symbol_section[5], 16):
                    symbol = elf.unpack('<IIIBBH', offset)
                    if elf.string(strings, symbol[0]) == symbol_name:
                        if defect == 'weak_irq':
                            corrupted[offset + 12] = 0x20 | (symbol[3] & 15)
                        else:
                            struct.pack_into('<I', corrupted, offset + 4, symbol[1] + 4)
                        break
                else:
                    self.fail('real artifact required symbol absent: ' + symbol_name)
                mutations[defect] = corrupted
            for defect, corrupted in mutations.items():
                with self.subTest(image=image.name, defect=defect), self.assertRaises(FirmwareError):
                    validate_image(bytes(corrupted), config)


if __name__ == '__main__':
    unittest.main()
