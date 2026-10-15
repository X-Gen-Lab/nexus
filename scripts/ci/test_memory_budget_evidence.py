"""Keep canonical-budget tampering distinct from measured authored overruns."""

import importlib
from pathlib import Path
import tempfile
import tomllib
import unittest


ROOT = Path(__file__).resolve().parents[2]


class MemoryBudgetEvidenceTests(unittest.TestCase):
    def gate(self):
        return importlib.import_module('memory_budget')

    def test_family_checkers_capture_shared_budget_and_recorder_sources(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / 'scripts/ci').mkdir(parents=True)
            (root / 'CMakeLists.txt').write_text('# Source fixture.\n')
            for name in ('gd32_memory.py', 'memory_budget.py', 'arch_compile.py'):
                (root / 'scripts/ci' / name).write_text('# Recorded helper.\n')
            for module in ('gd32_memory', 'stm32_memory', 'os_threadx'):
                gate = importlib.import_module(module)
                before = gate.source_inputs(root)
                paths = {item['path'] for item in before}
                recorder = root / 'scripts/ci/arch_compile.py'
                self.assertIn(str(recorder), paths)
                if module != 'os_threadx':
                    shared = root / 'scripts/ci/memory_budget.py'
                    self.assertIn(str(shared), paths)
                    shared.write_text(shared.read_text() + '# Changed helper.\n')
                    self.assertNotEqual(before, gate.source_inputs(root))
                    before = gate.source_inputs(root)
                recorder.write_text(recorder.read_text() + '# Changed recorder.\n')
                self.assertNotEqual(before, gate.source_inputs(root))

    def test_budget_json_tamper_requires_the_precise_authority_rejection(self):
        report = {'schema_version': 1, 'kind': 'elf_resource_budget',
                  'status': 'failed',
                  'failure': 'resource budget differs from the resolved memory contract'}
        self.gate().check_budget_tamper(report)
        for changed in ({**report, 'status': 'passed'},
                        {**report, 'failure': 'ELF is missing'},
                        {**report, 'violations': []},
                        {**report, 'metrics': {}},
                        {**report, 'schema_version': True}):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                self.gate().check_budget_tamper(changed)

    def test_authored_budget_overflow_requires_actual_measured_region(self):
        for region in ('ccm', 'addsram', 'tcm'):
            report = {'schema_version': 1, 'kind': 'elf_resource_budget',
                      'status': 'failed',
                      'metrics': {'ram': {region: {'reserved_bytes': 36}}},
                      'violations': [{'metric': 'ram.' + region,
                                      'actual': 36, 'maximum': 32}]}
            self.gate().check_budget_overflow(report, region)
            for changed in ({**report, 'status': 'passed'},
                            {**report, 'violations': []},
                            {**report, 'violations': [{'metric': 'ram.' + region,
                                                      'actual': 36, 'maximum': 35}]},
                            {**report, 'failure': 'resource budget differs from the resolved memory contract'},
                            {**report, 'metrics': {'ram': {region: {'reserved_bytes': 0}}}},
                            {**report, 'schema_version': True}):
                with self.subTest(region=region, changed=changed), self.assertRaises(ValueError):
                    self.gate().check_budget_overflow(changed, region)

    def test_small_limit_is_authored_toml_and_preserves_other_regions(self):
        with tempfile.TemporaryDirectory() as name:
            output = Path(name)
            for family, preset, region in (('stm32', 'zg-ccm', 'ccm'),
                                          ('gd32', 'both', 'addsram'),
                                          ('gd32', 'both', 'tcm')):
                template = ROOT / 'tests/contracts' / (family + '_memory') / (preset + '.toml')
                before = template.read_bytes()
                selected = output / (region + '.toml')
                self.gate().write_budget_assembly(template, selected, region)
                old = tomllib.loads(before.decode())
                new = tomllib.loads(selected.read_text())
                self.assertEqual(new['memory']['regions'][region]['limit_bytes'], 32)
                new['memory']['regions'][region]['limit_bytes'] = 128
                self.assertEqual(old, new)
                self.assertEqual(template.read_bytes(), before)

    def test_authored_overflow_input_rejects_invalid_region_and_occupied_output(self):
        template = ROOT / 'tests/contracts/stm32_memory/zg-ccm.toml'
        with tempfile.TemporaryDirectory() as name:
            output = Path(name) / 'assembly.toml'
            with self.assertRaises(ValueError):
                self.gate().write_budget_assembly(template, output, 'unknown')
            self.assertFalse(output.exists())
            output.write_text('keep original input\n')
            with self.assertRaises(ValueError):
                self.gate().write_budget_assembly(template, output, 'ccm')
            self.assertEqual(output.read_text(), 'keep original input\n')


if __name__ == '__main__':
    unittest.main()
