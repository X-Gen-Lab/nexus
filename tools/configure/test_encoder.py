"""F407 encoder personalities share exact controllers and static wiring."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate, dependencies


class EncoderConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.board = gate.load(gate.ROOT /
            'tests/contracts/multi_controller_assembly/stm32-ve-board/board.json')
        self.package = self.directory / 'board'
        self.package.mkdir()

    def assembly(self, timer='TIM3', counting='x4-ti12'):
        pins = {'TIM3': ('pa6', 'pa7'), 'TIM4': ('pb6', 'pb7'),
                'TIM5': ('pa0', 'pa1')}[timer]
        self.board['bindings'] = [{'id': 'position', 'controller': timer,
            'route': f'{timer.lower()}-encoder-{pins[0]}-{pins[1]}'}]
        (self.package / 'board.json').write_text(json.dumps(self.board))
        path = self.directory / 'encoder.toml'
        path.write_text('schema=2\nboard_package="board"\nbackend="baremetal"\n'
                        'clock="hse8-pll168"\n[memory]\nmain_stack_bytes=2048\n'
                        '[encoder.position]\nbinding="position"\nmode="quadrature"\n'
                        'counting=' + json.dumps(counting) + '\nfilter_a=0\n'
                        'filter_b=15\ninvert_a=false\ninvert_b=true\nexclusive=true\n')
        return path

    def test_three_timers_and_counting_modes_generate_static_c(self):
        for timer in ('TIM3', 'TIM4', 'TIM5'):
            for counting, symbol in (('x2-ti1', 'X2_TI1'), ('x2-ti2', 'X2_TI2'),
                                     ('x4-ti12', 'X4_TI12')):
                with self.subTest(timer=timer, counting=counting):
                    path = self.assembly(timer, counting)
                    result = gate.resolve_ir(path)
                    self.assertEqual(timer, result.controllers[0].controller)
                    self.assertEqual(counting, result.controllers[0].options.counting)
                    self.assertFalse(any(c.owner == 'position' and
                                         c.key.startswith(('dma:', 'irq:'))
                                         for c in result.claims))
                    output = self.directory / (timer + '-' + counting)
                    gate.configure(path, output)
                    source = (output / 'bindings.c').read_text()
                    self.assertIn('.registers = ' + timer, source)
                    self.assertIn('NX_ENCODER_MODE_' + symbol, source)
                    self.assertIn('nx_stm32_encoder_prepare(', source)
                    self.assertIn('nx_stm32_encoder_close(', source)

    def test_unknown_mode_filter_and_nonexclusive_inputs_reject(self):
        for old, new in (('"x4-ti12"', '"signed-position"'),
                         ('filter_a=0', 'filter_a=16'),
                         ('filter_b=15', 'filter_b=true'),
                         ('invert_a=false', 'invert_a=0'),
                         ('exclusive=true', 'exclusive=false')):
            with self.subTest(new=new):
                path = self.assembly()
                path.write_text(path.read_text().replace(old, new))
                with self.assertRaisesRegex(ValueError, '[Ee]ncoder'):
                    gate.resolve_ir(path)

    def test_pwm_cannot_share_the_encoder_controller_on_different_pins(self):
        path = self.assembly()
        self.board['bindings'].append({'id': 'other', 'controller': 'TIM3',
                                      'route': 'tim3-ch3-pb0', 'initial': 0})
        (self.package / 'board.json').write_text(json.dumps(self.board))
        path.write_text(path.read_text() + '[pwm.other]\nbinding="other"\n'
                        'mode="fixed-pwm"\nperiod_ticks=1000\nduty_ticks=250\n'
                        'tick_hz=1000000\n')
        with self.assertRaisesRegex(ValueError, 'TIM3'):
            gate.resolve_ir(path)

    def test_helper_changes_invalidate_frozen_generation_and_identity(self):
        helper = 'tools/configure/stm32f4/encoder.py'
        root = self.directory / 'generator-source'
        for name in set(dependencies.source_names('platform', 'stm32f4')) | {helper}:
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((gate.ROOT / name).read_bytes())
        with patch.object(dependencies, 'ROOT', root):
            paths, before = dependencies.snapshot(mode='platform', family='stm32f4')
            target = root / helper
            target.write_bytes(target.read_bytes() + b'\n# Edited validation.\n')
            with self.assertRaisesRegex(ValueError, 'encoder.py'):
                dependencies.verify(paths, before)
            _, after = dependencies.snapshot(mode='platform', family='stm32f4')
        self.assertNotEqual(before['sha256'], after['sha256'])


if __name__ == '__main__':
    unittest.main()
