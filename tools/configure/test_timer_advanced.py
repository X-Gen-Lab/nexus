"""Selected timer contracts retain clocks, vectors and explicit safe levels."""
from tools.configure.tests.native_fixture import native_fixture
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


class TimerAssemblyConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def channel_case(self, kind, controller, channel):
        if kind == 'timer_capture':
            pins = {'TIM3': ('PA6', 'PA7', 'PB0', 'PB1'),
                    'TIM4': ('PB6', 'PB7', 'PB8', 'PB9'),
                    'TIM5': ('PA0', 'PA1', 'PA2', 'PA3'),
                    'TIM1': ('PA8', 'PA9', 'PA10', 'PA11'),
                    'TIM8': ('PC6', 'PC7', 'PC8', 'PC9'),
                    'TIM9': ('PA2', 'PA3'), 'TIM10': ('PB8',),
                    'TIM11': ('PB9',), 'TIM12': ('PB14', 'PB15'),
                    'TIM13': ('PA6',), 'TIM14': ('PA7',)}[controller]
            route = ('capture-pb6' if controller == 'TIM4' and channel == 1
                     else f'capture-{controller.lower()}-ch{channel}-{pins[channel-1].lower()}')
            options = 'edge="both"\nfilter=3\n'
            mode = 'edge-capture'
        else:
            pads = {'TIM1': (('PA8', 'PB13', 'PB12'),
                             ('PA9', 'PB14', 'PB12'),
                             ('PA10', 'PB15', 'PB12')),
                    'TIM8': (('PC6', 'PA7', 'PA6'),
                             ('PC7', 'PB0', 'PA6'),
                             ('PC8', 'PB1', 'PA6'))}[controller][channel-1]
            route = ({'TIM1': 'complementary-pa8-pb12-pb13',
                      'TIM8': 'complementary-pc6-pa7-pa6'}[controller]
                     if channel == 1 else
                     f'complementary-{controller.lower()}-ch{channel}-'+
                     '-'.join(p.lower() for p in pads))
            options = ('period_ticks=1000\ndeadtime_ticks=128\n'
                       'break_active_high=true\nidle_high=false\n'
                       'idle_complement_high=false\n')
            mode = 'complementary-pwm'
        base = gate.ROOT/'tests/contracts/multi_controller_assembly'
        board = json.loads((base/'stm32-ve-board/board.json').read_text())
        board['bindings'] = [{'id': 'selected', 'controller': controller,
                              'route': route}]
        package = self.directory/'channel-board'
        package.mkdir(exist_ok=True)
        (package/'board.json').write_text(json.dumps(board))
        path = self.directory/'channel.toml'
        path.write_text('schema=2\nboard_package="channel-board"\n'
                        'backend="baremetal"\nclock="hse8-pll168"\n'
                        'optimization="Os"\n[factory]\ncapabilities=true\n'
                        '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                        'static_ram_limit_bytes=16384\nflash_load_limit_bytes=65536\n'
                        f'[{kind}.signal]\nbinding="selected"\nmode="{mode}"\n'
                        'tick_hz=1000000\nexclusive=true\n'+options+
                        f'[{kind}.signal.irq]\npriority=6\ncalls_os=false\n')
        return path

    def test_advanced_capture_selects_cc_vector_without_break_peer_claim(self):
        from tools.configure import bindings
        for timer, vector, peer in (('TIM1', 'TIM1_CC', 'TIM9'),
                                     ('TIM8', 'TIM8_CC', 'TIM12')):
            for channel in range(1, 5):
                with self.subTest(timer=timer, channel=channel):
                    path = self.channel_case('timer_capture', timer, channel)
                    result = gate.resolve_ir(path)
                    source = bindings.emit(result)[1]
                    self.assertIn(f'void {vector}_IRQHandler', source)
                    self.assertIn(f'&nx_stm32_{timer.lower()}_timer_capture_ops', source)
                    keys = {claim.key for claim in result.claims}
                    self.assertIn('irq:' + vector, keys)
                    self.assertNotIn('controller:' + peer, keys)
                    self.assertNotIn('irq:' + timer + '_BRK_' + peer, keys)

    def test_advanced_capture_and_encoder_cannot_share_a_timer_on_other_pads(self):
        for timer, pins in (('TIM1', 'pa8-pa9'), ('TIM8', 'pc6-pc7')):
            with self.subTest(timer=timer):
                path = self.channel_case('timer_capture', timer, 3)
                board_path = self.directory / 'channel-board/board.json'
                board = json.loads(board_path.read_text())
                board['bindings'].append({'id': 'position', 'controller': timer,
                                          'route': timer.lower() + '-encoder-' + pins})
                board_path.write_text(json.dumps(board))
                path.write_text(path.read_text() +
                    '[encoder.position]\nbinding="position"\nmode="quadrature"\n'
                    'counting="x4-ti12"\nfilter_a=0\nfilter_b=0\n'
                    'invert_a=false\ninvert_b=false\nexclusive=true\n')
                with self.assertRaisesRegex(ValueError, 'controller:' + timer):
                    gate.resolve_ir(path)

    def test_small_capture_selects_exact_channel_and_shared_source_vector(self):
        from tools.configure import bindings
        for timer, count, vector in (
                ('TIM9', 2, 'TIM1_BRK_TIM9'), ('TIM10', 1, 'TIM1_UP_TIM10'),
                ('TIM11', 1, 'TIM1_TRG_COM_TIM11'), ('TIM12', 2, 'TIM8_BRK_TIM12'),
                ('TIM13', 1, 'TIM8_UP_TIM13'), ('TIM14', 1, 'TIM8_TRG_COM_TIM14')):
            for channel in range(1, count + 1):
                with self.subTest(timer=timer, channel=channel):
                    result = gate.resolve_ir(self.channel_case('timer_capture', timer, channel))
                    source = bindings.emit(result)[1]
                    self.assertIn(f'void {vector}_IRQHandler', source)
                    self.assertIn(f'.channel = {channel}u', source)
                    keys = {claim.key for claim in result.claims}
                    self.assertIn('irq:' + vector, keys)
                    self.assertNotIn('controller:TIM1', keys)
                    self.assertNotIn('controller:TIM8', keys)

    def test_small_capture_coexists_with_peer_cc_but_rejects_same_vector_owner(self):
        for timer, peer, pin, shared, cc, complementary in (
                ('TIM9', 'TIM1', 'pa8', 'TIM1_BRK_TIM9', 'TIM1_CC',
                 'complementary-pa8-pb12-pb13'),
                ('TIM12', 'TIM8', 'pc6', 'TIM8_BRK_TIM12', 'TIM8_CC',
                 'complementary-pc6-pa7-pa6')):
            with self.subTest(timer=timer):
                path = self.channel_case('timer_capture', timer, 1)
                original = path.read_text()
                board_path = self.directory / 'channel-board/board.json'
                board = json.loads(board_path.read_text())
                board['bindings'].append({'id': 'peer', 'controller': peer,
                    'route': 'capture-' + peer.lower() + '-ch1-' + pin})
                board_path.write_text(json.dumps(board))
                path.write_text(original + '[timer_capture.peer]\nbinding="peer"\n'
                    'mode="edge-capture"\ntick_hz=1000000\nedge="rising"\n'
                    'filter=0\nexclusive=true\n[timer_capture.peer.irq]\n'
                    'priority=6\ncalls_os=false\n')
                result = gate.resolve_ir(path)
                keys = {claim.key for claim in result.claims}
                self.assertIn('irq:' + cc, keys)
                self.assertIn('irq:' + shared, keys)
                board['bindings'][-1]['route'] = complementary
                board_path.write_text(json.dumps(board))
                path.write_text(original + '[advanced_pwm.peer]\nbinding="peer"\n'
                    'mode="complementary-pwm"\ntick_hz=1000000\nperiod_ticks=1000\n'
                    'deadtime_ticks=128\nbreak_active_high=true\nidle_high=false\n'
                    'idle_complement_high=false\nexclusive=true\n[advanced_pwm.peer.irq]\n'
                    'priority=6\ncalls_os=false\n')
                with self.assertRaisesRegex(ValueError, 'controller:' + timer + '|irq:' + shared):
                    gate.resolve_ir(path)

    def test_capture_routes_derive_twelve_channels_and_exact_static_transports(self):
        from tools.configure import bindings
        for controller in ('TIM3', 'TIM4', 'TIM5'):
            for channel in range(1, 5):
                with self.subTest(controller=controller, channel=channel):
                    path = self.channel_case('timer_capture', controller, channel)
                    try:
                        result = gate.resolve_ir(path)
                    except ValueError as error:
                        self.fail(str(error))
                    source = bindings.emit(result)[1]
                    self.assertIn(f'.channel = {channel}u', source)
                    self.assertIn(f'.registers = {controller}', source)
                    stem = ('nx_stm32_timer_capture' if controller == 'TIM4'
                            else f'nx_stm32_{controller.lower()}_timer_capture')
                    self.assertIn('&'+stem+'_ops', source)
                    self.assertIn(f'{controller}_IRQHandler', source)
                    self.assertFalse(result.physical_qualified)
                    self.assertEqual(len(result.controllers), 1)

    def test_advanced_routes_derive_all_six_pairs_and_preserve_break_peer(self):
        from tools.configure import bindings
        for controller, peer in (('TIM1', 'TIM9'), ('TIM8', 'TIM12')):
            for channel in range(1, 4):
                with self.subTest(controller=controller, channel=channel):
                    path = self.channel_case('advanced_pwm', controller, channel)
                    try:
                        result = gate.resolve_ir(path)
                    except ValueError as error:
                        self.fail(str(error))
                    source = bindings.emit(result)[1]
                    self.assertIn(f'.channel = {channel}u', source)
                    self.assertIn('controller:'+peer,
                                  {item.key for item in result.claims})
                    self.assertFalse(result.physical_qualified)

    def test_new_capture_personalities_exclude_their_own_pwm_controller(self):
        for controller, pin in (('TIM3', 'pa7'), ('TIM5', 'pa1')):
            with self.subTest(controller=controller):
                path = self.channel_case('timer_capture', controller, 1)
                gate.resolve_ir(path)
                board_path = self.directory/'channel-board/board.json'
                board = json.loads(board_path.read_text())
                board['bindings'].append({'id': 'alias', 'controller': controller,
                                          'route': controller.lower()+'-ch2-'+pin,
                                          'initial': 0})
                board_path.write_text(json.dumps(board))
                path.write_text(path.read_text()+'\n[pwm.alias]\nbinding="alias"\n'
                                'mode="fixed-pwm"\ntick_hz=1000000\n'
                                'period_ticks=1000\nduty_ticks=0\n')
                with self.assertRaisesRegex(ValueError, 'controller:'+controller):
                    gate.resolve_ir(path)

    def dual_advanced(self):
        base = gate.ROOT / 'tests/contracts/timer_assembly'
        board = json.loads((base / 'stm32-zg-board/board.json').read_text())
        board['bindings'].append({'id': 'complementary8', 'controller': 'TIM8',
                                 'route': 'complementary-pc6-pa7-pa6'})
        package = self.directory / 'dual-board'
        package.mkdir(exist_ok=True)
        (package / 'board.json').write_text(json.dumps(board))
        text = (base / 'stm32-zg.toml').read_text().replace(
            'board_package="stm32-zg-board"', 'board_package="dual-board"')
        second = text[text.index('[advanced_pwm.power]'):].replace(
            'advanced_pwm.power', 'advanced_pwm.power8').replace(
            'binding="complementary"', 'binding="complementary8"')
        path = self.directory / 'dual.toml'
        path.write_text(text + '\n' + second)
        return path

    def test_two_advanced_timers_have_exact_distinct_pads_vectors_and_factories(self):
        ir = gate.resolve_ir(self.dual_advanced())
        self.assertEqual({'TIM1', 'TIM8'}, {item.controller for item in ir.controllers
                                          if item.kind == 'advanced_pwm'})
        claims = {item.key for item in ir.claims}
        self.assertTrue({'irq:TIM1_BRK_TIM9', 'irq:TIM8_BRK_TIM12',
                         'controller:TIM9', 'controller:TIM12'} <= claims)
        from tools.configure import bindings
        source = bindings.emit(ir)[1]
        self.assertIn('&nx_stm32_tim8_advanced_pwm_ops', source)
        self.assertIn('nx_stm32_tim8_advanced_cold_prepare', source)
        self.assertIn('TIM8_BRK_TIM12_IRQHandler', source)
        self.assertIn('.break_input = &GPIOA->IDR, .break_mask = 64u', source)

    def test_tim8_reserves_its_actual_tim12_shared_break_vector_peer(self):
        path = self.dual_advanced()
        package = path.parent / 'dual-board/board.json'
        board = json.loads(package.read_text())
        board['bindings'].append({'id': 'pwm12', 'controller': 'TIM12',
                                  'route': 'tim12-ch1-pb14', 'initial': 0})
        package.write_text(json.dumps(board))
        path.write_text(path.read_text() + '\n[pwm.peer]\nbinding="pwm12"\n'
                        'mode="fixed-pwm"\nperiod_ticks=1000\nduty_ticks=0\n'
                        'tick_hz=1000000\n')
        with self.assertRaisesRegex(ValueError, 'TIM12'):
            gate.resolve_ir(path)

    def assembly(self, label="stm32-zg", change=None):
        base = gate.ROOT / "tests/contracts/timer_assembly"
        text = (base / (label + ".toml")).read_text().replace(
            f'board_package="{label}-board"',
            'board_package=' + json.dumps(str(base / (label + "-board"))))
        path = self.directory / "assembly.toml"
        path.write_text(change(text) if change else text)
        return path

    def test_two_exact_parts_bind_two_exclusive_controllers_and_shared_break_vector(self):
        for label in ('stm32-zg', 'stm32-ve'):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label), test_support=native_fixture(self.assembly(label)))
                self.assertFalse(result.physical_qualified)
                self.assertEqual(len(result.controllers), 2)
                capture = next(item for item in result.controllers if item.kind == "timer_capture")
                self.assertEqual(capture.options.edge, "both")
                self.assertEqual(capture.options.filter, 3)
                with self.assertRaises(FrozenInstanceError):
                    capture.options.tick_hz = 2
                claims = {item.key for item in result.claims}
                self.assertIn("pin:PB6", claims)
                self.assertIn("pin:PB12", claims)
                self.assertIn("pin:PB13", claims)
                self.assertIn("irq:TIM1_BRK_TIM9", claims)
                self.assertFalse(any(key.startswith("dma:") for key in claims))

    def test_public_factory_types_and_caller_owned_capture_storage(self):
        output = self.directory / "generated"
        gate.configure(self.assembly(), output, test_support=native_fixture(self.assembly()))
        header = (output / "nexus_factory.h").read_text()
        source = (output / "bindings.c").read_text()
        self.assertIn("const nx_timer_capture_t* nx_factory_timer_capture(", header)
        self.assertIn("const nx_advanced_pwm_t* nx_factory_advanced_pwm(", header)
        self.assertIn("static nx_timer_capture_controller_t s_nx_port_encoder;", source)
        self.assertIn("static nx_advanced_pwm_controller_t s_nx_port_power;", source)
        self.assertIn("nx_stm32_capture_cold_close(&s_nx_port_encoder,", source)
        self.assertIn("nx_stm32_advanced_cold_close(&s_nx_port_power,", source)
        self.assertIn("static const nx_stm32_capture_cold_config_t", source)
        self.assertIn("static const nx_stm32_advanced_cold_config_t", source)
        self.assertNotIn("static nx_result_t prepare_encoder", source)
        self.assertNotIn("static nx_result_t stop_power", source)
        self.assertNotIn("static nx_timer_capture_event_t", source)
        self.assertNotIn("malloc", source)

    def test_selected_timer_owner_has_a_privileged_context_connection(self):
        for label in ('stm32-zg',):
            with self.subTest(label=label):
                output = self.directory / label
                gate.configure(self.assembly(label), output, test_support=native_fixture(self.assembly(label)))
                source = (output / "bindings.c").read_text()
                context = source.split(
                    "nx_platform_context_check(void) {", 1)[1].split("}", 1)[0]
                for check in ("!nx_arch_is_privileged()", "nx_arch_in_isr()",
                              "nx_arch_irq_is_masked()"):
                    self.assertIn(check, context)
                self.assertIn(".context_check = nx_platform_context_check,", source)
                self.assertIn(".release = nx_platform_release,", source)
                self.assertIn("nx_stm32_advanced_cold_close(", source)
                self.assertIn("nx_stm32f4_platform_system_start_local(", source)

    def test_non_exact_divider_filter_edge_and_period_reject(self):
        for old, new in (("tick_hz=1000000", "tick_hz=999999"),
                         ('edge="both"', 'edge="invalid"'),
                         ('edge="both"', 'edge=["both"]'),
                         ('edge="both"', 'edge={kind="both"}'),
                         ("filter=3", "filter=16"),
                         ("filter=3", "filter=true"),
                         ("period_ticks=1000", "period_ticks=65536"),
                         ("period_ticks=1000", "period_ticks=0")):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, "Timer"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(old, new)), test_support=native_fixture(self.assembly(change=lambda text: text.replace(old, new))))

    def test_deadtime_accepts_exact_encoding_boundaries_and_rejects_rounding(self):
        for ticks in (0, 127, 128, 254, 256, 504, 512, 1008):
            with self.subTest(ticks=ticks):
                result = gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}")), test_support=native_fixture(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}"))))
                self.assertEqual(next(item for item in result.controllers if item.kind == "advanced_pwm").options.deadtime_ticks, ticks)
        for ticks in (129, 255, 257, 505, 511, 1009, -1):
            with self.subTest(ticks=ticks), self.assertRaisesRegex(ValueError, "Timer deadtime"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}")), test_support=native_fixture(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}"))))

    def test_exclusive_and_safe_polarities_are_explicit_boolean_policy(self):
        for old, new in (("exclusive=true", "exclusive=false"),
                         ("exclusive=true", "exclusive=1"),
                         ("break_active_high=true", "break_active_high=1"),
                         ("idle_high=false\n", ""),
                         ("idle_complement_high=false", "idle_complement_high=0")):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, "Timer"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(old, new)), test_support=native_fixture(self.assembly(change=lambda text: text.replace(old, new))))

    def test_dma_or_generated_ring_options_do_not_enter_timer_contract(self):
        for added in ("event_capacity=16", "dma=true", "safe_level=true"):
            with self.subTest(added=added), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    'edge="both"', 'edge="both"\n' + added)), test_support=native_fixture(self.assembly(change=lambda text: text.replace(
                    'edge="both"', 'edge="both"\n' + added))))

    def test_default_assembly_has_no_optional_timer_state_or_methods(self):
        output = self.directory / "generated"
        gate.configure(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml", output, test_support=native_fixture(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml"))
        source = (output / "bindings.c").read_text()
        self.assertNotIn("nx_timer_capture_controller_t", source)
        self.assertNotIn("nx_advanced_pwm_controller_t", source)
        self.assertNotIn("nx_stm32_timer_capture_ops", source)
        self.assertNotIn("nx_stm32_advanced_pwm_ops", source)
        self.assertIn("nx_arch_irq_is_masked() || !nx_arch_is_privileged()", source)


if __name__ == "__main__":
    unittest.main()
