"""PVD instances carry comparator policy without GPIO or hidden storage."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, bindings, configure


class PvdConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def assembly(self, part='ve', mode='pvd-level', fields='threshold_code=5\n'):
        board = self.root / (part + '-board')
        board.mkdir(exist_ok=True)
        facts = json.loads((configure.ROOT /
            f'tests/contracts/multi_controller_assembly/stm32-{part}-board/board.json').read_text())
        facts['bindings'] = [{'id': 'supply', 'controller': 'EXTI16',
                              'route': 'pvd-exti16'}]
        (board / 'board.json').write_text(json.dumps(facts))
        text = (f'schema=2\nboard_package="{board}"\nbackend="baremetal"\n'
                'clock="hse8-pll168"\n[memory]\nmain_stack_bytes=2048\n'
                f'[exti.supply]\nbinding="supply"\nmode="{mode}"\n' + fields)
        if mode == 'pvd-events':
            text += 'edge="both"\nevent_capacity=2\nirq.priority=6\n'
        path = self.root / (part + '.toml')
        path.write_text(text)
        return path

    def test_both_parts_and_modes_are_typed_static_and_pinless(self):
        for part in ('ve', 'zg'):
            for mode in ('pvd-level', 'pvd-events'):
                for threshold in (0, 7):
                    with self.subTest(part=part, mode=mode, threshold=threshold):
                        ir = configure.resolve_ir(self.assembly(
                            part, mode, f'threshold_code={threshold}\n'))
                        item = ir.controllers[0]
                        self.assertEqual(threshold, item.options.threshold_code)
                        self.assertFalse(item.pins)
                        with self.assertRaises(FrozenInstanceError):
                            item.options.threshold_code = 2
                        source = bindings.emit(ir)[1]
                        stem = 'nx_stm32_pvd' + ('_events' if mode == 'pvd-events' else '')
                        for operation in ('admit', 'prepare', 'stop'):
                            self.assertIn(stem + '_' + operation, source)
                        self.assertNotIn('s_nx_platform_syscfg_clock_added', source)
                        self.assertNotIn('.gpio = GPIO', source)
                        if mode == 'pvd-events':
                            self.assertIn('PVD_IRQHandler', source)
                            self.assertIn('.events', source)
                        else:
                            self.assertNotIn('s_nx_events_', source)
                            self.assertNotIn('PVD_IRQHandler', source)

    def test_threshold_is_required_and_no_pin_or_poll_irq_policy_is_invented(self):
        for fields in ('', 'threshold_code=8\n', 'threshold_code=-1\n',
                       'threshold_code=true\n', 'threshold_code=1.5\n',
                       'threshold_code=5\npull="up"\n',
                       'threshold_code=5\nirq.priority=6\n',
                       'threshold_code=5\nevent_capacity=2\n'):
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                configure.resolve_ir(self.assembly(fields=fields))

    def test_threshold_field_cannot_leak_into_a_gpio_exti_mode(self):
        path = self.assembly(mode='pvd-events')
        board = path.parent / 've-board' / 'board.json'
        facts = json.loads(board.read_text())
        facts['bindings'] = [{'id': 'supply', 'controller': 'EXTI0',
                              'route': 'exti0-pa0'}]
        board.write_text(json.dumps(facts))
        path.write_text(path.read_text().replace('pvd-events', 'edge-event'))
        with self.assertRaisesRegex(ValueError, 'threshold_code'):
            configure.resolve_ir(path)

    def test_editor_exposes_exact_encoding_without_a_default_voltage_policy(self):
        field = authored.editor_schema()['properties']['exti'][
            'additionalProperties']['properties']['threshold_code']
        self.assertEqual({'type': 'integer', 'minimum': 0, 'maximum': 7},
                         {key: field[key] for key in ('type', 'minimum', 'maximum')})
        self.assertNotIn('default', field)
        self.assertIn('PVD', field['description'])


if __name__ == '__main__':
    unittest.main()
