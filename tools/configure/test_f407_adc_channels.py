"""F407 raw ranks follow reviewed external pads and internal analog paths."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import adc, bindings, configure as gate
from tools.configure.stm32f4 import contract


class F407AdcChannelTests(unittest.TestCase):
    PINS = tuple([f"PA{i}" for i in range(8)] + ["PB0", "PB1"] +
                 [f"PC{i}" for i in range(6)])
    ADC3_PINS = ("PA0", "PA1", "PA2", "PA3", "PF6", "PF7", "PF8", "PF9",
                 "PF10", "PF3", "PC0", "PC1", "PC2", "PC3", "PF4", "PF5")

    def selection(self, pins, channels, samples=None, mode="low-rate-scan",
                  controller="ADC1"):
        route = {"controller": controller, "kind": "adc",
                 "pins": [{"pin": pin} for pin in pins]}
        selected = {"mode": mode, "channels": list(channels),
                    "sample_times": list(samples or [7] * len(channels)),
                    "reference_mv": 3300}
        return selected, route

    def test_internal_adc1_sources_resolve_without_gpio_claims(self):
        selected, route = self.selection([], [16, 17, 18])
        adc.validate_selection(selected, route, contract)
        contract.validate_selection(selected, route)
        self.assertEqual([], adc.selected_route(selected, route, contract)['pins'])

    def test_injected_groups_reject_more_than_four_ranks(self):
        selected, route = self.selection(self.PINS[:5], list(range(5)),
                                         mode='injected-shot')
        adc.validate_selection(selected, route, contract)
        with self.assertRaisesRegex(ValueError, 'four ranks'):
            contract.validate_selection(selected, route)

    def test_internal_sources_reject_other_converters_and_short_acquisition(self):
        for controller, sample in (("ADC1", 6), ("ADC2", 7), ("ADC3", 7)):
            with self.subTest(controller=controller, sample=sample):
                selected, route = self.selection(
                    [], [16, 17, 18], [sample] * 3, controller=controller)
                with self.assertRaises(ValueError):
                    adc.validate_selection(selected, route, contract)
                    contract.validate_selection(selected, route)

    def test_internal_sources_have_a_complete_pinless_static_assembly(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(gate.ROOT / 'tests/contracts/multi_controller_assembly/stm32-ve-board/board.json')
            board['bindings'] = [{'id': 'internal', 'controller': 'ADC1',
                                   'route': 'adc1-internal'}]
            (directory / 'board').mkdir()
            (directory / 'board/board.json').write_text(json.dumps(board))
            for mode in ('single-shot', 'low-rate-scan', 'trigger-dma',
                         'trigger-dma-continuous', 'injected-shot'):
                with self.subTest(mode=mode):
                    channels = [17] if mode == 'single-shot' else [16, 17, 18]
                    text = ('schema=2\nboard_package="board"\nbackend="baremetal"\n'
                            'clock="hse8-pll168"\n[memory]\nmain_stack_bytes=2048\n'
                            '[adc.internal]\nbinding="internal"\nmode=' + json.dumps(mode) +
                            '\nchannels=' + json.dumps(channels) + '\nsample_times=' +
                            json.dumps([7] * len(channels)) + '\nreference_mv=3300\n')
                    if mode.startswith('trigger-'):
                        text += '[adc.internal.irq]\npriority=5\ncalls_os=false\n'
                    path = directory / 'assembly.toml'
                    path.write_text(text)
                    result = gate.resolve_ir(path)
                    self.assertEqual(result.controllers[0].pins, ())
                    self.assertFalse(any(c.key.startswith('pin:') for c in result.claims))
                    output = directory / mode
                    gate.configure(path, output)
                    self.assertIn('.registers = ADC1', (output / 'bindings.c').read_text())

    def test_all_external_pad_mappings_accept_a_single_shot(self):
        for channel, pin in enumerate(self.PINS):
            with self.subTest(channel=channel, pin=pin):
                selected, route = self.selection([pin], [channel],
                                                  mode="single-shot")
                adc.validate_selection(selected, route, contract)
                contract.validate_selection(selected, route)

    def test_adc2_and_adc3_use_their_own_reviewed_channel_maps(self):
        for controller, pins in (("ADC2", self.PINS), ("ADC3", self.ADC3_PINS)):
            for channel, pin in enumerate(pins):
                with self.subTest(controller=controller, channel=channel):
                    selected, route = self.selection(
                        [pin], [channel], mode="single-shot",
                        controller=controller)
                    adc.validate_selection(selected, route, contract)
                    contract.validate_selection(selected, route)

    def test_adc3_never_reuses_the_adc12_upper_channel_map(self):
        for channel in (4, 8, 9, 14, 15):
            with self.subTest(channel=channel):
                selected, route = self.selection(
                    [self.PINS[channel]], [channel], mode="single-shot",
                    controller="ADC3")
                with self.assertRaises(ValueError):
                    adc.validate_selection(selected, route, contract)

    def test_scan_order_and_consistent_repeated_ranks_remain_explicit(self):
        for pins, channels in ((self.PINS, list(reversed(range(16)))),
                               (["PC5"], [15, 15, 15])):
            with self.subTest(channels=channels):
                selected, route = self.selection(pins, channels)
                adc.validate_selection(selected, route, contract)
                contract.validate_selection(selected, route)

    def test_conflicting_sample_times_are_not_a_representable_sequence(self):
        selected, route = self.selection(["PA0"], [0, 0], [1, 7])
        with self.assertRaisesRegex(ValueError, "sample time"):
            adc.validate_selection(selected, route, contract)

    def test_an_unconnected_channel_or_wrong_bank_is_rejected(self):
        for pins, channels in ((["PC0"], [0]), (["PB0"], [0]),
                               (["PA0"], [19]), (["PC6"], [6])):
            with self.subTest(pins=pins, channels=channels):
                selected, route = self.selection(pins, channels,
                                                  mode="single-shot")
                with self.assertRaises(ValueError):
                    adc.validate_selection(selected, route, contract)

    def test_malformed_ranks_report_configuration_errors(self):
        for channels in ([[0]], [True], ["0"], [None]):
            with self.subTest(channels=channels):
                selected, route = self.selection(["PA0"], channels,
                                                  mode="single-shot")
                with self.assertRaises(ValueError):
                    adc.validate_selection(selected, route, contract)

    def test_every_external_input_generates_static_c_configuration(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(gate.ROOT /
                              "boards/stm32f407_qiming_v31/board.json")
            board["id"] = "adc-external-input-software-fixture"
            board["provenance"] = []
            board["unknowns"] = ["Software fixture; no PCB wiring or HIL claim"]
            for channel in range(16):
                with self.subTest(channel=channel):
                    board["bindings"] = [{"id": "input", "controller": "ADC1",
                                          "route": f"adc1-ch{channel}-{self.PINS[channel].lower()}"}]
                    (directory / "board.json").write_text(json.dumps(board))
                    assembly = directory / "assembly.toml"
                    assembly.write_text(
                        'schema=2\nboard_package="."\nbackend="baremetal"\n'
                        'clock="hse8-pll168"\noptimization="Os"\n'
                        '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                        'flash_load_limit_bytes=65536\n'
                        'static_ram_limit_bytes=32768\n'
                        '[adc.input]\nbinding="input"\nmode="single-shot"\n'
                        f'channels=[{channel}]\nsample_times=[7]\n'
                        'reference_mv=3300\n')
                    result = gate.resolve_ir(assembly)
                    selected, = result.controllers
                    self.assertEqual(selected.options.channels, (channel,))
                    source = bindings.emit(result)[1]
                    self.assertIn("nx_stm32_adc_prepare", source)
                    self.assertIn(f"= {{ {channel}u }};", source)
                    self.assertNotIn("ADC1->", source)
                    claims = {claim.key for claim in result.resource_plan.claims
                              if claim.owner == "input"}
                    self.assertIn("pin:" + self.PINS[channel], claims)
                    self.assertNotIn("dma:DMA2:stream4", claims)

    def test_full_scan_fixtures_preserve_order_and_claim_all_three_banks(self):
        directory = gate.ROOT / "tests/contracts/adc_external_assembly"
        for part in ("ve", "zg"):
            with self.subTest(part=part):
                result = gate.resolve_ir(directory / f"{part}-scan.toml")
                selected, = result.controllers
                self.assertEqual(selected.options.channels,
                                 tuple(reversed(range(16))))
                claims = {claim.key for claim in result.resource_plan.claims
                          if claim.owner == "analog"}
                self.assertEqual({key for key in claims if key.startswith("pin:")},
                                 {"pin:" + pin for pin in self.PINS})
                self.assertIn("nx_stm32_adc_prepare", bindings.emit(result)[1])

    def test_route_bank_selects_only_authored_channels_before_resource_claims(self):
        fixture = gate.ROOT / "tests/contracts/adc_external_assembly"
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(fixture / "ve_board/board.json")
            (directory / "board.json").write_text(json.dumps(board))
            source = (fixture / "ve-scan.toml").read_text()
            source = source.replace('board_package = "ve_board"',
                                    'board_package = "."')
            source = source.replace(
                'channels = [15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0]',
                'channels = [15, 9, 3, 10, 8, 0, 15]')
            source = source.replace(
                'sample_times = [7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7]',
                'sample_times = [7, 7, 7, 7, 7, 7, 7]')
            assembly = directory / "assembly.toml"
            assembly.write_text(source)
            result = gate.resolve_ir(assembly)
            selected, = result.controllers
            self.assertEqual(selected.options.channels, (15, 9, 3, 10, 8, 0, 15))
            expected = {"PC5", "PB1", "PA3", "PC0", "PB0", "PA0"}
            self.assertEqual({pin["pin"] for pin in selected.pins}, expected)
            claims = {claim.key for claim in result.resource_plan.claims
                      if claim.owner == "analog" and claim.key.startswith("pin:")}
            self.assertEqual(claims, {"pin:" + pin for pin in expected})
            generated = bindings.emit(result)[1]
            self.assertIn(".channel_count = 7u", generated)
            self.assertIn(".pin_count = 6u", generated)
            self.assertEqual(len(result.routes["adc1-scan-external16"]["pins"]), 16)


    def test_independent_factories_select_three_distinct_controllers(self):
        fixture = gate.ROOT / "tests/contracts/adc_external_assembly"
        for density in ("ve", "zg"):
            with self.subTest(density=density):
                result = gate.resolve_ir(fixture / f"{density}-independent.toml")
                self.assertEqual([item.controller for item in result.controllers],
                                 ["ADC1", "ADC2", "ADC3"])
                self.assertEqual([item.options.resolution_bits
                                  for item in result.controllers], [12, 10, 6])
                source = bindings.emit(result)[1]
                for controller in ("ADC1", "ADC2", "ADC3"):
                    self.assertIn(f".registers = {controller}", source)
                    self.assertNotIn(controller + "->", source)
                self.assertEqual({claim.key for claim in result.resource_plan.claims
                                  if claim.owner in {"a", "b", "c"}
                                  and claim.key.startswith("pin:")},
                                 {"pin:PA4", "pin:PB0",
                                  "pin:PC0" if density == "ve" else "pin:PF6"})
                self.assertFalse(any(claim.key.startswith("dma:")
                                     for claim in result.resource_plan.claims))

    def test_adc3_pf_input_requires_the_bonded_package(self):
        fixture = gate.ROOT / "tests/contracts/adc_external_assembly"
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(fixture / "zg_board/board.json")
            assembly = directory / "assembly.toml"
            assembly.write_text((fixture / "zg-independent.toml").read_text().replace(
                'board_package = "zg_board"', 'board_package = "."'))
            board["soc"] = "STM32F407VET6"
            (directory / "board.json").write_text(json.dumps(board))
            with self.assertRaises(ValueError):
                gate.resolve_ir(assembly)
            board["soc"] = "STM32F407ZGT6"
            (directory / "board.json").write_text(json.dumps(board))
            result = gate.resolve_ir(assembly)
            self.assertEqual(result.controllers[2].pins[0]["pin"], "PF6")

    def dma_assembly(self, directory, density, controller, mode, peer=False):
        fixture = gate.ROOT / "tests/contracts/adc_external_assembly"
        board = gate.load(fixture / f"{density}_board/board.json")
        (directory / "board.json").write_text(json.dumps(board))
        binding, channel = ("independent2", 8) if controller == "ADC2" else (
            "independent3", 10 if density == "ve" else 4)
        source = ('schema=2\nboard_package="."\nbackend="baremetal"\n'
                  'clock="hse8-pll168"\noptimization="Os"\n'
                  '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                  'flash_load_limit_bytes=65536\nstatic_ram_limit_bytes=32768\n'
                  '[adc.input]\nbinding=' + json.dumps(binding) + '\nmode=' +
                  json.dumps(mode) + f'\nchannels=[{channel}]\nsample_times=[7]\n'
                  'reference_mv=3300\n[adc.input.irq]\npriority=5\n')
        if peer:
            source += ('[adc.peer]\nbinding="independent3"\n'
                       'mode="trigger-dma"\nchannels=[10]\nsample_times=[7]\n'
                       'reference_mv=3300\n[adc.peer.irq]\npriority=5\n')
        assembly = directory / "assembly.toml"
        assembly.write_text(source)
        return assembly

    def test_secondary_dma_modes_use_exact_lanes_and_keep_the_timer_exclusive(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for density in ("ve", "zg"):
                for controller, lane, request in (("ADC2", 2, 1), ("ADC3", 1, 2)):
                    for mode in ("trigger-dma", "trigger-dma-continuous"):
                        with self.subTest(density=density, controller=controller,
                                          mode=mode):
                            assembly = self.dma_assembly(
                                directory, density, controller, mode)
                            result = gate.resolve_ir(assembly)
                            selected, = result.controllers
                            self.assertEqual(selected.controller, controller)
                            self.assertEqual([(item.engine, item.lane, item.request)
                                              for item in selected.integration.dma],
                                             [("DMA2", lane, request)])
                            self.assertIn("controller:TIM3",
                                          {item.key for item in result.claims})
                            source = bindings.emit(result)[1]
                            self.assertIn(f"void DMA2_Stream{lane}_IRQHandler(void)",
                                          source)
                            self.assertNotIn("void DMA2_Stream4_IRQHandler(void)",
                                             source)

    def test_distinct_adc_dma_lanes_do_not_authorize_two_tim3_trigger_owners(self):
        with tempfile.TemporaryDirectory() as temporary:
            assembly = self.dma_assembly(Path(temporary), "ve", "ADC2",
                                         "trigger-dma", peer=True)
            with self.assertRaisesRegex(ValueError, "controller:TIM3"):
                gate.resolve_ir(assembly)


    def test_regular_dma_trigger_choices_have_typed_resources_and_static_links(self):
        choices = (("tim3-trgo", "TIM3", "TIM3_TRGO"),
                   ("tim1-ch1", "TIM1", "TIM1_CH1"),
                   ("tim1-ch2", "TIM1", "TIM1_CH2"),
                   ("tim1-ch3", "TIM1", "TIM1_CH3"),
                   ("tim3-ch1", "TIM3", "TIM3_CH1"),
                   ("tim4-ch4", "TIM4", "TIM4_CH4"),
                   ("tim5-ch1", "TIM5", "TIM5_CH1"),
                   ("tim5-ch2", "TIM5", "TIM5_CH2"),
                   ("tim5-ch3", "TIM5", "TIM5_CH3"),
                   ("tim8-ch1", "TIM8", "TIM8_CH1"),
                   ("tim8-trgo", "TIM8", "TIM8_TRGO"))
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for trigger, timer, symbol in choices:
                for mode in ('trigger-dma', 'trigger-dma-continuous'):
                    with self.subTest(trigger=trigger, mode=mode):
                        assembly = self.dma_assembly(directory, 've', 'ADC2', mode)
                        assembly.write_text(assembly.read_text().replace(
                            'reference_mv=3300',
                            'reference_mv=3300\ntrigger='+json.dumps(trigger)))
                        result = gate.resolve_ir(assembly)
                        self.assertEqual(trigger, result.controllers[0].options.trigger)
                        claims = {claim.key for claim in result.claims}
                        self.assertIn('controller:'+timer, claims)
                        if timer != 'TIM3':
                            self.assertNotIn('controller:TIM3', claims)
                        self.assertIn('.trigger = NX_STM32_ADC_'+symbol,
                                      bindings.emit(result)[1])

    def test_independent_adc_dma_can_use_distinct_timer_owners(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            assembly = self.dma_assembly(directory, 've', 'ADC2', 'trigger-dma', peer=True)
            text = assembly.read_text().replace(
                '[adc.input.irq]', 'trigger="tim4-ch4"\n[adc.input.irq]').replace(
                '[adc.peer.irq]', 'trigger="tim8-trgo"\n[adc.peer.irq]')
            text += ('[adc.third]\nbinding="independent1"\nmode="trigger-dma-continuous"\n'
                     'channels=[4]\nsample_times=[7]\nreference_mv=3300\ntrigger="tim1-ch2"\n'
                     '[adc.third.irq]\npriority=5\ncalls_os=false\n')
            assembly.write_text(text)
            result = gate.resolve_ir(assembly)
            claims = {claim.key for claim in result.claims}
            self.assertTrue({'controller:TIM1', 'controller:TIM4', 'controller:TIM8'} <= claims)
            self.assertNotIn('controller:TIM3', claims)
            self.assertEqual(3, len(result.controllers))

    def test_selected_adc_timer_still_excludes_pwm_on_a_different_pin(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            assembly = self.dma_assembly(directory, 've', 'ADC2', 'trigger-dma')
            board_path = directory/'board.json'
            board = json.loads(board_path.read_text())
            board['bindings'].append({'id': 'timer', 'controller': 'TIM5',
                                       'route': 'tim5-ch4-pa3'})
            board_path.write_text(json.dumps(board))
            assembly.write_text(assembly.read_text().replace(
                '[adc.input.irq]', 'trigger="tim5-ch1"\n[adc.input.irq]')+
                '[pwm.output]\nbinding="timer"\nmode="fixed-pwm"\n'
                'tick_hz=1000000\nperiod_ticks=1000\nduty_ticks=0\n')
            with self.assertRaisesRegex(ValueError, 'controller:TIM5'):
                gate.resolve_ir(assembly)

    def test_unknown_reserved_or_software_trigger_selections_reject(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for trigger in ('tim2-trgo', 'exti11', 'tim9-ch1', 'tim8-unknown', 8, True):
                with self.subTest(trigger=trigger):
                    assembly = self.dma_assembly(directory, 've', 'ADC2', 'trigger-dma')
                    assembly.write_text(assembly.read_text().replace(
                        '[adc.input.irq]', 'trigger='+json.dumps(trigger)+'\n[adc.input.irq]'))
                    with self.assertRaises(ValueError):
                        gate.resolve_ir(assembly)
            assembly = self.dma_assembly(directory, 've', 'ADC2', 'trigger-dma')
            text = assembly.read_text().split('[adc.input.irq]')[0]
            assembly.write_text(text.replace('trigger-dma', 'single-shot')+'trigger="tim3-trgo"\n')
            with self.assertRaises(ValueError):
                gate.resolve_ir(assembly)


if __name__ == "__main__":
    unittest.main()
