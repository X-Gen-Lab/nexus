"""Static power-stage domains own shared hardware and selected axes once."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import copy
import json
import tempfile
import unittest

from tools.configure import authored, bindings, emission, factory
from tools.configure.configure import resolve_ir
from tools.configure.coupled_io import CoupledIoPolicy
from tools.configure.pinmux import NumericPinMuxPolicy
from tools.configure.providers.common import ConfigurationError


ROOT = Path(__file__).resolve().parents[5]
ASSEMBLY = ROOT / "tests/integration/assemblies/f407_power_stage/dual.toml"
SINGLE = ROOT / "tests/integration/assemblies/f407_power_stage/single.toml"
CENTER = ROOT / "tests/integration/assemblies/f407_power_stage/center.toml"
PAIR = ROOT / "tests/integration/assemblies/f407_power_stage/pair.toml"
TDM = ROOT / "tests/integration/assemblies/f407_power_stage/tdm.toml"
DC = ROOT / "tests/integration/assemblies/f407_power_stage/dc.toml"


class PowerStageDomainConfiguration(unittest.TestCase):
    def changed(self, before, after, *, right_only=False, assembly=ASSEMBLY, replacements=()):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "assembly.toml"
            content = assembly.read_text().replace(
                'board_package = "domain_board"',
                'board_package = "' + str(ASSEMBLY.parent / "domain_board") + '"')
            if right_only:
                prefix, right = content.split("[pwm_adc.right]", 1)
                content = prefix + "[pwm_adc.right]" + right.replace(before, after)
            else:
                content = content.replace(before, after)
            for old, new in replacements:
                content = content.replace(old, new)
            path.write_text(content)
            return resolve_ir(path)

    def test_editor_types_match_carrier_phase_and_startup_contracts(self):
        properties = authored.editor_schema()["properties"]["pwm_adc"][
            "additionalProperties"]["properties"]
        self.assertEqual(properties["center_aligned"]["type"], "boolean")
        for field, minimum in (("phase_offset_ticks", 0),
                               ("start_tolerance_us", 1)):
            self.assertEqual(properties[field]["type"], "integer")
            self.assertEqual(properties[field]["minimum"], minimum)
            self.assertEqual(properties[field]["maximum"], 0xffffffff)
        self.assertEqual(properties["domain"]["type"], "string")
        self.assertEqual(properties["domain"]["minLength"], 1)

    def test_two_axes_have_one_immutable_physical_domain(self):
        selected = resolve_ir(ASSEMBLY)
        self.assertEqual(len(selected.controllers), 2)
        domain, = selected.power_stage_domains
        self.assertEqual(domain.id, "current")
        self.assertEqual(domain.members, ("left", "right"))
        with self.assertRaises(FrozenInstanceError):
            domain.members = ()
        for item in selected.controllers:
            self.assertIs(item.integration.power_stage_domain, domain)
            self.assertEqual(item.options.current_count, 2)
            self.assertFalse(item.options.sample_bus)
            self.assertIsNone(item.options.encoder_filter)
        claims = selected.resource_plan.claims
        for key in ("controller:ADC1", "controller:ADC2", "controller:ADC3",
                    "irq:ADC"):
            owners = {item.owner for item in claims if item.key == key}
            self.assertEqual(owners, {"power-stage-domain:current"}, key)
        self.assertNotIn("controller:TIM4", {item.key for item in claims})

    def test_converter_count_is_declared_by_domain_facts(self):
        path = ROOT / "soc/stm32/stm32f4/drivers/pwm_adc/binding.json"
        facts = copy.deepcopy(json.loads(path.read_text())["domains"]["coupled_io"])
        row = next(row for row in facts["routes"] if "power_stage" in row)
        row["adc"] = "ADC7"
        power = row["power_stage"]
        power["capture"]["adc_index"] = 6
        power["domain"]["converters"] = ["ADC" + str(i) for i in range(1, 8)]
        power["domain"]["resources"] = [
            "controller:" + name for name in power["domain"]["converters"]]
        selected = CoupledIoPolicy.from_validated(facts,
            pinmux=NumericPinMuxPolicy.from_validated({
                "encoding": "numeric", "selectors": list(range(16))}))
        self.assertEqual(selected.routes[row["id"]]["adc"], "ADC7")

    def test_single_axis_has_no_second_axis_or_angle_dependency(self):
        selected = resolve_ir(SINGLE)
        domain, = selected.power_stage_domains
        self.assertEqual(domain.members, ("drive",))
        item, = selected.controllers
        self.assertEqual(item.options.current_count, 2)
        _, source = bindings.emit(selected)
        self.assertNotIn("s_nx_port_right", source)
        self.assertNotIn("TIM4", source)
        self.assertIn("s_nx_domain_members_drive[1]", source)

    def test_unreviewed_center_alignment_is_rejected_before_generation(self):
        with self.assertRaisesRegex(ConfigurationError, "alignment"):
            self.changed("exclusive = true", "exclusive = true\ncenter_aligned = true")

    def test_center_route_owns_its_update_vector_and_peer_controller(self):
        selected = resolve_ir(CENTER)
        item, = selected.controllers
        self.assertTrue(item.options.center_aligned)
        keys = {claim.key for claim in selected.resource_plan.claims}
        self.assertIn("irq:TIM8_UP_TIM13", keys)
        self.assertIn("controller:TIM13", keys)
        _, source = bindings.emit(selected)
        self.assertEqual(source.count("void TIM8_UP_TIM13_IRQHandler(void) {"), 1)
        self.assertIn("nx_stm32_power_stage_update_irq", source)
        self.assertIn(".center_aligned = true", source)

    def test_center_capture_stays_in_the_down_slope_quiet_window(self):
        for before, after, reason in (
                ("period_ticks = 1000", "period_ticks = 1001", "center carrier"),
                ("trigger_ticks = 700", "trigger_ticks = 500", "center update"),
                ("trigger_ticks = 700", "trigger_ticks = 900", "center sampling")):
            with self.subTest(after=after), self.assertRaisesRegex(
                    ConfigurationError, reason):
                self.changed(before, after, assembly=CENTER)

    def test_simultaneous_pair_is_one_selected_capture_profile(self):
        selected = resolve_ir(PAIR)
        item, = selected.controllers
        self.assertEqual(item.options.pair.converters, ("ADC1", "ADC2"))
        self.assertEqual(item.options.scan_span_us, 1)
        with self.assertRaises(FrozenInstanceError):
            item.options.pair.converters = ()
        _, source = bindings.emit(selected)
        self.assertIn("nx_stm32_adc_pair_config_t s_nx_pair_drive", source)
        self.assertIn(".pair = &s_nx_pair_drive", source)

    def test_pair_feature_is_selected_from_the_same_capture_record(self):
        for path, enabled in ((PAIR, True), (SINGLE, False)):
            selected = resolve_ir(path)
            with self.subTest(path=path), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                emission.emit_bundle(selected, output)
                self.assertIn("#define NEXUS_STM32_POWER_STAGE_PAIR " +
                              str(int(enabled)), (output / "nexus_config.h").read_text())
                self.assertIn("set(NEXUS_FEATURE_POWER_STAGE_PAIR " +
                              ("ON" if enabled else "OFF") + ")",
                              (output / "selection.cmake").read_text())

    def test_simultaneous_pair_requires_matching_apertures(self):
        with self.assertRaisesRegex(ConfigurationError, "simultaneous sampling"):
            self.changed("sample_times = [0, 0]", "sample_times = [0, 1]",
                         assembly=PAIR)

    def test_pair_and_independent_adc_axis_cannot_share_one_domain(self):
        with self.assertRaisesRegex(ConfigurationError, "paired capture.*exclusive"):
            self.changed('binding = "left"', 'binding = "pair"')

    def test_time_shared_axes_reference_one_immutable_timing_contract(self):
        selected = resolve_ir(TDM)
        domain, = selected.power_stage_domains
        self.assertEqual(domain.timing.start_tolerance_us, 3)
        self.assertEqual(tuple(item.options.slot.phase_offset_ticks
                               for item in selected.controllers), (0, 500))
        for item in selected.controllers:
            self.assertIs(item.options.slot.timing, domain.timing)
        with self.assertRaises(FrozenInstanceError):
            domain.timing.start_tolerance_us = 0
        _, source = bindings.emit(selected)
        self.assertIn(".time_shared = true", source)
        self.assertIn(".start_tolerance_us = 3", source)
        self.assertIn(".phase_offset_ticks = 500", source)
        self.assertIn("void TIM1_UP_TIM10_IRQHandler(void) {", source)
        self.assertEqual(source.count("void ADC_IRQHandler(void) {"), 1)

    def test_time_shared_feature_uses_the_selected_slot_record(self):
        for path, enabled in ((TDM, True), (SINGLE, False)):
            selected = resolve_ir(path)
            with self.subTest(path=path), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                emission.emit_bundle(selected, output)
                self.assertIn("#define NEXUS_STM32_POWER_STAGE_TDM " +
                              str(int(enabled)), (output / "nexus_config.h").read_text())

    def test_time_shared_phase_slots_and_tolerances_are_explicit(self):
        for before, after, reason in (
                ("phase_offset_ticks = 500", "phase_offset_ticks = 0", "half-carrier"),
                ("start_tolerance_us = 3", "start_tolerance_us = 4", "tolerance"),
                ("center_aligned = true", "center_aligned = false", "alignment")):
            with self.subTest(after=after), self.assertRaisesRegex(
                    ConfigurationError, reason):
                self.changed(before, after, assembly=TDM, right_only=True)

    def test_time_shared_origin_is_the_first_selected_member(self):
        with self.assertRaisesRegex(ConfigurationError, "first.*zero"):
            self.changed("phase_offset_ticks = 500", "phase_offset_ticks = SWAP",
                assembly=TDM, replacements=(
                    ("phase_offset_ticks = 0", "phase_offset_ticks = 500"),
                    ("phase_offset_ticks = SWAP", "phase_offset_ticks = 0")))

    def test_time_shared_capture_to_rearm_intervals_cannot_overlap(self):
        with self.assertRaisesRegex(ConfigurationError, "time slots.*overlap"):
            self.changed("compute_budget_us = 100", "compute_budget_us = 260",
                         assembly=TDM, replacements=(("start_tolerance_us = 3",
                                                      "start_tolerance_us = 10"),))

    def test_time_slots_include_core_to_carrier_quantization_margin(self):
        with self.assertRaisesRegex(ConfigurationError, "time slots.*overlap"):
            self.changed("compute_budget_us = 100", "compute_budget_us = 262",
                assembly=TDM, replacements=(("start_tolerance_us = 3",
                                             "start_tolerance_us = 4"),))

    def test_independent_axes_reject_time_slot_options(self):
        with self.assertRaisesRegex(ConfigurationError, "time-shared"):
            self.changed("exclusive = true", "exclusive = true\nphase_offset_ticks = 0",
                         assembly=SINGLE)

    def test_dc_link_has_selected_state_without_fictitious_phase_measurements(self):
        selected = resolve_ir(DC)
        item, = selected.controllers
        self.assertEqual(item.options.current_count, 0)
        self.assertEqual(item.options.current_phase_mask, 0)
        self.assertEqual(item.options.channels, (0,))
        self.assertFalse(item.options.sample_bus)
        self.assertIsNone(item.options.pair)
        self.assertIsNone(item.options.slot)
        self.assertEqual(item.options.dc.rearm_guard_ticks, 4)
        self.assertEqual(item.options.dc.maximum_lateness_ticks, 121)
        self.assertEqual(item.options.dc.aperture_ticks, 1)
        self.assertEqual(item.options.dc.settling_ticks, 26)
        _, source = bindings.emit(selected)
        self.assertIn("nx_stm32_power_stage_dc_t s_nx_dc_state_drive", source)
        self.assertIn("nx_stm32_power_stage_dc_config_t s_nx_dc_drive", source)
        self.assertIn(".dc = &s_nx_dc_drive", source)
        self.assertIn(".immediate_trigger = true", source)
        self.assertIn("void TIM8_UP_TIM13_IRQHandler(void) {", source)

    def test_dc_capture_is_absent_from_unselected_instances(self):
        selected = resolve_ir(SINGLE)
        item, = selected.controllers
        self.assertIsNone(item.options.dc)
        _, source = bindings.emit(selected)
        self.assertNotIn("nx_stm32_power_stage_dc_t s_nx_", source)
        self.assertIn(".immediate_trigger = false", source)

    def test_dc_feature_is_selected_from_its_same_immutable_record(self):
        for path, enabled in ((DC, True), (SINGLE, False)):
            selected = resolve_ir(path)
            with self.subTest(path=path), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                emission.emit_bundle(selected, output)
                self.assertIn("#define NEXUS_STM32_POWER_STAGE_DC " +
                              str(int(enabled)), (output / "nexus_config.h").read_text())

    def test_dc_link_rejects_an_unreviewed_sampling_shape(self):
        with self.assertRaisesRegex(ConfigurationError, "alignment"):
            self.changed("exclusive = true", "exclusive = true\ncenter_aligned = true",
                         assembly=DC)

    def test_domain_wiring_has_one_owner_and_two_strong_faces(self):
        selected = resolve_ir(ASSEMBLY)
        _, source = bindings.emit(selected)
        header, factory_source = factory.emit(selected)
        self.assertEqual(source.count("nx_stm32_power_stage_domain_prepare("), 1)
        self.assertEqual(source.count("nx_stm32_power_stage_domain_close("), 1)
        self.assertEqual(source.count("void ADC_IRQHandler(void) {"), 1)
        self.assertIn("nx_stm32_power_stage_domain_adc_irq", source)
        self.assertIn("nx_factory_power_stage", header)
        self.assertIn("NX_POWER_STAGE_ID_LEFT", header)
        self.assertIn("NX_POWER_STAGE_ID_RIGHT", header)
        self.assertIn("nx_power_stage_t", factory_source)
        self.assertNotIn("RCC_APB1ENR_TIM4EN", source)

    def test_cold_preflight_includes_domain_irq_and_clock_resources(self):
        selected = resolve_ir(ASSEMBLY)
        _, source = bindings.emit(selected)
        self.assertIn("RCC_APB2ENR_ADC1EN", source)
        self.assertIn("RCC_APB2ENR_ADC2EN", source)
        self.assertIn("RCC_APB2ENR_ADC3EN", source)
        for item in selected.controllers:
            self.assertIn("irq:ADC", {claim.key for claim in item.integration.resources})

    def test_domain_members_require_the_same_carrier(self):
        with self.assertRaisesRegex(ConfigurationError, "domain.*carrier"):
            self.changed("period_ticks = 1000", "period_ticks = 2000",
                         right_only=True)

    def test_domain_members_require_one_irq_priority(self):
        with self.assertRaisesRegex(ConfigurationError, "domain.*IRQ"):
            self.changed('irq = { priority = 5, calls_os = false }\n',
                         'irq = { priority = 6, calls_os = false }\n',
                         right_only=True)

    def test_two_domains_cannot_claim_the_same_physical_resource(self):
        with self.assertRaisesRegex(ConfigurationError, "Resource conflict"):
            self.changed('domain = "current"', 'domain = "other"',
                         right_only=True)

    def test_shared_irq_serial_compute_must_fit_each_commit_window(self):
        with self.assertRaisesRegex(ConfigurationError, "domain.*budget"):
            self.changed("compute_budget_us = 100", "compute_budget_us = 250")


if __name__ == "__main__":
    unittest.main()
