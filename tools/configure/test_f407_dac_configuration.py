"""F407 DAC choices retain typed static ownership and exact pad admission."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, bindings, configure as gate, factory


class F407DacConfigurationTests(unittest.TestCase):
    def resolve(self, directory, *, density="ve", mode="direct", channels="[1,2]",
                data_format="u12-right", idle="[0,0]", trigger=None,
                extra="", adc_conflict=False):
        fixture = gate.ROOT / "tests/contracts/dac_assembly"
        board = gate.load(fixture / f"{density}_board/board.json")
        board["id"] = f"stm32f407{density}-dac-software-fixture"
        board["bindings"] = [{"id": "output", "controller": "DAC",
                              "route": "dac-pa4-pa5"}]
        if adc_conflict:
            board["bindings"].append({"id": "analog", "controller": "ADC1",
                                      "route": "adc1-ch4-pa4"})
        (directory / "board.json").write_text(json.dumps(board))
        trigger = trigger if trigger is not None else (
            "none" if mode == "direct" else "software")
        source = (
            'schema=2\nboard_package="."\nbackend="baremetal"\n'
            'clock="hse8-pll168"\noptimization="Os"\n'
            '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
            '[dac.output]\nbinding="output"\n'
            f'mode="{mode}"\nchannels={channels}\nformat="{data_format}"\n'
            f'idle_codes={idle}\noutput_buffer=true\ntrigger="{trigger}"\n'
            + extra)
        if adc_conflict:
            source += ('\n[adc.input]\nbinding="analog"\nmode="single-shot"\n'
                       'channels=[4]\nsample_times=[7]\nreference_mv=3300\n')
        assembly = directory / "assembly.toml"
        assembly.write_text(source)
        return gate.resolve_ir(assembly)

    def test_direct_formats_resolve_for_both_exact_parts(self):
        with tempfile.TemporaryDirectory() as temporary:
            for density in ("ve", "zg"):
                for data_format in ("u8", "u12-right", "u12-left"):
                    with self.subTest(density=density, data_format=data_format):
                        result = self.resolve(Path(temporary), density=density,
                                              data_format=data_format)
                        port = result.controllers[0]
                        self.assertEqual("DAC", port.controller)
                        self.assertEqual((1, 2), port.options.channels)
                        self.assertEqual(data_format, port.options.format)
                        self.assertEqual((0, 0), port.options.idle_codes)
                        with self.assertRaises(FrozenInstanceError):
                            port.options.output_buffer = False

    def test_selected_channel_claims_only_its_authorized_pad(self):
        with tempfile.TemporaryDirectory() as temporary:
            for channels, pin in (("[1]", "PA4"), ("[2]", "PA5")):
                with self.subTest(channels=channels):
                    result = self.resolve(Path(temporary), channels=channels,
                                          idle="[0]")
                    self.assertEqual([pin], [row["pin"]
                        for row in result.controllers[0].pins])
                    claimed = {row.key for row in result.claims
                               if row.owner == "output" and
                               row.key.startswith("pin:")}
                    self.assertEqual({"pin:" + pin}, claimed)

    def test_software_trigger_and_hardware_wave_modes_resolve(self):
        with tempfile.TemporaryDirectory() as temporary:
            for mode in ("triggered", "noise", "triangle"):
                with self.subTest(mode=mode):
                    extra = "amplitude=255\n" if mode != "triggered" else ""
                    result = self.resolve(Path(temporary), mode=mode,
                                          extra=extra)
                    self.assertEqual("software", result.controllers[0].
                                     options.trigger)

    def test_factory_keeps_a_nominal_dac_identity_and_c_lifecycle(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.resolve(Path(temporary))
            header, _ = factory.emit(result)
            self.assertIn("nx_factory_dac", header)
            self.assertIn("nx_dac_port_t", header)
            source = bindings.emit(result)[1]
            self.assertIn("nx_stm32_dac_prepare", source)
            self.assertIn("nx_stm32_dac_close", source)
            self.assertNotIn("HAL_DAC_Start", source)

    def test_channel_selection_is_nonempty_unique_and_physical(self):
        with tempfile.TemporaryDirectory() as temporary:
            for channels in ("[]", "[0]", "[3]", "[1,1]", "[true]"):
                with self.subTest(channels=channels):
                    with self.assertRaisesRegex(ValueError, "DAC.*channel"):
                        self.resolve(Path(temporary), channels=channels)

    def test_idle_codes_have_exact_count_and_raw_resolution_range(self):
        with tempfile.TemporaryDirectory() as temporary:
            for data_format, idle in (("u8", "[256,0]"),
                                      ("u12-right", "[4096,0]"),
                                      ("u12-left", "[-1,0]"),
                                      ("u12-right", "[0]"),
                                      ("u12-right", "[false,0]")):
                with self.subTest(data_format=data_format, idle=idle):
                    with self.assertRaisesRegex(ValueError, "DAC.*idle"):
                        self.resolve(Path(temporary), data_format=data_format,
                                     idle=idle)

    def test_direct_mode_cannot_silently_accept_a_trigger(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "DAC.*trigger"):
                self.resolve(Path(temporary), trigger="software")

    def test_wave_amplitude_is_exact_and_cannot_wrap_the_raw_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            for extra, idle in (("amplitude=256\n", "[0,0]"),
                                 ("amplitude=4095\n", "[1,0]")):
                with self.subTest(extra=extra):
                    with self.assertRaisesRegex(ValueError, "DAC.*amplitude"):
                        self.resolve(Path(temporary), mode="triangle",
                                     idle=idle, extra=extra)

    def test_unimplemented_double_buffer_is_not_advertised(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError,
                                        "[Uu]nsupported|[Uu]nmaintained|not.*support"):
                self.resolve(Path(temporary), mode="trigger-dma-doublebuffer")

    def test_dma_selects_one_exact_stream_and_basic_timer_for_each_part(self):
        with tempfile.TemporaryDirectory() as temporary:
            for density in ("ve", "zg"):
                for mode in ("trigger-dma", "trigger-dma-circular"):
                    for channels, idle, stream in (
                            ("[1]", "[0]", 5), ("[2]", "[0]", 6),
                            ("[1,2]", "[0,0]", 5)):
                        with self.subTest(density=density, mode=mode,
                                          channels=channels):
                            result = self.resolve(Path(temporary),
                                density=density, mode=mode, channels=channels,
                                idle=idle, trigger="tim7-trgo",
                                extra="irq={priority=6,calls_os=false}\n")
                            item = result.controllers[0]
                            self.assertEqual((f"DMA1:stream{stream}:channel7",),
                                             item.dma)
                            self.assertEqual(6, item.options.irq.priority)
                            claims = {row.key for row in result.claims}
                            self.assertTrue({"controller:TIM7",
                                "irq:TIM6_DAC", f"irq:DMA1_Stream{stream}"}
                                .issubset(claims))

    def test_dma_factory_exposes_a_static_facet_with_the_same_nominal_id(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.resolve(Path(temporary), mode="trigger-dma",
                trigger="tim6-trgo", extra="irq={priority=5}\n")
            header, lookup = factory.emit(result)
            source = bindings.emit(result)[1]
            self.assertIn("nx_factory_dac_tx(nx_dac_id_t id)", header)
            self.assertIn("nx_factory_dac_tx_ref(nx_dac_ref_t id)",
                          factory.emit_strong(result))
            self.assertIn("case NX_DAC_ID_OUTPUT:", lookup)
            self.assertIn("return &s_nx_dac_tx_output;", lookup)
            for token in ("nx_stm32_dac_dma_prepare",
                          "nx_stm32_dac_dma_close",
                          "nx_stm32_dac_dma_tx_ops",
                          "DMA1_Stream5_IRQHandler", "TIM6_DAC_IRQHandler"):
                self.assertIn(token, source)
            for token in ("LL_DMA_", "LL_DAC_", "NVIC_", "while ("):
                self.assertNotIn(token, source)

    def test_dma_requires_explicit_irq_priority_and_hardware_trigger(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "DAC.*IRQ"):
                self.resolve(Path(temporary), mode="trigger-dma",
                             trigger="tim6-trgo")
            with self.assertRaisesRegex(ValueError, "DAC.*timer|DAC.*TIM"):
                self.resolve(Path(temporary), mode="trigger-dma",
                             trigger="software", extra="irq={priority=5}\n")

    def test_cpu_dac_modes_reject_unused_irq_configuration(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "DAC.*IRQ"):
                self.resolve(Path(temporary), extra="irq={priority=5}\n")

    def test_cpu_factory_does_not_allocate_a_dma_extension_or_transfer_face(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.resolve(Path(temporary))
            header, lookup = factory.emit(result)
            self.assertIn("nx_factory_dac_tx(nx_dac_id_t id)", header)
            self.assertNotIn("return &s_nx_dac_tx_output", lookup)
            source = bindings.emit(result)[1]
            self.assertNotIn("nx_stm32_dac_dma", source)
            self.assertNotIn("s_nx_dac_tx_output", source)

    def test_adc_and_dac_cannot_own_the_same_physical_pad(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "conflict.*pin:PA4"):
                self.resolve(Path(temporary), adc_conflict=True)

    def test_editor_schema_declares_only_real_dac_storage_formats(self):
        schema = authored.editor_schema()["properties"]["dac"][
            "additionalProperties"]["properties"]
        self.assertEqual(["u8", "u12-right", "u12-left"],
                         schema["format"]["enum"])
        self.assertEqual("boolean", schema["output_buffer"]["type"])


class F407DacExternalConfigurationTests(unittest.TestCase):
    def assembly(self, directory, *, mode="triggered", trigger="exti9",
                 trigger_input="source", source_mode="hardware-event",
                 source_line=9, recovery=None, reserved=()):
        board = gate.load(gate.ROOT /
                          "tests/contracts/dac_assembly/ve_board/board.json")
        board["bindings"] = [
            {"id": "output", "controller": "DAC", "route": "dac-pa4-pa5"},
            {"id": "source", "controller": f"EXTI{source_line}",
             "route": f"exti{source_line}-pb{source_line}"}]
        board["reserved_resources"] += list(reserved)
        (directory / "board.json").write_text(json.dumps(board))
        extra = ""
        if trigger_input is not None:
            extra += f'trigger_input="{trigger_input}"\n'
        if recovery is not None:
            extra += f'recovery_trigger="{recovery}"\n'
        if mode.startswith("trigger-dma"):
            extra += 'irq={priority=5}\n'
        if mode in {"noise", "triangle"}:
            extra += 'amplitude=255\n'
        source_fields = '' if source_mode == 'hardware-event' else (
            'event_capacity=2\nirq={priority=5}\n')
        source = (
            'schema=2\nboard_package="."\nbackend="baremetal"\n'
            'clock="hse8-pll168"\n[memory]\n'
            'main_stack_bytes=2048\nlibc_heap_bytes=0\n'
            '[dac.output]\nbinding="output"\n'
            f'mode="{mode}"\nchannels=[1,2]\nformat="u12-right"\n'
            f'idle_codes=[0,0]\noutput_buffer=true\ntrigger="{trigger}"\n'
            + extra + '[exti.source]\nbinding="source"\n'
            f'mode="{source_mode}"\nedge="rising"\npull="down"\n'
            + source_fields)
        path = directory / "assembly.toml"
        path.write_text(source)
        return path

    def test_cpu_wave_sources_resolve_and_prepare_event_before_dac(self):
        with tempfile.TemporaryDirectory() as temporary:
            for mode in ("triggered", "noise", "triangle"):
                with self.subTest(mode=mode):
                    ir = gate.resolve_ir(self.assembly(Path(temporary), mode=mode))
                    source = next(x for x in ir.controllers if x.id == "source")
                    output = next(x for x in ir.controllers if x.id == "output")
                    self.assertEqual("source", output.options.trigger_input)
                    self.assertIsNone(output.options.recovery_trigger)
                    self.assertLess(ir.controllers.index(source),
                                    ir.controllers.index(output))
                    emitted = bindings.emit(ir)[1]
                    self.assertIn(".external = &s_nx_port_source", emitted)
                    self.assertIn("NX_STM32_DAC_TRIGGER_EXTI9", emitted)
                    output_claims = {x.key for x in ir.claims if x.owner == "output"}
                    self.assertFalse(any("TIM6" in x or "TIM7" in x
                                         for x in output_claims))

    def test_circular_external_dma_claims_only_selected_recovery_timer(self):
        with tempfile.TemporaryDirectory() as temporary:
            for recovery in ("tim6-trgo", "tim7-trgo"):
                with self.subTest(recovery=recovery):
                    ir = gate.resolve_ir(self.assembly(Path(temporary),
                        mode="trigger-dma-circular", recovery=recovery))
                    item = next(x for x in ir.controllers if x.id == "output")
                    self.assertEqual(recovery, item.options.recovery_trigger)
                    keys = {x.key for x in ir.claims if x.owner == "output"}
                    timer = recovery.split('-')[0].upper()
                    self.assertTrue({"controller:" + timer,
                                     "irq:TIM6_DAC", "dma:DMA1:stream5"}
                                    .issubset(keys))
                    self.assertNotIn("timer:" + timer, keys)
                    emitted = bindings.emit(ir)[1]
                    self.assertIn(".recovery_trigger = NX_STM32_DAC_TRIGGER_" +
                                  timer, emitted)

    def test_finite_external_and_missing_recovery_refuse_explicitly(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "external.*finite|finite.*external"):
                gate.resolve_ir(self.assembly(Path(temporary),
                    mode="trigger-dma", recovery="tim6-trgo"))
            with self.assertRaisesRegex(ValueError, "recovery.*(timer|TIM6|TIM7)"):
                gate.resolve_ir(self.assembly(Path(temporary),
                    mode="trigger-dma-circular"))

    def test_source_is_exact_hardware_event_line_nine(self):
        with tempfile.TemporaryDirectory() as temporary:
            for fields in ({"trigger_input": None}, {"trigger_input": "missing"},
                           {"source_mode": "edge-event"}, {"source_line": 8}):
                with self.subTest(fields=fields), self.assertRaisesRegex(
                        ValueError, "trigger_input|hardware.event|EXTI9"):
                    gate.resolve_ir(self.assembly(Path(temporary), **fields))

    def test_recovery_timer_conflict_is_detected_by_the_existing_resource_plan(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "Resource conflict.*TIM6"):
                gate.resolve_ir(self.assembly(Path(temporary),
                    mode="trigger-dma-circular", recovery="tim6-trgo",
                    reserved=("controller:TIM6",)))

    def test_internal_sources_refuse_unused_external_fields(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "external|trigger_input"):
                gate.resolve_ir(self.assembly(Path(temporary), trigger="software"))
            with self.assertRaisesRegex(ValueError, "recovery.*DMA|DMA.*recovery"):
                gate.resolve_ir(self.assembly(Path(temporary), recovery="tim6-trgo"))


if __name__ == "__main__":
    unittest.main()
