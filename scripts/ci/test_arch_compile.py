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
        domain = ("mrs r0, basepri\nmrs r1, faultmask\n"
                  if gate.profile(cpu).mainline else "")
        return ("mrs r0, primask\ncpsid i\nmsr primask, r0\n"
                "mrs r0, ipsr\nmrs r0, control\ndmb sy\ndsb sy\nisb sy\n"
                + domain)

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
                    bad = self.assembly(cpu).replace('mrs r0, basepri\n', '')
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
                            'mrs r0, control'):
            with self.subTest(instruction=instruction):
                with self.assertRaisesRegex(ValueError, 'required instruction'):
                    gate.check_instructions(
                        'cortex-m4', self.assembly('cortex-m4').replace(
                            instruction, ''))


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


if __name__ == '__main__':
    unittest.main()
