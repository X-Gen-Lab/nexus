"""Real ARM byte/control-flow mutations of the H743 pre-RAM acceptance gate."""
import copy
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from scripts.ci.stm32h743 import validate_power_entry, validate_memory_facts
from tools.measurement.elf import Elf32
from scripts.ci import stm32h743
from tools.configure.configure import resolve_ir


class PowerGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = shutil.which('arm-none-eabi-gcc')
        cls.objdump = shutil.which('arm-none-eabi-objdump')
        if not cls.compiler or not cls.objdump:
            raise RuntimeError('Actual ARM compiler/objdump required; no skips')
        cls.source = (ROOT / 'soc/stm32/stm32h743/power_boot.S').read_text()

    def build(self, source):
        temporary = tempfile.TemporaryDirectory(prefix='h743-power-contract-')
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        assembly = directory / 'power.S'
        assembly.write_text(source)
        elf = directory / 'power.elf'
        command = [self.compiler, '-mcpu=cortex-m7', '-mthumb',
                   '-mfpu=fpv5-d16', '-mfloat-abi=hard', '-nostdlib',
                   '-Wl,-e,ExitRun0Mode', '-Wl,-Ttext=0x08000000',
                   '-Wl,--build-id=none',
                   '-I' + str(ROOT / 'soc/stm32/stm32h743/private'),
                   str(assembly), '-o', str(elf)]
        compiled = subprocess.run(command, capture_output=True, timeout=15)
        self.assertEqual(compiled.returncode, 0, compiled.stderr.decode())
        decoded = subprocess.run([self.objdump, '-d', str(elf)],
                                 capture_output=True, timeout=15)
        self.assertEqual(decoded.returncode, 0, decoded.stderr.decode())
        return Elf32(elf.read_bytes()), decoded.stdout.decode()

    def rejects(self, source):
        elf, raw = self.build(source)
        with self.assertRaises(ValueError):
            validate_power_entry(elf, raw)

    def test_actual_leaf(self):
        elf, raw = self.build(self.source)
        report = validate_power_entry(elf, raw)
        self.assertEqual(report['ram_stores'], 0)
        self.assertEqual(report['nested_calls'], 0)
        self.assertEqual(report['poll_limit'], 1000000)
        self.assertEqual(report['mmio_stores'], 2)
        self.assertEqual(report['axi_workaround'], {
            'register': 0x51008108, 'read_iss_override': 1,
            'revision_limit_exclusive': 0x2000, 'readback_required': True,
            'phase': 'after_power_ready_before_ram'})
        self.assertEqual(report['evidence_kind'],
                         'actual_elf_control_flow_not_physical_execution')

    def test_ram_store_rejected(self):
        self.rejects(self.source.replace('str r1, [r0]', 'str r1, [sp]'))

    def test_unconditional_ready_return_rejected(self):
        self.rejects(self.source.replace('bne .Lready', 'b .Lready'))

    def test_non_decreasing_counter_rejected(self):
        self.rejects(self.source.replace('subs r2, r2, #1', 'nop'))

    def test_failure_without_irq_mask_rejected(self):
        self.rejects(self.source.replace('cpsid i', 'nop'))

    def test_older_revision_cannot_bypass_the_axi_write(self):
        self.rejects(self.source.replace('bhs .Lboot_complete', 'b .Lboot_complete'))

    def test_axi_write_and_readback_guard_cannot_be_removed(self):
        for replacement in ('orr r1, r1, #NX_H743_BOOT_AXI_READ',
                            'tst r1, #NX_H743_BOOT_AXI_READ',
                            'bne .Lboot_complete'):
            with self.subTest(instruction=replacement):
                self.assertIn(replacement, self.source)
                self.rejects(self.source.replace(replacement, 'nop'))

    def test_axi_store_barrier_order_and_failed_readback_are_closed(self):
        self.rejects(self.source.replace(
            'str r1, [r0]\n    dsb sy\n    ldr r1, [r0]\n    tst r1, #NX_H743_BOOT_AXI_READ',
            'dsb sy\n    str r1, [r0]\n    ldr r1, [r0]\n    tst r1, #NX_H743_BOOT_AXI_READ'))
        self.rejects(self.source.replace('movs r0, #3\n    b .Lfailed',
                                        'movs r0, #3\n    bx lr'))

    def test_unreviewed_value_mutation_rejected(self):
        self.rejects(self.source.replace('bic r1, r1, #NX_H743_BOOT_BYPASS',
                                        'and r1, r1, #0\n'
                                        '    bic r1, r1, #NX_H743_BOOT_BYPASS'))

    def test_missing_store_barrier_rejected(self):
        self.rejects(self.source.replace('str r1, [r0]\n    dsb sy',
                                        'dsb sy\n    str r1, [r0]'))

    def test_counterfeit_disassembly_rejected(self):
        elf, raw = self.build(self.source)
        with self.assertRaises(ValueError):
            validate_power_entry(elf, raw.replace('f644 000c', 'f644 000d'))

    def test_counterfeit_mnemonic_for_changed_machine_bytes_rejected(self):
        elf, raw = self.build(self.source)
        entry = elf.symbol('ExitRun0Mode') & ~1
        section = next(item for item in elf.sections
                       if item['address'] <= entry <
                       item['address'] + item['size'])
        offset = section['offset'] + entry - section['address']
        changed = bytearray(elf.data)
        changed[offset + 2] ^= 1
        forged = raw.replace('f644 000c', 'f644 000d')
        with self.assertRaises(ValueError):
            validate_power_entry(Elf32(bytes(changed)), forged)

    def test_report_cannot_overwrite_elf_input(self):
        elf, raw = self.build(self.source)
        with tempfile.TemporaryDirectory(prefix='h743-input-contract-') as temp:
            directory = Path(temp)
            image, decoded = directory / 'image.elf', directory / 'decoded.txt'
            image.write_bytes(elf.data)
            decoded.write_text(raw)
            checked = subprocess.run(
                [sys.executable, str(ROOT / 'scripts/ci/stm32h743.py'),
                 '--elf', str(image), '--disassembly', str(decoded),
                 '--output', str(image)], capture_output=True, timeout=15)
            self.assertNotEqual(checked.returncode, 0)
            self.assertEqual(image.read_bytes(), elf.data)


class MemoryFacts(unittest.TestCase):
    def setUp(self):
        facts = json.loads((ROOT / 'soc/stm32/stm32h743/soc.json').read_text())
        self.resolved = {
            'part': 'STM32H743ZIT6',
            'memory': copy.deepcopy(facts['variants']['STM32H743ZIT6']['memory'])}

    def test_actual_chip_and_all_bus_facts(self):
        self.assertIsNone(validate_memory_facts(self.resolved))

    def test_only_explicit_sram1_is_additional_initialized_memory(self):
        self.resolved['memory_budgets'] = {'regions': {'sram1': {'limit_bytes': 256}}}
        self.resolved['memory'][3]['linker'] = True
        self.assertEqual('sram1', self.resolved['memory'][3]['id'])
        self.assertIsNone(validate_memory_facts(self.resolved))
        self.resolved['memory'][4]['linker'] = True
        with self.assertRaises(ValueError):
            validate_memory_facts(self.resolved)

    def test_omitted_physical_domain_rejected(self):
        self.resolved['memory'].pop()
        with self.assertRaises(ValueError):
            validate_memory_facts(self.resolved)

    def test_shifted_domain_or_unselected_placement_rejected(self):
        for field, value in [('origin', 0x20000004), ('linker', True)]:
            with self.subTest(field=field):
                changed = copy.deepcopy(self.resolved)
                changed['memory'][1][field] = value
                with self.assertRaises(ValueError):
                    validate_memory_facts(changed)

    def test_tcm_mdma_reachability_cannot_be_erased(self):
        self.resolved['memory'][1]['dma_engines']['MDMA'] = False
        with self.assertRaises(ValueError):
            validate_memory_facts(self.resolved)

    def test_engine_map_cannot_be_replaced_by_boolean(self):
        for region in self.resolved['memory']:
            region.pop('dma_engines')
            region['dma'] = False
        with self.assertRaises(ValueError):
            validate_memory_facts(self.resolved)

    def test_extra_engine_or_wrong_density_rejected(self):
        for change in ['engine', 'density']:
            with self.subTest(change=change):
                changed = copy.deepcopy(self.resolved)
                if change == 'engine':
                    changed['memory'][0]['dma_engines']['nearest-DMA'] = True
                else:
                    changed['part'] = 'STM32H743ZGT6'
                with self.assertRaises(ValueError):
                    validate_memory_facts(changed)


class ReferenceProfile(unittest.TestCase):
    def setUp(self):
        self.resolved = resolve_ir(
            ROOT / 'tests/contracts/h743_assembly/empty.toml', ROOT).to_dict()

    def test_exact_maintained_profile(self):
        self.assertIsNone(stm32h743.validate_profile(self.resolved))

    def test_exact_new_peripheral_and_sram1_profiles(self):
        for name in ('peripherals', 'peripherals32', 'peripherals-freertos',
                     'peripherals32-freertos', 'rx-sram1', 'rx-sram1-cache'):
            with self.subTest(name=name):
                resolved = resolve_ir(ROOT / (
                    'tests/contracts/h743_assembly/' + name + '.toml'), ROOT)
                self.assertIsNone(stm32h743.validate_profile(resolved.to_dict()))

    def test_integer_cannot_substitute_for_boolean_cpu_fact(self):
        self.resolved['cpu']['dsp'] = 1
        with self.assertRaises(ValueError):
            stm32h743.validate_profile(self.resolved)

    def test_other_board_part_or_backend_is_rejected(self):
        for key, value in [('board', 'stm32h743-custom'),
                           ('part', 'STM32H743ZGT6'), ('backend', 'native')]:
            with self.subTest(key=key):
                changed = copy.deepcopy(self.resolved)
                changed[key] = value
                with self.assertRaises(ValueError):
                    stm32h743.validate_profile(changed)

    def test_guessed_clock_source_cannot_qualify_hsi_reference(self):
        self.resolved['clock']['source'] = 'hse'
        with self.assertRaises(ValueError):
            stm32h743.validate_profile(self.resolved)

    def test_clock_counts_cannot_use_float_numeric_equality(self):
        for key in ('input_hz', 'core_hz', 'apb1_hz', 'apb2_hz'):
            changed = copy.deepcopy(self.resolved)
            changed['clock'][key] = float(changed['clock'][key])
            with self.subTest(field=key):
                with self.assertRaises(ValueError):
                    stm32h743.validate_profile(changed)

    def test_soft_float_or_unreviewed_cache_profile_is_rejected(self):
        for key, value in [('float_abi', 'soft'), ('dcache_line_bytes', 0)]:
            with self.subTest(key=key):
                changed = copy.deepcopy(self.resolved)
                changed['cpu'][key] = value
                with self.assertRaises(ValueError):
                    stm32h743.validate_profile(changed)

    def test_unsupported_controller_cannot_enter_reference_matrix(self):
        self.resolved['controllers'] = [
            {'kind': 'spi', 'controller': 'SPI1', 'mode': 'poll'}]
        with self.assertRaises(ValueError):
            stm32h743.validate_profile(self.resolved)


class ToolInputs(unittest.TestCase):
    def test_all_maintained_tool_sources_exclude_historical_capsules(self):
        with tempfile.TemporaryDirectory(prefix='h743-tools-inputs-') as temporary:
            root = Path(temporary)
            for name in ('tools/maintenance/lock.py', 'tools/hil/fixture.json',
                         'scripts/ci/reference.py',
                         'tools/testing/evidence/source/old.py'):
                p = root / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text('{}\n')
            rows = stm32h743.tool_inputs(root)
            self.assertEqual({str(root / path) for path in (
                'tools/maintenance/lock.py', 'tools/hil/fixture.json',
                'scripts/ci/reference.py')}, {r['path'] for r in rows})



class StartupClosure(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler, objdump = (shutil.which(name) for name in
                             ('arm-none-eabi-gcc', 'arm-none-eabi-objdump'))
        if not compiler or not objdump:
            raise RuntimeError('Actual ARM compiler and objdump are mandatory')
        cls.temporary = tempfile.TemporaryDirectory(prefix='h743-startup-contract-')
        cls.addClassCleanup(cls.temporary.cleanup)
        directory = Path(cls.temporary.name)
        generated = directory / 'generated'
        assembly = ROOT / 'tests/contracts/h743_assembly/empty.toml'
        if getattr(cls, 'optional_sram1', False):
            authored = assembly.read_text().replace(
                '../../../boards/stm32h743_nucleo_zi2',
                str(ROOT / 'boards/stm32h743_nucleo_zi2'))
            assembly = directory / 'assembly.toml'
            assembly.write_text(authored + '\n[memory.regions.sram1]\nlimit_bytes=256\n')
        configured = subprocess.run([sys.executable,
            str(ROOT / 'tools/configure/configure.py'), '--source-root', str(ROOT),
            '--assembly', str(assembly),
            '--output', str(generated)], capture_output=True, timeout=30)
        if configured.returncode:
            raise RuntimeError(configured.stderr.decode())
        cls.resolved = json.loads((generated / 'resolved.json').read_text())
        # Only the external application entry is a test fixture. Startup,
        # supply entry, SystemInit, barriers and libc constructors are real.
        main = directory / 'main.c'
        main.write_text('#include \"nexus/core/memory.h\"\n'
            + ('NX_MEMORY_SRAM1_DATA unsigned boot_data = 0x12345678U;\n'
               'NX_MEMORY_SRAM1_BSS unsigned boot_bss;\n'
               'NX_MEMORY_SRAM1_NOINIT unsigned boot_noinit;\n'
               if getattr(cls, 'optional_sram1', False) else '')
            + 'void _init(void) {}\nvoid _fini(void) {}\n'
            + ('int main(void) { boot_noinit = 0; return boot_data + boot_bss + boot_noinit; }\n'
               if getattr(cls, 'optional_sram1', False) else
               'int main(void) { return 0; }\n'))
        image = directory / 'startup.elf'
        command = [compiler, '-mcpu=cortex-m7', '-mthumb', '-mfpu=fpv5-d16',
            '-mfloat-abi=hard', '-fshort-enums', '-Os', '-ffunction-sections',
            '-fdata-sections', '-DNEXUS_ARCH_HAS_DWT_CYCCNT=1', '-DSTM32H743xx',
            '-I' + str(generated),
            '-I' + str(ROOT / 'soc/stm32/stm32h743/private'),
            '-I' + str(ROOT / 'core/include'), '-I' + str(ROOT / 'arch/include'),
            '-I' + str(ROOT / 'vendors/st/cmsis_device_h7/Include'),
            '-I' + str(ROOT / 'vendors/arm/CMSIS_5/CMSIS/Core/Include'),
            str(ROOT / 'soc/stm32/stm32h743/system.c'),
            str(ROOT / 'soc/stm32/stm32h743/memory_init.c'),
            str(ROOT / 'soc/stm32/stm32h743/power_boot.S'),
            str(ROOT / 'vendors/st/cmsis_device_h7/Source/Templates/gcc/startup_stm32h743xx.s'),
            str(ROOT / 'arch/arm/cortex_m/nx_arch_cortex_m.c'),
            str(ROOT / 'core/src/fault.c'), str(main), '-nostartfiles',
            '-Wl,--gc-sections', '-Wl,--build-id=none',
            '-T' + str(generated / 'memory.ld'), '-lc', '-lgcc', '-o', str(image)]
        if not getattr(cls, 'optional_sram1', False):
            command.remove(str(ROOT / 'soc/stm32/stm32h743/memory_init.c'))
        compiled = subprocess.run(command, capture_output=True, timeout=30)
        if compiled.returncode:
            raise RuntimeError(compiled.stderr.decode())
        decoded = subprocess.run([objdump, '-d', '-z', str(image)],
                                 capture_output=True, timeout=30)
        if decoded.returncode:
            raise RuntimeError(decoded.stderr.decode())
        cls.command = command
        cls.main = main
        cls.elf = Elf32(image.read_bytes())
        cls.disassembly = decoded.stdout.decode()
        cls.objdump = objdump

    def setUp(self):
        self.resolved = copy.deepcopy(type(self).resolved)

    def offset(self, address):
        section = next(s for s in self.elf.sections if s['kind'] != 8 and
                       s['address'] <= address < s['address'] + s['size'])
        return section['offset'] + address - section['address']

    def rejects_bytes(self, address, replacement):
        data = bytearray(self.elf.data)
        offset = self.offset(address)
        data[offset:offset + len(replacement)] = replacement
        with self.assertRaises(ValueError):
            stm32h743.validate_image(Elf32(bytes(data)), self.resolved,
                                    self.disassembly)

    def test_real_startup_full_closure(self):
        report = stm32h743.validate_image(self.elf, self.resolved,
                                         self.disassembly)
        self.assertEqual(report['vector_count'], 166)
        self.assertEqual(report['pre_ram_initialized_global_accesses'], 0)
        self.assertEqual(report['pre_supply_stack_stores'], 0)

    def test_wrong_time_vector_is_rejected(self):
        self.rejects_bytes(0x08000000 + 44 * 4, struct.pack('<I',
                           self.elf.symbol('Default_Handler')))

    def test_elf_entry_cannot_bypass_actual_reset_supply_path(self):
        data = bytearray(self.elf.data)
        struct.pack_into('<I', data, 24, self.elf.symbol('main'))
        with self.assertRaises(ValueError):
            stm32h743.validate_image(Elf32(bytes(data)), self.resolved,
                                    self.disassembly)

    def test_ram_store_before_power_entry_is_rejected(self):
        self.rejects_bytes(self.elf.symbol('Reset_Handler') & ~1,
                           bytes.fromhex('00bf0090'))

    def test_system_init_before_power_is_rejected(self):
        address = self.elf.symbol('Reset_Handler') & ~1
        offset = self.offset(address)
        self.rejects_bytes(address + 4, self.elf.data[offset + 8:offset + 12])

    def test_c_level_initialized_global_literal_is_rejected(self):
        address = self.elf.symbol('SystemInit') & ~1
        self.rejects_bytes(address + 48, struct.pack('<I', 0x24000000))

    def test_barrier_dependency_cannot_be_silently_replaced(self):
        address = self.elf.symbol('nx_arch_dsb') & ~1
        self.rejects_bytes(address, bytes.fromhex('00bf00bf'))

    def test_stack_literal_cannot_disagree_with_actual_msp(self):
        address = self.elf.symbol('Reset_Handler') & ~1
        self.rejects_bytes(address + 60, struct.pack('<I', 0x20020000))

    def coherent_symbol_literal(self, name, index, replacement):
        data = bytearray(self.elf.data)
        address = self.elf.symbol('Reset_Handler') & ~1
        struct.pack_into('<I', data, self.offset(address + 60 + index * 4), replacement)
        header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
        sections = [struct.unpack_from('<IIIIIIIIII', data, header[6] + i * 40)
                    for i in range(header[12])]
        modified = 0
        for section in sections:
            if section[1] != 2:
                continue
            table = sections[section[6]]
            strings = data[table[4]:table[4] + table[5]]
            for offset in range(section[4], section[4] + section[5], 16):
                symbol = struct.unpack_from('<IIIBBH', data, offset)
                end = strings.index(0, symbol[0])
                if bytes(strings[symbol[0]:end]) == name.encode():
                    struct.pack_into('<I', data, offset + 4, replacement)
                    modified += 1
        self.assertEqual(1, modified)
        with tempfile.TemporaryDirectory(prefix='h743-stack-contract-') as temp:
            image = Path(temp) / 'changed-stack.elf'
            image.write_bytes(data)
            decoded = subprocess.run([self.objdump, '-d', '-z', str(image)],
                                     capture_output=True, timeout=30)
            self.assertEqual(0, decoded.returncode, decoded.stderr.decode())
            return Elf32(bytes(data)), decoded.stdout.decode()


    def test_coherent_stack_symbol_and_literal_cannot_escape_vector_msp(self):
        elf, raw = self.coherent_symbol_literal('_estack', 0, 0x20020000)
        with self.assertRaises(ValueError):
            stm32h743.validate_image(elf, self.resolved, raw)

    def test_coherent_data_initializer_must_match_actual_load_bytes(self):
        elf, raw = self.coherent_symbol_literal('_sidata', 3, 0x08000000)
        with self.assertRaises(ValueError):
            stm32h743.validate_image(elf, self.resolved, raw)

    def test_coherent_data_start_must_cover_the_actual_section(self):
        elf, raw = self.coherent_symbol_literal('_sdata', 1,
                                               self.elf.symbol('_sdata') + 4)
        with self.assertRaises(ValueError):
            stm32h743.validate_image(elf, self.resolved, raw)

    def test_coherent_bss_end_cannot_zero_beyond_actual_section(self):
        elf, raw = self.coherent_symbol_literal('_ebss', 5,
                                               self.elf.symbol('_ebss') + 4)
        with self.assertRaises(ValueError):
            stm32h743.validate_image(elf, self.resolved, raw)


class OptionalStartupClosure(StartupClosure):
    optional_sram1 = True

    def test_complete_optional_closure_and_real_data_zero_noinit_sections(self):
        report = stm32h743.validate_image(self.elf, self.resolved,
                                         self.disassembly)
        self.assertEqual(100, report['after_supply_c_stack_bytes'])
        self.assertEqual(7, len(report['pre_ram_closure']))
        for kind in ('data', 'bss', 'noinit'):
            self.assertEqual(4, report['optional_sram1_initialization']
                             ['.nx_sram1_' + kind]['bytes'])

    def test_empty_data_and_noinit_linker_sections_require_no_copy(self):
        with tempfile.TemporaryDirectory(prefix='h743-empty-sram1-') as temp:
            directory = Path(temp)
            main, image = directory / 'main.c', directory / 'empty.elf'
            main.write_text('#include "nexus/core/memory.h"\n'
                'NX_MEMORY_SRAM1_BSS unsigned payload;\n'
                'void _init(void) {}\nvoid _fini(void) {}\n'
                'int main(void) { return payload; }\n')
            command = [str(main) if part == str(self.main) else part
                       for part in self.command]
            command[-1] = str(image)
            compiled = subprocess.run(command, capture_output=True, timeout=30)
            self.assertEqual(0, compiled.returncode, compiled.stderr.decode())
            decoded = subprocess.run([self.objdump, '-d', '-z', str(image)],
                                     capture_output=True, timeout=30)
            self.assertEqual(0, decoded.returncode, decoded.stderr.decode())
            report = stm32h743.validate_image(Elf32(image.read_bytes()),
                self.resolved, decoded.stdout.decode())
            initialized = report['optional_sram1_initialization']
            self.assertEqual(0, initialized['.nx_sram1_data']['bytes'])
            self.assertEqual(0, initialized['.nx_sram1_noinit']['bytes'])
            self.assertEqual(4, initialized['.nx_sram1_bss']['bytes'])

    def test_descriptor_pointer_cannot_point_into_mutable_ram(self):
        boot = self.elf.symbol('nx_stm32h743_memory_boot') & ~1
        self.rejects_bytes(boot + 56, struct.pack('<I', 0x24000000))

    def test_descriptor_words_cannot_silently_widen_copy_or_zero(self):
        boot = self.elf.symbol('nx_stm32h743_memory_boot') & ~1
        table, = struct.unpack('<I', stm32h743.image_bytes(self.elf, boot + 56, 4))
        for index in (2, 4, 6, 8):
            word, = struct.unpack('<I', stm32h743.image_bytes(
                self.elf, table + index * 4, 4))
            with self.subTest(word=index):
                self.rejects_bytes(table + index * 4, struct.pack('<I', word + 4))

    def test_actual_sram1_gate_enable_and_readback_cannot_be_removed(self):
        system = self.elf.symbol('SystemInit') & ~1
        for offset in (56, 64):
            with self.subTest(offset=offset):
                self.rejects_bytes(system + offset, bytes.fromhex('00bf00bf'))

    def test_source_closed_word_copy_and_zero_cannot_be_removed(self):
        initialize = self.elf.symbol('nx_stm32h743_memory_initialize') & ~1
        for offset in (174, 180):
            with self.subTest(offset=offset):
                self.rejects_bytes(initialize + offset, bytes.fromhex('00bf00bf'))


class MatrixArtifact(unittest.TestCase):
    def test_actual_shared_matrix_bytes_identity_is_accepted(self):
        with tempfile.TemporaryDirectory(prefix='h743-artifact-') as temporary:
            path = Path(temporary) / 'input.json'
            path.write_text('{}\n')
            identity = stm32h743.arch_compile.digest(path)
            self.assertEqual(path, stm32h743.matrix_artifact(identity))

    def test_mixed_size_authorities_and_actual_tampering_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix='h743-artifact-') as temporary:
            path = Path(temporary) / 'input.json'
            path.write_text('{}\n')
            identity = stm32h743.arch_compile.digest(path)
            with self.assertRaises(ValueError):
                stm32h743.matrix_artifact({**identity, 'size': identity['bytes']})
            path.write_text('[]\n')
            with self.assertRaises(ValueError):
                stm32h743.matrix_artifact(identity)



if __name__ == '__main__':
    unittest.main()
