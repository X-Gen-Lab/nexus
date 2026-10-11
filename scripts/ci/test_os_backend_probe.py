"""Onboarding rejects unsupported policies and missing actual contract cases."""
import copy
from pathlib import Path
import json
import tempfile
from unittest.mock import patch
import unittest

import os_backend_probe as probe


def reference():
    source = Path(__file__).resolve().parents[2]
    return json.loads((source / 'tests/contracts/os_backend_probe/native.json')
                      .read_text())


class BackendContractTests(unittest.TestCase):
    def test_native_reference_is_a_probe_without_support_promotion(self):
        contract = reference()
        self.assertEqual(probe.validate_contract(contract), contract)
        self.assertFalse(contract['backend']['claim_support'])

    def test_unknown_keys_and_bool_schema_do_not_change_contract(self):
        for path, field, value in (((), 'schema_version', True),
                                   ((), 'extra', False),
                                   (('backend',), 'compatibility_fallback', True),
                                   (('contract',), 'deadline', 'relative_ticks')):
            candidate = reference()
            nested = candidate
            for key in path:
                nested = nested[key]
            nested[field] = value
            with self.assertRaises(ValueError):
                probe.validate_contract(candidate)

    def test_no_manifest_can_register_an_unreviewed_backend(self):
        candidate = reference()
        candidate['backend']['claim_support'] = True
        with self.assertRaises(ValueError):
            probe.validate_contract(candidate)

    def test_hidden_workers_or_unsafe_reclaim_are_rejected(self):
        for key, value in (('hidden_workers', True),
                           ('notification_reclaim', 'request_settled'),
                           ('task_reclaim', 'entry_returned'),
                           ('queue_shutdown', 'destroy_without_drain'),
                           ('clock_continues_while_waiting', False),
                           ('hint_is_result', True)):
            candidate = reference()
            candidate['contract'][key] = value
            with self.assertRaises(ValueError):
                probe.validate_contract(candidate)

    def test_task_context_and_mask_policy_must_match_host_backend(self):
        for key, value in (('mask_policy', 'basepri'),
                           ('irq_policy', 'configurable_irq'),
                           ('incoming_masks', 'cleared')):
            candidate = reference()
            candidate['contract'][key] = value
            with self.assertRaises(ValueError):
                probe.validate_contract(candidate)

    def test_missing_empty_duplicate_or_filtered_case_sets_fail(self):
        for mutation in ('missing', 'empty', 'duplicate', 'wildcard'):
            candidate = reference()
            if mutation == 'missing':
                del candidate['requirements']['absolute_deadline']
            elif mutation == 'empty':
                candidate['requirements']['absolute_deadline'] = []
            elif mutation == 'duplicate':
                candidate['requirements']['absolute_deadline'] *= 2
            else:
                candidate['requirements']['absolute_deadline'] = ['NativeWait.*']
            with self.assertRaises(ValueError):
                probe.validate_contract(candidate)

    def test_host_contract_cannot_invent_kernel_evidence(self):
        candidate = reference()
        candidate['kernel'] = {'lock': 'invented.json'}
        with self.assertRaises(ValueError):
            probe.validate_contract(candidate)

    def test_kernel_candidate_requires_mask_tests_and_locked_real_kernel(self):
        candidate = reference()
        candidate['backend'].update(name='candidate-rtos', kind='kernel')
        candidate['contract'].update(mask_policy='primask',
                                     irq_policy='configurable_irq',
                                     incoming_masks='preserved',
                                     host_library_allocations=False)
        candidate['requirements']['mask_isr'] = ['Kernel.MaskAndBounds']
        candidate['requirements']['kernel_integration'] = ['Kernel.RealScheduler']
        candidate['kernel'] = {'lock': 'dependencies/freertos.lock.json',
                               'required_symbols': ['actualStaticKernelApi']}
        self.assertEqual(probe.validate_contract(candidate), candidate)
        for mutation in ('no_kernel', 'no_mask', 'unsupported_mask', 'empty_api'):
            changed = copy.deepcopy(candidate)
            if mutation == 'no_kernel':
                changed['kernel'] = None
            elif mutation == 'no_mask':
                del changed['requirements']['mask_isr']
            elif mutation == 'unsupported_mask':
                changed['contract']['mask_policy'] = 'unreviewed_smp'
            else:
                changed['kernel']['required_symbols'] = []
            with self.assertRaises(ValueError):
                probe.validate_contract(changed)

    def test_non_kernel_backend_cannot_claim_kernel_task_lifecycle(self):
        candidate = reference()
        candidate['backend'].update(name='poll-port', kind='baremetal')
        candidate['contract'].update(irq_policy='configurable_irq',
                                     incoming_masks='preserved',
                                     host_library_allocations=False)
        candidate['requirements']['mask_isr'] = ['Poll.RestoresIncomingMask']
        candidate['requirements'].pop('task_join')
        candidate['requirements'].pop('queue_shutdown')
        candidate['contract'].update(task_reclaim='not_applicable',
                                     queue_shutdown='not_applicable')
        self.assertEqual(probe.validate_contract(candidate), candidate)
        candidate['contract']['task_reclaim'] = 'successful_join'
        with self.assertRaises(ValueError):
            probe.validate_contract(candidate)


class BackendExecutionAccountingTests(unittest.TestCase):
    def test_framework_discovery_uses_exact_suite_and_parameter_case_names(self):
        self.assertEqual(probe.discover_cases(
            'Running main() from gmock_main.cc\nNativeWait.\n  Never\n'
            'Prefix/Parameterized. # TypeParam = int\n'
            '  Contract/0 # GetParam() = 1\n'), {
                'NativeWait.Never', 'Prefix/Parameterized.Contract/0'})

    def test_discovery_rejects_empty_duplicate_and_disabled_cases(self):
        for output in ('', 'Empty.\n', 'A.\n  B\n  B\n',
                       'A.\n  DISABLED_B\n'):
            with self.assertRaises(ValueError):
                probe.discover_cases(output)

    def test_coverage_requires_actual_execution_of_every_declared_contract(self):
        contract = reference()
        cases = {case for group in contract['requirements'].values()
                 for case in group}
        probe.verify_coverage(contract, cases)
        cases.remove(next(iter(cases)))
        with self.assertRaises(ValueError):
            probe.verify_coverage(contract, cases)

    def test_generic_elf_cost_accepts_zero_data_without_guessing_resources(self):
        self.assertEqual(probe.elf_cost(
            'text data bss dec hex filename\n100 0 20 120 78 image.elf\n'), {
                'flash_bytes': 100, 'ram_bytes': 20,
                'text_bytes': 100, 'data_bytes': 0, 'bss_bytes': 20})

    def test_size_marker_empty_or_inconsistent_elf_cost_cannot_establish_proof(self):
        for output in ('{"status":"passed"}',
                       'text data bss dec hex filename\n0 0 0 0 0 image.elf\n',
                       'text data bss dec hex filename\n1 2 3 7 7 image.elf\n'):
            with self.assertRaises(ValueError):
                probe.elf_cost(output)


class BackendBuildBindingTests(unittest.TestCase):
    def test_existing_build_must_have_production_cmake_home(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / 'build/native'
            build.mkdir(parents=True)
            cache = build / 'CMakeCache.txt'
            cache.write_text('CMAKE_HOME_DIRECTORY:INTERNAL=' + str(root) + '\n')
            with patch.object(probe, 'ROOT', root):
                self.assertEqual(probe.actual_build(build), build)
                cache.write_text('CMAKE_HOME_DIRECTORY:INTERNAL=/other/root\n')
                with self.assertRaises(ValueError):
                    probe.actual_build(build)
                with self.assertRaises(ValueError):
                    probe.actual_build(root)

    def test_exact_registered_targets_cannot_be_replaced_by_other_binaries(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            manifest = build / 'google-contracts.json'
            target = build / 'bin/backend_test'
            manifest.write_text(json.dumps({'schema': 1,
                                            'executables': [str(target)]}))
            self.assertEqual(probe.planned_binaries(build, ['backend_test']),
                             [target])
            for names in ([], ['not_registered'], ['backend_test'] * 2,
                          ['backend_test;true']):
                with self.assertRaises(ValueError):
                    probe.planned_binaries(build, names)
            manifest.write_text(json.dumps({'schema': 1,
                                            'executables': ['/tmp/backend_test']}))
            with self.assertRaises(ValueError):
                probe.planned_binaries(build, ['backend_test'])

    def test_production_cpu_resolution_and_linked_float_enum_abi_must_agree(self):
        facts = probe.arch_compile.reference_facts('cortex-m0')
        profile = probe.cpu_contract.resolve(
            facts['cpu'], facts['irq'], 'baremetal').to_dict()
        contract = reference()
        contract['backend'].update(name='baremetal', kind='baremetal')
        contract['contract']['mask_policy'] = 'none'
        attributes = 'Tag_ABI_enum_size: small\n'
        self.assertEqual(probe.firmware_profile({'cpu_profile': profile},
                                               contract, attributes), profile)
        for mutation in ('hard', 'enum', 'basepri', 'invented_cpu'):
            value = copy.deepcopy(profile)
            policy = copy.deepcopy(contract)
            text = attributes
            if mutation == 'hard':
                text += 'Tag_ABI_VFP_args: VFP registers\n'
            elif mutation == 'enum':
                text = 'Tag_ABI_enum_size: int\n'
            elif mutation == 'basepri':
                policy['contract']['mask_policy'] = 'basepri'
            else:
                value['arch'] = 'invented-cortex'
            with self.assertRaises(ValueError):
                probe.firmware_profile({'cpu_profile': value}, policy, text)

    def test_kernel_not_registered_by_production_resolver_cannot_be_admitted(self):
        facts = probe.arch_compile.reference_facts('cortex-m4')
        profile = probe.cpu_contract.resolve(
            facts['cpu'], facts['irq'], 'baremetal').to_dict()
        contract = reference()
        contract['backend'].update(name='unreviewed-rtos', kind='kernel')
        profile['backend'] = 'unreviewed-rtos'
        with self.assertRaises(ValueError):
            probe.firmware_profile({'cpu_profile': profile}, contract,
                                   'Tag_ABI_enum_size: small\n')

    def test_actual_compile_commands_must_contain_every_resolved_abi_flag(self):
        profile = {'compile_options': ['-mcpu=cortex-m4', '-mthumb',
                                      '-mfloat-abi=soft', '-fshort-enums']}
        command = {'file': str(probe.ROOT / 'os/wait.c'),
                   'command': 'cc ' + ' '.join(profile['compile_options'])}
        probe.verify_compile_profile(profile, [command])
        with self.assertRaises(ValueError):
            probe.verify_compile_profile(profile, [])
        changed = copy.deepcopy(command)
        changed['command'] = changed['command'].replace('-fshort-enums', '')
        with self.assertRaises(ValueError):
            probe.verify_compile_profile(profile, [changed])


if __name__ == '__main__':
    unittest.main()
