"""WWDG selection is explicit, typed and retains failed cold preparation."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, configure, bindings
from tools.configure.stm32f4 import bindings as f4_bindings
from tools.configure.tests.native_fixture import native_fixture


class WindowWatchdogConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def assembly(self, part='ve', value=None):
        board = self.root / (part + '-board')
        board.mkdir(exist_ok=True)
        facts = json.loads((configure.ROOT /
            f'tests/contracts/multi_controller_assembly/stm32-{part}-board/board.json').read_text())
        known = {binding['id'] for binding in facts['bindings']}
        facts['bindings'] += [binding for binding in [
            {'id': 'wwdg', 'controller': 'WWDG', 'route': 'wwdg'},
            {'id': 'iwdg', 'controller': 'IWDG', 'route': 'iwdg'}]
            if binding['id'] not in known]
        (board / 'board.json').write_text(json.dumps(facts))
        text = (f'schema=2\nboard_package="{board}"\nbackend="baremetal"\n'
                'clock="hse8-pll168"\n[memory]\nmain_stack_bytes=2048\n'
                '[watchdog.window]\nbinding="wwdg"\nmode="window"\n')
        if value is not None:
            text += f'window_ticks={value}\n'
        path = self.root / (part + '.toml')
        path.write_text(text)
        return path

    def resolve(self, path):
        return configure.resolve_ir(path, test_support=native_fixture(path))

    def test_both_parts_use_typed_windows_and_c_lifecycle_with_retained_failure(self):
        for part in ('ve', 'zg'):
            for ticks in (None, 8, 62):
                with self.subTest(part=part, ticks=ticks):
                    ir = self.resolve(self.assembly(part, ticks))
                    item = ir.controllers[0]
                    self.assertEqual(item.options.window_ticks, ticks or 1)
                    with self.assertRaises(FrozenInstanceError):
                        item.options.window_ticks = 2
                    construction = f4_bindings._basic_constructor(item, ir)
                    self.assertTrue(construction.retain_on_failed_init)
                    self.assertTrue(construction.admission)
                    source = bindings.emit(ir)[1]
                    self.assertIn('nx_stm32_window_watchdog_prepare', source)
                    self.assertIn('nx_stm32_window_watchdog_admit', source)
                    self.assertIn('nx_stm32_window_watchdog_stop', source)
                    self.assertIn('&nx_stm32_window_watchdog_ops', source)

    def test_window_counts_reject_invalid_types_and_empty_feed_windows(self):
        for value in ('true', '0', '63', '-1', '1.5'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.resolve(self.assembly(value=value))

    def test_independent_watchdog_rejects_window_fields(self):
        path = self.assembly(value=8)
        path.write_text(path.read_text().replace('binding="wwdg"', 'binding="iwdg"')
                        .replace('mode="window"', 'mode="independent"'))
        with self.assertRaisesRegex(ValueError, 'window_ticks'):
            self.resolve(path)

    def test_editor_contract_uses_exact_integer_tick_bounds(self):
        field = authored.editor_schema()['properties']['watchdog'][
            'additionalProperties']['properties']['window_ticks']
        self.assertEqual({key: field[key] for key in ('type', 'minimum', 'maximum', 'default')},
                         {'type': 'integer', 'minimum': 1, 'maximum': 62, 'default': 1})
        self.assertIn('WWDG', field['description'])


if __name__ == '__main__':
    unittest.main()
