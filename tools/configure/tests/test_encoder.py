"""F407 encoder personalities share exact controllers and static wiring."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure.tests.f4_fixture import provider


class EncoderConfigurationTests(unittest.TestCase):
    def test_reviewed_alternate_ti_pair_is_not_a_constructor_pin_table(self):
        selected = provider("STM32F407VET6")
        selection = dict(mode="quadrature", counting="x4", filter_a=0, filter_b=0,
                         invert_a=False, invert_b=False, exclusive=True)
        route = {"kind": "encoder", "controller": "TIM4", "pins": [
            {"pin": "PD12", "function": "ti1", "af": 2},
            {"pin": "PD13", "function": "ti2", "af": 2}]}
        selected.validate_selection(selection, route)
        route["pins"][1]["function"] = "ti1"
        with self.assertRaises(ValueError):
            selected.validate_selection(selection, route)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.board = gate.load(gate.ROOT /
            'tests/contracts/multi_controller_assembly/stm32-ve-board/board.json')
        self.package = self.directory / 'board'
        self.package.mkdir()

    def assembly(self, timer='TIM3', counting='x4'):
        pins = {'TIM3': ('pa6', 'pa7'), 'TIM4': ('pb6', 'pb7'),
                'TIM5': ('pa0', 'pa1'), 'TIM1': ('pa8', 'pa9'),
                'TIM8': ('pc6', 'pc7')}[timer]
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

    def test_advanced_timers_have_native_encoder_without_irq_or_dma_claim(self):
        for timer in ('TIM1', 'TIM8'):
            with self.subTest(timer=timer):
                result = gate.resolve_ir(self.assembly(timer))
                self.assertEqual(timer, result.controllers[0].controller)
                self.assertFalse(any(claim.owner == 'position' and
                                     claim.key.startswith(('irq:', 'dma:'))
                                     for claim in result.claims))

    def test_three_timers_and_counting_modes_generate_static_c(self):
        for timer in ('TIM3', 'TIM4', 'TIM5'):
            for counting, symbol in (('x2-a', 'X2_A'), ('x2-b', 'X2_B'),
                                     ('x4', 'X4')):
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
        for old, new in (('"x4"', '"signed-position"'),
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



if __name__ == '__main__':
    unittest.main()
