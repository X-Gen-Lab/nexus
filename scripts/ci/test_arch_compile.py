"""Cortex-M compiler gate policy and execution boundary regressions."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import arch_compile as gate


class CortexCompilerPolicyTests(unittest.TestCase):
    def macros(self, cpu):
        profile = gate.profile(cpu)
        return (
            f"#define {profile.arch_macro} 1\n"
            "#define __GNUC__ 14\n"
            "#define __ARM_ARCH_PROFILE 77\n"
            "#define __SIZEOF_INT__ 4\n"
            "#define __SOFTFP__ 1\n"
            f"#define __GCC_ATOMIC_INT_LOCK_FREE {profile.atomic_lock_free}\n"
            + ('#define __ARM_FEATURE_DSP 1\n' if
               gate.reference_facts(cpu)['cpu']['dsp'] else '')
        )

    def assembly(self, cpu):
        return self.function_assembly(cpu)

    def test_scope_is_exact_and_unknown_cpu_fails_closed(self):
        self.assertEqual(tuple(gate.PROFILES), (
            'cortex-m0', 'cortex-m0plus', 'cortex-m3', 'cortex-m4',
            'cortex-m7', 'cortex-m23', 'cortex-m33', 'cortex-m55', 'cortex-m85'))
        for cpu in ('cortex-m52', 'cortex-a7', '', 'cortex-m4 -O0'):
            with self.assertRaisesRegex(ValueError, 'unsupported CPU'):
                gate.profile(cpu)

    def test_actual_target_macro_and_soft_abi_are_required(self):
        for cpu in gate.PROFILES:
            with self.subTest(cpu=cpu):
                gate.check_macros(cpu, self.macros(cpu))
                invalid = self.macros(cpu).replace(
                    gate.profile(cpu).arch_macro, '__UNRELATED_ARCH__')
                with self.assertRaisesRegex(ValueError, 'architecture macro'):
                    gate.check_macros(cpu, invalid)
                with self.assertRaisesRegex(ValueError, 'soft-float ABI'):
                    gate.check_macros(
                        cpu, self.macros(cpu) + '#define __ARM_PCS_VFP 1\n')

    def test_m23_exclusives_are_not_conflated_with_m0(self):
        self.assertEqual(gate.profile('cortex-m23').atomic_lock_free, 2)
        gate.check_atomic_symbols('cortex-m23', '')
        for cpu in ('cortex-m0', 'cortex-m0plus'):
            gate.check_atomic_symbols(cpu, '         U __atomic_fetch_add_4\n')
            with self.assertRaisesRegex(ValueError, 'atomic helper'):
                gate.check_atomic_symbols(cpu, '')
        with self.assertRaisesRegex(ValueError, 'atomic helper'):
            gate.check_atomic_symbols(
                'cortex-m23', '         U __atomic_fetch_add_4\n')

    def test_declared_abi_and_optional_instruction_macros_must_match(self):
        hard = self.macros('cortex-m55').replace(
            '#define __SOFTFP__ 1\n', '') + (
                '#define __ARM_PCS_VFP 1\n#define __ARM_FP 14\n'
                '#define __ARM_FEATURE_MVE 3\n')
        gate.check_macros('cortex-m55', hard, fpu='auto', float_abi='hard',
                          mve='float')
        for argument in ({'float_abi': 'softfp'}, {'mve': 'integer'},
                         {'fpu': 'none'}):
            values = {'fpu': 'auto', 'float_abi': 'hard', 'mve': 'float'}
            values.update(argument)
            with self.assertRaisesRegex(ValueError, 'ABI|FPU|MVE'):
                gate.check_macros('cortex-m55', hard, **values)

    def test_optional_dsp_instruction_macros_must_match_declared_facts(self):
        nodsp = self.macros('cortex-m33').replace(
            '#define __ARM_FEATURE_DSP 1\n', '')
        enabled = nodsp + '#define __ARM_FEATURE_DSP 1\n'
        gate.check_macros('cortex-m33', nodsp, dsp=False)
        gate.check_macros('cortex-m33', enabled, dsp=True)
        for facts, actual in ((False, enabled), (True, nodsp)):
            with self.assertRaisesRegex(ValueError, 'DSP instruction'):
                gate.check_macros('cortex-m33', actual, dsp=facts)

    def test_software_matrix_covers_declared_fpus_and_security_without_boards(self):
        variants = gate.software_profiles()
        self.assertEqual(len({item['name'] for item in variants}), len(variants))
        self.assertEqual({item['facts']['cpu']['arch'] for item in variants},
                         set(gate.PROFILES))
        for cpu in ('cortex-m55', 'cortex-m85'):
            scoped = [item['facts']['cpu'] for item in variants
                      if item['facts']['cpu']['arch'] == cpu]
            self.assertEqual({facts['mve'] for facts in scoped},
                             {'none', 'integer', 'float'})
            self.assertEqual({facts['security'] for facts in scoped},
                             {'single', 'secure', 'nonsecure'})
        optional = [item['facts']['cpu'] for item in variants
                    if item['facts']['cpu']['arch'] == 'cortex-m33']
        self.assertEqual(len(optional), 10)
        self.assertEqual({item['dsp'] for item in optional}, {False, True})
        for enabled in (False, True):
            self.assertEqual({item['float_abi'] for item in optional
                              if item['dsp'] is enabled},
                             {'soft', 'hard', 'softfp'})
        self.assertTrue(all('board' not in item['facts'] for item in variants))

    def test_wrong_atomic_fact_or_unexpected_helpers_fail(self):
        with self.assertRaisesRegex(ValueError, 'atomic lock-free'):
            gate.check_macros('cortex-m4', self.macros('cortex-m4').replace(
                '__GCC_ATOMIC_INT_LOCK_FREE 2',
                '__GCC_ATOMIC_INT_LOCK_FREE 1'))
        for cpu in gate.PROFILES:
            with self.assertRaisesRegex(ValueError, 'atomic helper'):
                gate.check_atomic_symbols(cpu, '         U malloc\n')
        with self.assertRaisesRegex(ValueError, 'external dependency'):
            gate.check_arch_symbols('         U __aeabi_memcpy\n')

    def test_baseline_masks_are_excluded_and_mainline_masks_required(self):
        for cpu in gate.PROFILES:
            with self.subTest(cpu=cpu):
                gate.check_instructions(cpu, self.assembly(cpu))
                if gate.profile(cpu).mainline:
                    bad = self.assembly(cpu).replace('mrs r2, basepri\n', '')
                else:
                    bad = self.assembly(cpu) + 'mrs r0, basepri\n'
                with self.assertRaisesRegex(ValueError, 'mask register'):
                    gate.check_instructions(cpu, bad)

    def test_mask_queries_cannot_overwrite_other_mask_domains(self):
        for register in ('basepri', 'basepri_max', 'faultmask'):
            with self.assertRaisesRegex(ValueError, 'mask write'):
                gate.check_instructions(
                    'cortex-m4', self.assembly('cortex-m4') +
                    f'msr {register}, r0\n')

    def test_arch_exports_are_complete_without_hidden_helpers(self):
        exported = ''.join(f'00000000 T {name}\n'
                           for name in gate.PUBLIC_SYMBOLS)
        gate.check_exports(exported)
        for invalid in (exported.replace('nx_arch_exception_number', 'wrong'),
                        exported + '00000000 T runtime_dispatch\n'):
            with self.assertRaisesRegex(ValueError, 'public symbol scope'):
                gate.check_exports(invalid)

    def test_unavailable_counter_object_does_not_access_memory(self):
        body = ('00000000 <nx_arch_cycle_snapshot>:\n'
                ' 0: 2000 movs r0, #0\n 2: 4770 bx lr\n')
        gate.check_disabled_counter(body)
        for instruction in ('ldr r0, [r1]', 'str r0, [r1]'):
            with self.assertRaisesRegex(ValueError, 'unavailable cycle counter'):
                gate.check_disabled_counter(body + ' 4: 6808 ' +
                                            instruction + '\n')

    def test_required_barrier_and_irq_instructions_cannot_disappear(self):
        for instruction in ('cpsid i', 'dmb sy', 'dsb sy', 'isb sy',
                            'mrs r1, control'):
            with self.subTest(instruction=instruction):
                with self.assertRaisesRegex(ValueError, 'required instruction'):
                    gate.check_instructions(
                        'cortex-m4', self.assembly('cortex-m4').replace(
                            instruction, ''))

    def function_assembly(self, cpu='cortex-m4'):
        priority = ('mrs r2, basepri\nmrs r3, faultmask\n'
                    if gate.profile(cpu).mainline else '')
        bodies = {
            'nx_arch_irq_save': 'mrs r0, primask\ncpsid i\ndsb sy\nisb sy\nbx lr',
            'nx_arch_irq_restore': 'dsb sy\nmsr primask, r0\nisb sy\nbx lr',
            'nx_arch_irq_masks': 'mrs r1, primask\n' + priority + 'bx lr',
            'nx_arch_irq_is_masked': 'mrs r0, primask\nbx lr',
            'nx_arch_exception_number': 'mrs r0, ipsr\nbx lr',
            'nx_arch_is_privileged': 'mrs r0, ipsr\nmrs r1, control\nbx lr',
            'nx_arch_in_isr': 'mrs r0, ipsr\nbx lr',
            'nx_arch_dmb': 'dmb sy\nbx lr',
            'nx_arch_dsb': 'dsb sy\nbx lr',
            'nx_arch_isb': 'isb sy\nbx lr',
            'nx_arch_cycle_snapshot': 'movs r0, #0\nbx lr',
        }
        return ''.join(f'.type {name}, %function\n{name}:\n{body}\n'
                       f'.size {name}, .-{name}\n' for name, body in bodies.items())

    def test_per_function_irq_order_cannot_borrow_another_functions_barrier(self):
        text = self.function_assembly()
        gate.check_instructions('cortex-m4', text)
        bad = text.replace('nx_arch_irq_restore:\ndsb sy\n',
                           'nx_arch_irq_restore:\n')
        with self.assertRaisesRegex(ValueError, 'irq_restore.*barrier|irq_restore.*order'):
            gate.check_instructions('cortex-m4', bad)
        swapped = text.replace('mrs r0, primask\ncpsid i',
                               'cpsid i\nmrs r0, primask')
        with self.assertRaisesRegex(ValueError, 'irq_save.*order'):
            gate.check_instructions('cortex-m4', swapped)

    def test_function_audit_reads_actual_objdump_without_counting_literal_data(self):
        text = ('00000000 <nx_probe>:\n'
                ' 0: f3ef 8010 mrs r0, PRIMASK\n'
                ' 4: f3bf 8f4f dsb sy\n'
                ' 8: 4770 bx lr\n'
                ' a: bf00 nop\n'
                ' c: e000ed14 .word 0xe000ed14\n')
        metrics = gate.function_costs(text)
        self.assertEqual(metrics['nx_probe']['static_instructions'], 4)
        self.assertFalse(metrics['nx_probe']['wcet_established'])

    def test_real_header_atomic_probe_has_explicit_helper_and_mask_contract(self):
        gate.check_nexus_atomic_symbols('cortex-m23', '')
        gate.check_nexus_atomic_symbols('cortex-m0',
            ' U nx_arch_irq_save\n U nx_arch_irq_restore\n')
        with self.assertRaisesRegex(ValueError, 'production atomic'):
            gate.check_nexus_atomic_symbols('cortex-m23',
                ' U __atomic_compare_exchange_4\n')
        with self.assertRaisesRegex(ValueError, 'production atomic'):
            gate.check_nexus_atomic_symbols('cortex-m0', '')

    def atomic_assembly(self, cpu):
        functions = {}
        for suffix in ('load_relaxed', 'load_acquire', 'store_relaxed',
                       'store_release'):
            load = suffix.startswith('load')
            body = 'ldr r0, [r0]' if load else 'str r1, [r0]'
            if suffix == 'load_acquire':
                body += '\ndmb sy'
            if suffix == 'store_release':
                body = 'dmb sy\n' + body
            functions['nx_probe_atomic_' + suffix] = body + '\nbx lr'
        for suffix in ('fetch_add_acq_rel', 'fetch_add_release',
                       'fetch_sub_acq_rel', 'fetch_sub_release',
                       'compare_exchange_acq_rel'):
            body = ('bl nx_arch_irq_save\nldr r4, [r5]\n'
                    'str r0, [r5]\nbl nx_arch_irq_restore\nbx lr'
                    if gate.profile(cpu).atomic_lock_free == 1 else
                    'dmb sy\nldrex r2, [r0]\nstrex r3, r1, [r0]\n'
                    'cmp r3, #0\nbne 0b\ndmb sy\nbx lr')
            functions['nx_probe_atomic_' + suffix] = body
        return ''.join(f'.type {name}, %function\n{name}:\n{body}\n'
                       f'.size {name}, .-{name}\n'
                       for name, body in functions.items())

    def test_production_atomic_instruction_audit_is_per_operation(self):
        for cpu in gate.PROFILES:
            text = self.atomic_assembly(cpu)
            gate.check_nexus_atomic_instructions(cpu, text)
            if gate.profile(cpu).atomic_lock_free == 1:
                bad = text.replace('bl nx_arch_irq_restore', 'nop', 1)
            else:
                bad = text.replace('strex r3, r1, [r0]', 'str r1, [r0]', 1)
            with self.assertRaisesRegex(ValueError, 'atomic.*RMW'):
                gate.check_nexus_atomic_instructions(cpu, bad)
        text = self.atomic_assembly('cortex-m23')
        with self.assertRaisesRegex(ValueError, 'atomic.*publication'):
            gate.check_nexus_atomic_instructions('cortex-m23', text.replace(
                'nx_probe_atomic_load_relaxed:\n',
                'nx_probe_atomic_load_relaxed:\ncpsid i\n'))
        with self.assertRaisesRegex(ValueError, 'atomic.*ordering'):
            gate.check_nexus_atomic_instructions('cortex-m23', text.replace(
                'nx_probe_atomic_store_release:\ndmb sy',
                'nx_probe_atomic_store_release:\nnop'))

    def test_machine_relocations_bind_each_irq_fallback_call(self):
        text = self.atomic_assembly('cortex-m0')
        for operation in ('save', 'restore'):
            text = text.replace('bl nx_arch_irq_' + operation,
                '0: f7ff fffe bl 0 <nx_probe_atomic_fetch_add_acq_rel>\n'
                '0: R_ARM_THM_CALL nx_arch_irq_' + operation)
        gate.check_nexus_atomic_instructions('cortex-m0', text)
        with self.assertRaisesRegex(ValueError, 'atomic.*RMW'):
            gate.check_nexus_atomic_instructions('cortex-m0', text.replace(
                'R_ARM_THM_CALL nx_arch_irq_restore',
                'R_ARM_THM_CALL nx_arch_irq_save', 1))


class CortexCompilerExecutionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.output = self.root / 'evidence'

    def test_missing_compiler_fails_with_structured_evidence(self):
        self.assertEqual(gate.run(
            Path(__file__).resolve().parents[2],
            str(self.root / 'missing arm gcc'), self.output), 1)
        import json
        report = json.loads((self.output / 'report.json').read_text())
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(report['full_platform_status'], 'not_qualified')
        self.assertIn('compiler', report['error'])
        self.assertEqual(report['profiles'], [])

    def test_empty_version_or_failed_process_cannot_pass(self):
        recorder = gate.Recorder(self.output)
        for code, stdout in ((0, b''), (73, b'compiler crashed')):
            with self.subTest(code=code), patch.object(
                    gate.subprocess, 'run', return_value=
                    subprocess.CompletedProcess(['tool'], code, stdout, b'')):
                with self.assertRaisesRegex(ValueError, 'command failed|empty'):
                    recorder.execute(['tool', '--version'], 'version',
                                     nonempty=True)
        self.assertEqual(len(recorder.commands), 2)
        self.assertEqual(recorder.commands[1]['returncode'], 73)
        self.assertTrue(all(Path(command['stdout']['path']).is_file()
                            for command in recorder.commands))

    def test_process_error_is_recorded_without_fake_returncode(self):
        recorder = gate.Recorder(self.output)
        with patch.object(gate.subprocess, 'run', side_effect=
                          FileNotFoundError('no actual tool')):
            with self.assertRaisesRegex(ValueError, 'execution failed'):
                recorder.execute(['missing tool'], 'missing')
        self.assertIsNone(recorder.commands[0]['returncode'])
        self.assertIn('no actual tool', recorder.commands[0]['error'])

    def test_negative_compile_crashes_are_not_contract_rejections(self):
        recorder = gate.Recorder(self.output)
        with patch.object(gate.subprocess, 'run', return_value=
                          subprocess.CompletedProcess(
                              ['tool'], -11, b'', b'compiler crashed')):
            with self.assertRaisesRegex(ValueError, 'command failed'):
                recorder.execute(['tool'], 'crash', reject=True)

    def test_independent_cmsis_reference_rejects_changed_production_address(self):
        import os
        root = Path(__file__).resolve().parents[2]
        selected = gate.tools(os.environ.get('NEXUS_ARM_GCC',
                                             'arm-none-eabi-gcc'))
        source = root / 'tests/contracts/arch_cmsis_layout_probe.c'
        for cpu in gate.PROFILES:
            with self.subTest(cpu=cpu):
                command = gate.cmsis_layout_command(
                    root, selected['gcc'], cpu, source,
                    self.root / (cpu + '.o'))
                completed = subprocess.run(command, capture_output=True,
                                           text=True, timeout=30)
                self.assertEqual(completed.returncode, 0, completed.stderr)
        private = self.root / 'private'
        private.mkdir()
        original = root / 'arch/arm/cortex_m/private/registers.h'
        (private / original.name).write_text(original.read_text().replace(
            '0xe000ed14U', '0xe000ed18U'))
        command = gate.cmsis_layout_command(root, selected['gcc'], 'cortex-m7',
                                            source, self.root / 'changed.o')
        command[1:1] = ['-I', str(self.root)]
        completed = subprocess.run(command, capture_output=True, text=True,
                                   timeout=30)
        self.assertEqual(completed.returncode, 1)
        self.assertIn('CMSIS register address', completed.stderr)


if __name__ == '__main__':
    unittest.main()
