"""Exact SAI static wiring seams; no stand-in for a device/Board resolution."""
from dataclasses import FrozenInstanceError, replace
from pathlib import Path
import unittest
import json
import re
import tempfile
from tools.configure import bindings, configure, factory
from tools.configure.resource_graph import ResourceClaimIR, ResourcePlanIR
from tools.configure.ir import freeze, mode_options
from tools.configure.contracts import CONTROLLER_CONTRACTS
from tools.configure.stm32f4_descriptor import DESCRIPTOR
from tools.configure.stm32f4 import capture_audio, contract, emission, preflight, sai



class SaiConfigurationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = configure.resolve_ir(configure.ROOT /
            "tests/contracts/f407_capture_audio/ve-i2s2-duplex-blocks.toml")

    def fixture(self, mixed=False):
        # Reuse the actual immutable owner/result shape for a pure emitter
        # unit. Complete SAI part/package/route resolution is a separate gate.
        i2s, = self.base.controllers
        sai = replace(i2s, id="sai", controller="SAI1", mode="slave-duplex",
                      options=mode_options({**self.raw(), "mode": "slave-duplex"}))
        claims = tuple(ResourceClaimIR(key, sai.id, "exclusive", "unit",
                                       "exclusive", (), None) for key in
                       ("irq:SAI1", "dma:DMA2:stream1", "dma:DMA2:stream4"))
        return replace(self.base, controllers=((i2s, sai) if mixed else (sai,)),
                       resource_plan=ResourcePlanIR(claims))

    @staticmethod
    def raw():
        return dict(kind="audio", controller="SAI1", mode="master-duplex",
                    sample_bits=24, slot_bits=32, slots=2, frame_sync_bits=32,
                    first_bit_offset=0, frame_sync_high=False,
                    frame_sync_before=True, frame_sync_channels=True,
                    first_lsb=False, sample_rising=True, mclk=False,
                    no_divider=False, mckdiv=0, max_bit_clock_hz=3072000,
                    poll_limit=128, source_block="A", irq_priority=5)

    def test_sai_has_one_real_frozen_option_record_without_i2s_pll_choices(self):
        from tools.configure.ir import mode_options
        options = mode_options(self.raw())
        self.assertEqual("SaiOptions", type(options).__name__)
        self.assertEqual("A", options.source_block)
        self.assertEqual(24, options.sample_bits)
        self.assertEqual(5, options.irq.priority)
        for field in ("pll_n", "pll_r", "pll_m", "clock", "standard",
                      "frame_bits", "tx_block", "rx_block"):
            self.assertFalse(hasattr(options, field), field)
        with self.assertRaises(FrozenInstanceError):
            options.source_block = "B"

    @classmethod
    def selection(cls, mode="master-duplex"):
        result = cls.raw()
        for key in ("kind", "controller", "source_block"):
            result.pop(key)
        result.update(binding="sound", mode=mode)
        if mode.endswith("-blocks"):
            result["max_service_gap_us"] = 10
        return result

    @staticmethod
    def route(block="A", duplex=True, mclk=False, controller="SAI1"):
        pins = ([('PE4', 'fs_a'), ('PE5', 'ck_a'), ('PE6', 'sd_a')]
                if block == "A" else
                [('PF9', 'fs_b'), ('PF8', 'ck_b'), ('PE3', 'sd_b')])
        if duplex:
            pins += [('PE3', 'sd_b')] if block == "A" else [('PE6', 'sd_a')]
        if mclk:
            pins += [('PE2', 'mclk_a')] if block == "A" else [('PF7', 'mclk_b')]
        if controller == "SAI2":
            pads = dict(PE4="PD12", PE5="PD13", PE6="PD11", PE3="PE11",
                        PE2="PE0", PF9="PE13", PF8="PE12", PF7="PE14")
            pins = [(pads[pad], signal) for pad, signal in pins]
        return dict(kind="audio", controller=controller, id="sai-unit",
                    pins=[dict(pin=pad, function=signal,
                               af=10 if controller == "SAI2" else 6)
                          for pad, signal in pins])

    @staticmethod
    def clocks(auxiliary=True):
        from tools.configure.stm32f4.clocks import plan, projection
        profile = dict(source="hsi", input_hz=16000000, pll_m=16, pll_n=336,
            pll_p=4, pll_q=7, ahb_div=1, apb1_div=1, apb2_div=1,
            adc_div=4, supply_mv=3300, css=False, sysclk_source="input", pll48=False)
        if auxiliary:
            profile["pll_sai"] = dict(m=10, n=192,
                                      sai1=dict(q=5, post_divider=5))
        selected = plan("custom", profile)
        return projection(selected), selected

    def test_sai_master_does_not_join_the_unrelated_i2s_pll_tuple_set(self):
        values = {**self.selection(), "kind": "audio", "controller": "SAI1"}
        contract.validate_controller_set((values,), clock_profile="custom")

    def test_exact_source_and_dma_come_from_pads_for_each_finite_direction(self):
        for block in ("A", "B"):
            for direction in ("tx", "rx", "duplex"):
                with self.subTest(block=block, direction=direction):
                    route = self.route(block, direction == "duplex")
                    selected = self.selection("slave-" + direction)
                    clock, plan = self.clocks(False)
                    sai.validate(selected, route)
                    resolved = sai.resolve_fields(selected, route, clock, plan)
                    self.assertEqual(block, resolved["source_block"])
                    primary = (2, 1, 0) if block == "A" else (2, 4, 1)
                    peer = (2, 4, 1) if block == "A" else (2, 1, 0)
                    self.assertEqual((primary, peer) if direction == "duplex"
                                     else (primary,), sai.dma_paths(route, selected["mode"]))

    def test_sai_rejects_unsupported_modes_holes_and_wrong_af_without_repair(self):
        for change in (dict(mode="slave-spdif"), dict(slots=17),
                       dict(sample_bits=12), dict(slot_bits=8),
                       dict(active_slots=0), dict(source_block="B"),
                       dict(standard="philips"), dict(first_bit_offset=9),
                       dict(frame_sync_bits=0), dict(mclk=True),
                       dict(no_divider=True), dict(calls_os=True)):
            with self.subTest(change=change), self.assertRaises(ValueError):
                sai.validate({**self.selection("slave-duplex"), **change}, self.route())
        route = self.route()
        route["pins"][0]["af"] = 5
        with self.assertRaises(ValueError):
            sai.validate(self.selection(), route)

    def test_sparse_slots_preserve_wire_geometry_and_resolve_the_enabled_mask(self):
        selected = {**self.selection("slave-tx"), "slots": 8,
                    "frame_sync_bits": 128, "active_slots": 0x81}
        route = self.route(duplex=False)
        sai.validate(selected, route)
        clock, plan = self.clocks(False)
        resolved = sai.resolve_fields(selected, route, clock, plan)
        self.assertEqual(0x81, resolved["active_slots"])
        for invalid in (0, -1, 256, True):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                sai.validate({**selected, "active_slots": invalid}, route)

    def test_sai2_uses_its_exact_af10_pads_and_distinct_dma_requests(self):
        for block in ("A", "B"):
            with self.subTest(block=block):
                route = self.route(block, controller="SAI2")
                selected = self.selection("slave-duplex")
                sai.validate(selected, route)
                primary = (2, 4, 3) if block == "A" else (2, 6, 3)
                peer = (2, 6, 3) if block == "A" else (2, 4, 3)
                self.assertEqual((primary, peer),
                                 sai.dma_paths(route, selected["mode"]))
                route["pins"][0]["af"] = 6
                with self.assertRaises(ValueError):
                    sai.validate(selected, route)

    def test_continuous_sai_resolves_a_separate_personality_and_service_bound(self):
        selected = self.selection("slave-duplex-blocks")
        route = self.route()
        sai.validate(selected, route)
        clock, plan = self.clocks(False)
        resolved = sai.resolve_fields(selected, route, clock, plan)
        self.assertTrue(resolved["continuous"])
        self.assertEqual(10, resolved["max_service_gap_us"])
        self.assertEqual(((2, 1, 0), (2, 4, 1)),
                         sai.dma_paths(route, selected["mode"]))
        for change in (dict(max_service_gap_us=0),
                       dict(max_service_gap_us=1000001)):
            with self.subTest(change=change), self.assertRaises(ValueError):
                sai.validate({**selected, **change}, route)

    def test_sai_master_requires_its_one_system_source_and_safe_clock_ratio(self):
        selected = self.selection()
        clock, plan = self.clocks(False)
        with self.assertRaises(ValueError):
            sai.resolve_fields(selected, self.route(), clock, plan)
        clock, plan = self.clocks()
        resolved = sai.resolve_fields(selected, self.route(), clock, plan)
        self.assertEqual("A", resolved["source_block"])
        with self.assertRaises(ValueError):
            sai.resolve_fields({**selected, "max_bit_clock_hz": 2000000},
                               self.route(), clock, plan)
        with self.assertRaises(ValueError):
            sai.resolve_fields({**selected, "no_divider": True,
                                "max_bit_clock_hz": 12288000},
                               self.route(), clock, plan)

    def test_mclk_output_range_never_restricts_an_unselected_clock_pad(self):
        clock, plan = self.clocks()
        selected = {**self.selection("master-tx"), "mckdiv": 7}
        self.assertEqual("A", sai.resolve_fields(selected,
            self.route(duplex=False), clock, plan)["source_block"])
        with self.assertRaises(ValueError):
            sai.resolve_fields({**selected, "mclk": True},
                self.route(duplex=False, mclk=True), clock, plan)
        selected["mckdiv"] = 0
        self.assertEqual("A", sai.resolve_fields({**selected, "mclk": True},
            self.route(duplex=False, mclk=True), clock, plan)["source_block"])

    def test_each_interrupt_owner_requires_an_explicit_priority(self):
        selected = self.selection("slave-tx")
        selected.pop("irq_priority")
        with self.assertRaises(ValueError):
            sai.validate(selected, self.route(duplex=False))

    def test_sai_static_constructor_has_no_register_script_or_extra_pll_owner(self):
        for block in ("A", "B"):
            with self.subTest(block=block):
                selected = self.selection()
                route = self.route(block)
                clock, plan = self.clocks()
                resolved = sai.resolve_fields(selected, route, clock, plan)
                options = mode_options({**selected, **resolved, "kind": "audio",
                                        "controller": "SAI1"})
                paths = sai.dma_paths(route, selected["mode"])
                item = replace(self.base.controllers[0], controller="SAI1",
                    mode=selected["mode"], pins=freeze(route["pins"]), options=options,
                    dma=tuple(f"DMA{engine}:stream{lane}:channel{channel}"
                              for engine, lane, channel in paths))
                built = sai.constructor(item, self.base)
                text = "\n".join(built.definitions)
                self.assertEqual("nx_stm32_sai_state_t", built.state_type)
                self.assertIn("static const nx_stm32_sai_config_t", text)
                self.assertIn(".pclk_hz = NEXUS_APB2_HZ", text)
                self.assertIn(".frame_bits = 64u", text)
                self.assertIn(".active_slots = 3u", text)
                self.assertIn(".source = SAI1_Block_" + block, text)
                self.assertIn("SAI1_IRQHandler", text)
                self.assertIn("nx_stm32_sai_dma_irq", text)
                # The common bindings connection owns the single final DSB.
                for forbidden in ("s_nx_audio_clock", "RCC->", "while (",
                                  "HAL_SAI", "malloc", "__HAL_RCC"):
                    self.assertNotIn(forbidden, text)

    def test_sai_preflight_uses_its_apb2_gate_and_exact_vector(self):
        result = self.fixture()
        text = "\n".join(preflight.admission_declarations(result, "NULL", 0))
        self.assertIn("RCC_APB2ENR_SAI1EN", text)
        self.assertIn("SAI1_IRQn", text)
        self.assertNotIn("RCC_APB1ENR_SPI1EN", text)
        self.assertNotIn("->CR", text)

    def test_sai_uses_its_actual_private_type_and_readonly_ops(self):
        result = self.fixture()
        sai, = result.controllers
        text = "\n".join(emission.includes(result))
        self.assertIn('#include "stm32f4_sai.h"', text)
        self.assertNotIn('"stm32f407_audio.h"', text)
        self.assertNotIn('"stm32f407_audio_blocks.h"', text)
        self.assertEqual("sai", emission.implementation(sai))
        self.assertEqual("nx_stm32_sai_ops", emission.operations(sai))

    def test_sai_does_not_allocate_an_unrelated_i2s_clock_owner(self):
        text = "\n".join(emission.state_declarations(self.fixture()))
        self.assertNotIn("nx_stm32_audio_clock_t", text)
        self.assertNotIn("s_nx_audio_clock", text)
        self.assertIn("s_nx_platform_dma_clocks_added", text)

    def test_mixed_audio_retains_one_i2s_owner_and_each_selected_backend(self):
        result = self.fixture(mixed=True)
        text = "\n".join(emission.includes(result))
        self.assertIn('"stm32f4_sai.h"', text)
        self.assertIn('"stm32f407_audio.h"', text)
        self.assertIn('"stm32f407_audio_blocks.h"', text)
        state = "\n".join(emission.state_declarations(result))
        self.assertEqual(1, state.count("nx_stm32_audio_clock_t"))


    def test_family_audio_entry_uses_sai_validation_resolution_and_construction(self):
        selected, route = self.selection(), self.route()
        clock, plan = self.clocks()
        capture_audio.validate(selected, route)
        resolved = capture_audio.resolve_fields(selected, route, clock, plan)
        self.assertEqual("A", resolved["source_block"])
        item = replace(self.base.controllers[0], controller="SAI1",
            mode=selected["mode"], pins=freeze(route["pins"]),
            options=mode_options({**selected, **resolved, "kind": "audio",
                                  "controller": "SAI1"}),
            dma=("DMA2:stream1:channel0", "DMA2:stream4:channel1"))
        self.assertEqual("nx_stm32_sai_state_t",
                         capture_audio.constructor(item, self.base).state_type)
        with self.assertRaises(ValueError):
            capture_audio.validate(self.selection("slave-unknown"), route)

    def test_family_dma_gate_uses_the_exact_sai_source_and_peer_order(self):
        for block in ("A", "B"):
            route = self.route(block)
            paths = sai.dma_paths(route, "slave-duplex")
            route["dma"] = [f"DMA{e}:stream{s}:channel{c}" for e, s, c in paths]
            contract.validate_dma(route, "slave-duplex", None)
            route["dma"].reverse()
            with self.assertRaises(ValueError):
                contract.validate_dma(route, "slave-duplex", None)

    def test_exact_device_resolver_and_factory_use_the_reviewed_finite_sai_slice(self):
        # Exercise the canonical provider, actual reviewed facts and locked
        # vendor identities. Only the Board/assembly are temporary inputs.
        with tempfile.TemporaryDirectory(prefix="sai assembly ") as temporary:
            root = Path(temporary)
            for part, controller in (("STM32F446ZET6", "SAI1"),
                                     ("STM32F446ZET6", "SAI2"),
                                     ("STM32F479NIH6", "SAI1")):
                for block in ("A", "B"):
                    for mode in sai.MODES:
                        with self.subTest(part=part, controller=controller,
                                          block=block, mode=mode):
                            directory = root / "board"
                            directory.mkdir(exist_ok=True)
                            route = controller.lower() + "-" + block.lower() + ("-duplex"
                                if mode.removesuffix("-blocks").endswith("duplex") else "-simplex")
                            board = dict(schema_version=1, id="sai-software-fixture",
                                soc=part, soc_family="stm32f4", source_revision="v1",
                                physical_pcb_revision=None, clocks={},
                                bindings=[dict(id="sound", controller=controller, route=route)],
                                reserved_resources=["PA13", "PA14"],
                                unknowns=["Software fixture; no PCB or physical qualification"],
                                provenance=[dict(url="https://www.st.com/",
                                                 scope="Reviewed exact part SAI routes only")])
                            (directory / "board.json").write_text(json.dumps(board))
                            _, plan = self.clocks(mode.startswith("master-"))
                            from tools.configure.stm32f4.clocks import DEFAULT
                            parameters = {key: value for key, value in
                                plan["parameters"].items() if key in DEFAULT}
                            parameters.update(sysclk_source="input", pll48=False)
                            if part.startswith("STM32F479"):
                                parameters.update(pll_m=10, pll_n=192, pll_q=5)
                            if plan.get("pll_sai") is not None:
                                parameters["pll_sai"] = dict(m=10, n=192,
                                    sai1=dict(q=5, post_divider=5))
                            lines = ['schema=2', 'board_package="board"',
                                'backend="baremetal"', 'clock="custom"',
                                '[clock_parameters]']
                            for key, value in parameters.items():
                                if key != "pll_sai":
                                    lines.append(key + "=" + json.dumps(value))
                            if "pll_sai" in parameters:
                                lines.append('pll_sai={m=10,n=192,sai1={q=5,post_divider=5}}')
                            lines += ['[memory]', 'main_stack_bytes=2048',
                                      '[audio.sound]']
                            for key, value in self.selection(mode).items():
                                if key != "irq_priority":
                                    lines.append(key + "=" + json.dumps(value))
                            lines.append('irq={priority=5}')
                            path = root / "assembly.toml"
                            path.write_text("\n".join(lines) + "\n")
                            result = configure.resolve_ir(path)
                            item, = result.controllers
                            self.assertEqual("SaiOptions", type(item.options).__name__)
                            self.assertEqual(block, item.options.source_block)
                            self.assertFalse(result.physical_qualified)
                            source = bindings.emit(result)[1]
                            suffix = "_blocks" if mode.endswith("-blocks") else ""
                            self.assertIn("nx_stm32_sai" + suffix + "_prepare", source)
                            self.assertIn("nx_stm32_sai" + suffix + "_ops", source)
                            self.assertNotIn("s_nx_audio_clock", source)
                            for body in re.findall(r"void \w+_IRQHandler\(void\) \{([^}]+)\}", source):
                                self.assertEqual(1, body.count("nx_arch_dsb()"))
                            self.assertEqual(16000000, item.integration.clock_hz)
                            self.assertIn("nx_factory_audio", factory.emit(result)[0])

    def test_shared_authored_fields_and_input_identity_include_the_sai_leaf(self):
        fields = CONTROLLER_CONTRACTS["audio"]
        self.assertFalse(sai.FIELDS - fields.selected_fields)
        self.assertFalse(sai.FIELDS - fields.authored_fields)
        self.assertIn("tools/configure/stm32f4/sai.py",
                      DESCRIPTOR.configuration_helpers)


if __name__ == "__main__":
    unittest.main(verbosity=2)
