"""Failure-path regressions for the real Cortex-M Runtime graph audit."""
from pathlib import Path
import tempfile
import unittest

import cortex_runtime as gate


class RuntimeAuditTests(unittest.TestCase):
    def test_matrix_resolves_every_backend_without_board_identity(self):
        matrix = gate.matrix()
        self.assertEqual(len(matrix), 104)
        self.assertEqual(len({item['name'] for item in matrix}), 104)
        self.assertEqual({item['backend'] for item in matrix},
                         {'baremetal', 'freertos'})
        self.assertTrue(all('board' not in item['facts'] for item in matrix))

    def test_m0_runtime_rejects_libatomic_but_allows_its_irq_guard(self):
        gate.check_runtime_dependencies('00000000 T _start\n')
        with self.assertRaisesRegex(ValueError, 'atomic runtime helper'):
            gate.check_runtime_dependencies(
                '00000000 T __atomic_fetch_add_4\n')

    def test_linked_api_and_exception_retention_cannot_be_empty_or_partial(self):
        complete = '\n'.join('00000000 T ' + name for name in
                             gate.required_symbols('freertos'))
        gate.check_retention(complete, 'freertos')
        for missing in ('nx_request_result', 'nx_arch_sau_write',
                        'PendSV_Handler', 'vTaskStartScheduler'):
            with self.assertRaisesRegex(ValueError, 'retained symbols'):
                gate.check_retention(complete.replace(missing, 'wrong'),
                                     'freertos')

    def test_fpu_and_mve_context_evidence_matches_exact_profile(self):
        variant = next(item for item in gate.matrix()
                       if item['backend'] == 'freertos' and
                       item['facts']['cpu']['mve'] == 'integer' and
                       item['facts']['cpu']['fpu'] == 'none')
        context = ('vstmdb r0!, {s16-s31}\nvldmia r0!, {s16-s31}\n'
                   'tst lr, #16\n')
        gate.check_kernel_context(variant, context)
        for bad in ('', context.replace('vldmia', 'nop'),
                    context.replace('tst lr, #16', '')):
            with self.assertRaisesRegex(ValueError, 'coprocessor context'):
                gate.check_kernel_context(variant, bad)
        plain = next(item for item in gate.matrix()
                     if item['backend'] == 'freertos' and
                     item['facts']['cpu']['arch'] == 'cortex-m0')
        with self.assertRaisesRegex(ValueError, 'unexpected coprocessor'):
            gate.check_kernel_context(plain, context)

    def test_instruction_probes_require_actual_retained_cpu_instructions(self):
        variant = next(item for item in gate.matrix()
                       if item['facts']['cpu']['mve'] == 'float')
        with self.assertRaisesRegex(ValueError, 'floating probe'):
            gate.check_probes(variant, '')

    def test_fixture_inputs_are_one_authored_toml_and_exact_facts(self):
        with tempfile.TemporaryDirectory() as value:
            directory = Path(value)
            variant = gate.matrix()[0]
            assembly = gate.write_inputs(directory, variant)
            self.assertEqual(assembly.name, 'runtime.toml')
            self.assertIn('cpu_facts = "cpu.json"', assembly.read_text())
            import json
            self.assertEqual(json.loads((directory / 'cpu.json').read_text()),
                             variant['facts'])

    def test_generated_m7_kernel_source_is_part_of_the_actual_graph(self):
        with tempfile.TemporaryDirectory() as value:
            directory = Path(value)
            root, build = directory / 'source', directory / 'build'
            root.mkdir()
            build.mkdir()
            variant = next(item for item in gate.matrix()
                           if item['backend'] == 'freertos' and
                           item['facts']['cpu']['arch'] == 'cortex-m7' and
                           item['facts']['cpu']['fpu'] == 'none')
            names = ('core/src/request.c', 'core/src/time.c',
                     'arch/cortex_m/nx_arch_cortex_m.c',
                     'arch/cortex_m/nx_arch_cache.c',
                     'arch/cortex_m/nx_arch_mpu.c',
                     'arch/cortex_m/nx_arch_security.c', 'os/wait.c')
            sources = [root / name for name in names] + [
                build / 'nexus-runtime/os/freertos/m7-integer-port/port.c']
            entries = []
            for index, source in enumerate(sources):
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_text('int actual_database_boundary;\n')
                obj = build / f'actual-{index}.o'
                obj.write_bytes(b'object boundary')
                entries.append({'directory': str(build), 'file': str(source),
                                'arguments': ['arm-none-eabi-gcc',
                                              *variant['profile']['compile_options'],
                                              '-o', str(obj), '-c', str(source)]})
            import json
            (build / 'compile_commands.json').write_text(json.dumps(entries))
            objects = gate.compiled_objects(root, build, variant)
            self.assertEqual(len(objects), len(sources))
            self.assertEqual(objects[-1]['source']['path'], str(sources[-1]))
            # A caller CFLAGS value must not establish a second ABI truth.
            original = entries[0]['arguments'][:]
            for extra in ('-march=armv7e-m', '-mcpu=cortex-m7',
                          '-mfpu=fpv5-d16', '-mfloat-abi=softfp', '-mcmse',
                          '-fno-short-enums', '-fshort-enums', '-marm'):
                entries[0]['arguments'] = [*original, extra]
                (build / 'compile_commands.json').write_text(json.dumps(entries))
                with self.assertRaisesRegex(ValueError, 'CPU ABI'):
                    gate.compiled_objects(root, build, variant)
            entries[0]['arguments'] = [*original, '-Os', '-Wall']
            (build / 'compile_commands.json').write_text(json.dumps(entries))
            gate.compiled_objects(root, build, variant)

    def test_kernel_port_source_must_match_the_resolved_contract(self):
        root, build = Path('/reference/source'), Path('/reference/build')
        variant = next(item for item in gate.matrix()
                       if item['backend'] == 'freertos' and
                       item['facts']['cpu']['arch'] == 'cortex-m55' and
                       item['facts']['cpu']['fpu'] == 'none' and
                       item['facts']['cpu']['mve'] == 'none')
        path = root / 'ext/freertos/portable/GCC/ARM_CM55_NTZ/non_secure'
        objects = [{'source': {'path': str(path / name)}}
                   for name in ('port.c', 'portasm.c')]
        gate.check_kernel_sources(root, build, variant, objects)
        with self.assertRaisesRegex(ValueError, 'kernel port source'):
            gate.check_kernel_sources(root, build, variant, objects[:1])

    def test_mve_integer_context_requires_actual_coprocessor_initialization(self):
        variant = next(item for item in gate.matrix()
                       if item['backend'] == 'freertos' and
                       item['facts']['cpu']['mve'] == 'integer' and
                       item['facts']['cpu']['fpu'] == 'none')
        # Real GCC patterns: a literal CPACR pointer and base+offset FPCCR.
        initialization = (
            '08002384 <vPortEnableVFP>:\n'
            ' 8002384: f8df 000c ldr.w r0, [pc, #12] @ 8002394\n'
            ' 8002388: 6801 ldr r1, [r0, #0]\n'
            ' 800238a: f441 0170 orr.w r1, r1, #15728640\n'
            ' 800238e: 6001 str r1, [r0, #0]\n'
            ' 8002390: 4770 bx lr\n'
            ' 8002394: e000ed88 .word 0xe000ed88\n'
            '080024a8 <xPortStartScheduler>:\n'
            ' 8002574: f04f 24e0 mov.w r4, #3758153728\n'
            ' 80025a6: f8d4 3f34 ldr.w r3, [r4, #3892]\n'
            ' 80025aa: f043 4340 orr.w r3, r3, #3221225472\n'
            ' 80025ae: f8c4 3f34 str.w r3, [r4, #3892]\n')
        gate.check_coprocessor_initialization(variant, initialization)
        for bad in (initialization.replace('6001 str', '6001 nop'),
                    initialization.replace('#3221225472', '#0'),
                    initialization.replace('#3892', '#3896'),
                    initialization.replace('f8c4 3f34 str.w',
                                           '2400 movs r4, #0\n'
                                           ' 80025ae: f8c4 3f34 str.w')):
            with self.assertRaisesRegex(ValueError, 'coprocessor initialization'):
                gate.check_coprocessor_initialization(variant, bad)

    def test_inert_mmio_literals_do_not_prove_coprocessor_initialization(self):
        variant = next(item for item in gate.matrix()
                       if item['facts']['cpu']['fpu'] != 'none')
        with self.assertRaisesRegex(ValueError, 'coprocessor initialization'):
            gate.check_coprocessor_initialization(variant, (
                '08001000 <unrelated_unused_literals>:\n'
                ' 8001000: e000ed88 .word 0xe000ed88\n'
                ' 8001004: e000ef34 .word 0xe000ef34\n'))

    def test_m7_integer_kernel_requires_actual_saved_mask_errata_guards(self):
        variant = next(item for item in gate.matrix()
                       if item['profile']['irq']['kernel_port'] ==
                       'nexus/ARM_CM7_integer')
        guard = (' 8001000: f04f 0350 mov.w r3, #80\n'
                 ' 8001004: f3ef 8210 mrs r2, primask\n'
                 ' 8001008: b672 cpsid i\n'
                 ' 800100a: f383 8811 msr basepri, r3\n'
                 ' 800100e: f3bf 8f4f dsb sy\n'
                 ' 8001012: f3bf 8f6f isb sy\n'
                 ' 8001016: f382 8810 msr primask, r2\n')
        functions = ('vPortEnterCritical', 'SysTick_Handler', 'PendSV_Handler')
        text = ''.join('08001000 <' + name + '>:\n' + guard
                       for name in functions)
        gate.check_kernel_context(variant, text)
        for name in functions:
            for bad_guard in (guard.replace('cpsid i', 'nop i'),
                              guard.replace('msr primask, r2', 'cpsie i'),
                              guard.replace('isb sy', 'nop sy'),
                              guard.replace('#80', '#64'),
                              guard.replace('msr primask, r2',
                                            'movs r2, #0\n'
                                            ' 8001016: f382 8810 msr primask, r2')):
                with self.assertRaisesRegex(ValueError, 'M7 integer errata'):
                    gate.check_kernel_context(variant, text.replace(
                        '08001000 <' + name + '>:\n' + guard,
                        '08001000 <' + name + '>:\n' + bad_guard))

    def test_changed_production_input_cannot_qualify_a_mixed_matrix(self):
        with tempfile.TemporaryDirectory() as value:
            file = Path(value) / 'actual.c'
            file.write_text('int first_source;\n')
            before = [gate.compiler_gate.digest(file)]
            gate.verify_source_inputs(before)
            file.write_text('int different_source;\n')
            with self.assertRaisesRegex(ValueError, 'source changed'):
                gate.verify_source_inputs(before)


if __name__ == '__main__':
    unittest.main()
