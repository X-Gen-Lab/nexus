"""Pure resolver phases preserve physical limits and input ownership."""
from dataclasses import FrozenInstanceError, replace
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure.ir import freeze
from tools.configure.providers import maintained


class ResolutionPhaseTests(unittest.TestCase):
    def setUp(self):
        self.provider = maintained('native')
        self.soc = gate.load(gate.ROOT / 'soc/native/soc.json')
        self.variant = self.soc['variants']['NATIVE_MODEL']
        self.assembly = gate.load_assembly(gate.ROOT /
            'tools/configure/assemblies/native-empty.toml')

    def memory(self, assembly=None):
        return gate._resolve_memory(
            self.assembly if assembly is None else assembly, self.variant,
            self.provider, 'NATIVE_MODEL', None)

    def test_memory_result_keeps_typed_budget_and_frozen_physical_leaves(self):
        selected = self.memory()
        self.assertEqual(selected.budget.main_stack_bytes,
                         self.assembly.memory_budgets['main_stack_bytes'])
        self.assertEqual(selected.default_ram['id'],
                         next(region['id'] for region in self.variant['memory']
                              if region['linker']))
        with self.assertRaises(FrozenInstanceError):
            selected.gpio_user = True
        with self.assertRaises(TypeError):
            selected.default_ram['size'] = 0

    def test_memory_phase_rejects_misalignment_and_physical_exhaustion(self):
        for size, error in ((2049, 'alignment'),
                            (self.variant['memory'][0]['size'], 'exhaust')):
            with self.subTest(size=size), self.assertRaisesRegex(ValueError,
                                                                error):
                self.memory(replace(self.assembly,
                    memory_budgets=freeze({'main_stack_bytes': size})))

    def test_absent_layout_retains_physical_geometry_without_input(self):
        selected = gate._resolve_flash_layout(
            None, gate.ROOT, self.variant['flash'], gate.load)
        self.assertIsNone(selected.input_path)
        self.assertEqual(selected.layout, freeze(self.variant['flash']))
        with self.assertRaises(TypeError):
            selected.layout['size'] = 0

    def test_layout_phase_keeps_exact_input_and_rejects_overlap(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            physical = self.variant['flash']
            image = {'id': 'image', 'origin': physical['origin'],
                     'size': physical['erase_blocks'][0], 'kind': 'image'}
            path = root / 'layout.json'
            path.write_text(json.dumps({'schema_version': 1,
                                        'regions': [image]}))
            selected = gate._resolve_flash_layout(
                'layout.json', root, physical, gate.load)
            self.assertEqual(selected.input_path, path.resolve())
            self.assertEqual(selected.layout['size'], image['size'])
            path.write_text(json.dumps({'schema_version': 1,
                'regions': [image, dict(image, id='data', kind='data')]}))
            with self.assertRaisesRegex(ValueError, 'overlap'):
                gate._resolve_flash_layout('layout.json', root, physical,
                                           gate.load)

    def test_native_dependency_phase_does_not_query_unselected_git_sources(self):
        def unexpected(*args):
            self.fail('Unselected dependency queried: ' + repr(args))

        selected = gate._resolve_source_dependencies(
            self.soc, self.provider, 'native', gate.ROOT, 'native', gate.load,
            unexpected)
        self.assertEqual(selected.dependencies, ())
        self.assertEqual(selected.inputs, ())

    def test_dependency_phase_passes_exact_git_pins_and_kernel_selection(self):
        soc = gate.load(gate.ROOT / 'soc/stm32/stm32f407/soc.json')
        provider = maintained('stm32f407')
        calls = []

        def resolve(relative, expected=None):
            calls.append((relative, expected))
            return {'path': relative, 'commit': expected or 'kernel-pin'}

        selected = gate._resolve_source_dependencies(
            soc, provider, 'stm32f407', gate.ROOT, 'freertos', gate.load,
            resolve)
        pins = soc['source_lock']
        self.assertEqual(calls[-1], ('ext/freertos', None))
        self.assertEqual({pin for _, pin in calls[:-1]}, set(pins.values()))
        self.assertEqual(len(selected.dependencies), len(pins) + 1)
        with self.assertRaises(TypeError):
            selected.dependencies[0]['commit'] = 'changed'


if __name__ == '__main__':
    unittest.main()
