"""Named ADC layouts describe expectations; cold code observes silicon.

These whole-resolver cases require separate actual generated Platform and
private provider GoogleTests. A source-record digest is not HIL qualification.
"""
from dataclasses import FrozenInstanceError
import hashlib
from pathlib import Path
import re
import tempfile
import tomllib
import unittest

from tools.configure.tests.assembly_fixture import render_toml
from tools.configure import bindings, configure, factory, selected_contracts


class G474ADCGroupConfiguration(unittest.TestCase):
    namespace = "cmake/package/verification/io_evolution"

    def fixture(self, name="regular-software"):
        return (configure.ROOT / self.namespace /
                ("g474-adc-group-" + name + ".toml"))

    def resolve(self, name="regular-software", before=None, after=None,
                extra="", replacements=()):
        fixture = self.fixture(name)
        contents = fixture.read_text()
        package = tomllib.loads(contents)["board_package"]
        contents = contents.replace(
            'board_package = "' + package + '"',
            'board_package = "' + str((fixture.parent / package).resolve())
            + '"')
        if before is not None:
            self.assertIn(before, contents)
            contents = contents.replace(before, after)
        for before, after in replacements:
            self.assertIn(before, contents)
            contents = contents.replace(before, after)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "assembly.toml"
            path.write_text(contents + extra)
            return configure.resolve_ir(path)

    def test_four_inputs_select_one_real_adc1_and_one_nominal_face(self):
        for group in ("regular", "injected"):
            for trigger in ("software", "timer"):
                with self.subTest(group=group, trigger=trigger):
                    result = self.resolve(group + "-" + trigger)
                    item, = result.controllers
                    self.assertEqual((item.kind, item.controller),
                                     ("adc_group", "ADC1"))
                    records = selected_contracts.for_configuration(result)
                    self.assertEqual({row.public_class for row in records},
                                     {"adc_group"})
                    self.assertFalse(result.physical_qualified)

    def test_authored_adc_group_binds_the_validated_route_review(self):
        fixture = self.fixture()
        assembly = {
            "schema_version": 1,
            "board_package": str(fixture.parent / "boards/g474-adc-group"),
            "backend": "baremetal", "clock_profile": "hse24-pll170",
            "controllers": [{
                "id": "samples", "binding": "samples", "authored_kind": "adc_group",
                "mode": "regular-sequence-dma", "channels": [2, 1],
                "sample_times": [0, 0], "reference_mv": 3300,
                "exclusive": True, "silicon_profile": "g474-adc-group-x",
                "trigger": "software", "irq_priority": 5, "calls_os": False}],
            "devices": [], "memory_budgets": {"main_stack_bytes": 2048},
            "layout": None}
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "assembly.toml"
            path.write_text(render_toml(assembly))
            result = configure.resolve_ir(path, configure.ROOT)
        item, = result.controllers
        self.assertEqual((item.kind, item.controller, item.options.channels),
                         ("adc_group", "ADC1", (2, 1)))
        policy = selected_contracts.silicon_policy(result, item)
        review, = (entry for entry in result.inputs
                   if entry["sha256"] == policy.review_sha256)
        review_bytes = (configure.ROOT / review["path"]).read_bytes()
        self.assertEqual(hashlib.sha256(review_bytes).hexdigest(),
                         policy.review_sha256)

    def test_named_cuts_freeze_layout_and_required_algorithms(self):
        for cut, revision, repeat, required, applied in (
                ("x", 0x2003, 1, 2, 6), ("y", 0x2002, 1, 6, 6),
                ("z", 0x2001, 2, 3, 7)):
            with self.subTest(expected_cut=cut):
                result = self.resolve(
                    before='silicon_profile = "g474-adc-group-x"',
                    after='silicon_profile = "g474-adc-group-' + cut + '"')
                item, = result.controllers
                options = item.options
                self.assertEqual(options.rank_repeat, repeat)
                self.assertEqual(options.sample_stride, 2 * repeat)
                self.assertEqual(options.scan_stride, 4 * repeat)
                self.assertEqual(options.max_frames, 65535 // (2 * repeat))
                policy = selected_contracts.silicon_policy(result, item)
                self.assertEqual((policy.device_id, policy.flash_bytes),
                                 (0x469, 524288))
                rule, = policy.rules
                self.assertEqual(rule.revision_id, revision)
                self.assertEqual(rule.required_workarounds, required)
                self.assertEqual(policy.applied_workarounds, applied)
                self.assertEqual(policy.unknown_revision, "reject")
                self.assertFalse(policy.physical_qualified)
                self.assertRegex(policy.review_sha256, r"^[0-9a-f]{64}$")
                self.assertNotEqual(policy.review_sha256, "0" * 64)
                with self.assertRaises(FrozenInstanceError):
                    policy.applied_workarounds = 0xffffffff
                with self.assertRaises(FrozenInstanceError):
                    options.rank_repeat = 1

    def test_selected_profile_cannot_inherit_a_family_or_foreign_cut(self):
        for name in ("g474-adc-group-v", "h743-high-v", "g474-adc-group-X",
                     "g474-adc-group-unknown", ""):
            with self.subTest(profile=name), self.assertRaises(ValueError):
                self.resolve(before='silicon_profile = "g474-adc-group-x"',
                             after='silicon_profile = "' + name + '"')

    def test_authored_observation_workarounds_and_costs_are_forbidden(self):
        for field in ("observed_revision=8195", "applied_workarounds=7",
                      "required_workarounds=0", "rank_repeat=2",
                      "sample_stride=2", 'review_sha256="' + "a" * 64 + '"'):
            with self.subTest(field=field), self.assertRaisesRegex(
                    ValueError, "observed|workaround|repeat|stride|review"):
                self.resolve(before='trigger = "software"',
                             after='trigger = "software"\n' + field)

    def test_regular_and_injected_claim_only_their_real_dma_resources(self):
        for group in ("regular", "injected"):
            with self.subTest(group=group):
                result = self.resolve(group + "-software")
                keys = {claim.key for claim in result.claims}
                self.assertLessEqual({"controller:ADC1", "controller:ADC2",
                                      "controller:ADC345", "irq:ADC1_2"}, keys)
                has_dma = group == "regular"
                self.assertEqual("dma:DMA1:stream1" in keys, has_dma)
                self.assertEqual("irq:DMA1_Channel1" in keys, has_dma)
                self.assertEqual(len(result.dma_topology.lanes),
                                 1 if has_dma else 0)
                self.assertNotIn("controller:TIM6", keys)
                self.assertEqual(
                    {(edge.source, edge.target) for edge in
                     result.trigger_plan.edges},
                    {("samples:adc", "samples:dma")} if has_dma else set())

    def test_software_and_timer_have_distinct_owned_trigger_contracts(self):
        for group in ("regular", "injected"):
            with self.subTest(group=group):
                result = self.resolve(group + "-timer")
                keys = {claim.key for claim in result.claims}
                self.assertIn("controller:TIM6", keys)
                self.assertNotIn("irq:TIM6_DAC", keys)
                self.assertTrue(result.trigger_plan.edges)
                item, = result.controllers
                self.assertEqual(item.options.trigger, "tim6-trgo")
                self.assertEqual(item.options.period_us, 50)
                self.assertGreater(item.options.trigger_identity, 0)

    def test_clock_and_payload_cost_are_derived_from_the_selected_profile(self):
        result = self.resolve()
        item, = result.controllers
        self.assertEqual((item.options.hclk_hz, item.options.adc_clock_hz),
                         (170000000, 21250000))
        self.assertEqual(item.options.channels, (2, 1))
        self.assertEqual(item.options.sample_times, (0, 0))
        injected = self.resolve("injected-software").controllers[0]
        self.assertEqual(injected.options.max_frames, 1)

    def test_unimplemented_trigger_or_scheduling_arguments_reject(self):
        for before, after in (
                ('trigger = "software"', 'trigger = "timer"'),
                ('trigger = "software"', 'trigger = "tim1-trgo2"'),
                ('trigger = "software"', 'trigger = "software"\nperiod_us=1'),
                ('trigger = "software"', 'trigger = "software"\ntimer="TIM6"'),
                ('exclusive = true', 'exclusive = false')):
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.resolve(before=before, after=after)

    def test_timer_period_covers_scan_and_prime_freshness(self):
        self.resolve("regular-timer")
        for period, samples in ((1000, "[0, 0]"), (950, "[7, 7]")):
            with self.subTest(period=period, samples=samples):
                with self.assertRaisesRegex(ValueError, "period|prim|fresh"):
                    self.resolve(
                        "regular-timer", "period_us = 50",
                        "period_us = " + str(period), replacements=(
                            ("sample_times = [0, 0]",
                             "sample_times = " + samples),))

    def test_factory_keeps_only_the_group_face_without_constructor(self):
        result = self.resolve()
        header, getters = factory.emit(result)
        self.assertIn("const nx_adc_group_port_t* nx_factory_adc_group(",
                      header)
        self.assertIn("NX_ADC_GROUP_ID_SAMPLES", header)
        self.assertNotIn("NX_ADC_ID_SAMPLES", header)
        self.assertNotIn("initialize(", getters)
        self.assertNotIn("nx_silicon_preflight(", getters)
        source = bindings.emit(result)[1]
        self.assertIn("&nx_stm32g474_adc_group_ops", source)
        self.assertNotIn("&nx_stm32g474_adc_ops", source)

    def test_actual_cold_source_and_policy_are_readonly_and_selected_only(self):
        source = bindings.emit(self.resolve())[1]
        self.assertIn("nx_stm32_identity_read", source)
        self.assertIn("DBGMCU->IDCODE", source)
        self.assertIn("FLASHSIZE_BASE", source)
        self.assertIn("static const nx_silicon_admission_policy_t", source)
        self.assertIn(".allow_unreviewed = false", source)
        self.assertNotRegex(source, r"static\s+nx_silicon_admission_t\b")
        self.assertNotIn("s_nx_silicon_admitted", source)

    def test_strict_comparison_precedes_any_platform_system_effects(self):
        source = bindings.emit(self.resolve())[1]
        start = source.index("static nx_platform_start_result_t "
                             "nx_platform_start_impl(")
        # Locate the definition, past the forward declaration.
        start = source.index("static nx_platform_start_result_t "
                             "nx_platform_start_impl(", start + 1)
        comparison = source.index("nx_silicon_preflight(", start)
        system = source.index("nx_stm32g474_system_start(", start)
        self.assertLess(comparison, system)
        between = source[start:comparison]
        self.assertNotIn("NX_G474_WRITE(", between)
        self.assertNotIn("NX_PLATFORM_REGISTER_WRITE(", between)

    def test_named_review_record_is_an_actual_configuration_input(self):
        result = self.resolve()
        item, = result.controllers
        policy = selected_contracts.silicon_policy(result, item)
        review = [entry for entry in result.inputs
                  if entry["sha256"] == policy.review_sha256]
        self.assertEqual(len(review), 1)
        self.assertTrue(re.fullmatch(
            r"soc/stm32/stm32g474/reviews/[a-z0-9-]+\.json", review[0]["path"]))


if __name__ == "__main__":
    unittest.main()
