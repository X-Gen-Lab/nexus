"""Selected PLLSAI is one exact System plan, never an audio RCC owner."""
from dataclasses import FrozenInstanceError, replace
from pathlib import Path
from types import SimpleNamespace
import unittest

from tools.configure.tests.f4_fixture import provider as f4_provider

contract = f4_provider("STM32F446ZET6")
clock = contract.domain_policies.clock


class SaiClockPlanTests(unittest.TestCase):
    @staticmethod
    def profile():
        return dict(source="hsi", input_hz=16000000, pll_m=16, pll_n=336,
                    pll_p=4, pll_q=7, ahb_div=1, apb1_div=4, apb2_div=1,
                    adc_div=4, supply_mv=3300, css=False,
                    sysclk_source="input", pll48=False,
                    pll_sai=dict(m=10, n=192, sai1=dict(q=5, post_divider=5)))

    def test_dedicated_auxiliary_input_does_not_enable_the_main_pll(self):
        result = clock.plan("custom", self.profile())
        self.assertEqual(dict(m=10, n=192, sai1=dict(q=5, post_divider=5,
                                               kernel_hz=12288000), ltdc=None), result["pll_sai"])
        self.assertFalse(result["parameters"]["pll_enabled"])
        self.assertEqual(16, result["parameters"]["pll_m"])
        self.assertEqual(16000000, result["domains"]["core"]["nominal_hz"])
        self.assertEqual(0, result["domains"]["pll48"]["nominal_hz"])
        self.assertNotIn("pll_sai", result["parameters"])
        self.assertNotIn("sai1", result["domains"])

    def test_common_input_tuple_is_exact_without_rounding_the_input_first(self):
        profile = self.profile()
        profile.update(pll_m=10, pll_n=192, pll_q=5)
        result = clock.plan("custom", profile)
        self.assertEqual(12288000, result["pll_sai"]["sai1"]["kernel_hz"])
        self.assertEqual(10, result["parameters"]["pll_m"])
        # 1.6 MHz is a rational PLL input, not integer-MHz 1 MHz.
        self.assertEqual(16000000, result["domains"]["core"]["nominal_hz"])

    def test_unselected_auxiliary_plan_has_no_retained_tuple(self):
        profile = self.profile()
        del profile["pll_sai"]
        result = clock.plan("custom", profile)
        self.assertIsNone(result["pll_sai"])
        self.assertNotIn("pll_sai", result["parameters"])

    def test_leaf_plan_does_not_grant_another_devices_reviewed_cuts(self):
        profile = self.profile()
        del profile["pll_sai"]
        result = clock.plan("custom", profile)
        self.assertEqual((), result["reviewed_revision_ids"])
        self.assertEqual(
            "Selected explicit PLL M/N/P/Q clock-domain facts; physical accuracy unmeasured", result["provenance"])
        self.assertFalse(result["physical_qualified"])

    def test_fractional_kernel_is_rejected_instead_of_truncated(self):
        profile = self.profile()
        profile["pll_sai"]["sai1"]["q"] = 7
        with self.assertRaisesRegex(ValueError, "exact integer Hz.*auxiliary"):
            clock.plan("custom", profile)

    def test_auxiliary_tuple_rejects_unknown_fields_booleans_and_null(self):
        for value in (None, True, [], {},
                      dict(m=10, n=192, sai1=dict(q=5, post_divider=5), kernel_hz=1),
                      dict(m=10, n=True, sai1=dict(q=5, post_divider=5)),
                      dict(m=10, n=192, sai1=dict(q=5, post_divider=True)),
                      dict(m=10, n=192),
                      dict(m=10, n=192, sai1=None)):
            with self.subTest(value=value), self.assertRaisesRegex(
                    ValueError, "(?i)(auxiliary|PLL)"):
                profile = self.profile()
                profile["pll_sai"] = value
                clock.plan("custom", profile)

    def test_auxiliary_plan_requires_its_selected_sai_master_consumer(self):
        selected = clock.plan("custom", self.profile())
        for controllers in ((), (SimpleNamespace(kind="crc", controller="CRC",
                                                  mode="crc32-words"),),
                            (SimpleNamespace(kind="audio", controller="SAI1",
                                             mode="slave-tx"),)):
            with self.subTest(controllers=controllers), self.assertRaisesRegex(
                    ValueError, "Auxiliary Q.*audio master"):
                clock.validate_consumers(selected, controllers)

    def test_selected_sai_master_cannot_claim_an_undeclared_auxiliary_source(self):
        profile = self.profile()
        del profile["pll_sai"]
        selected = clock.plan("custom", profile)
        controller = SimpleNamespace(kind="audio", controller="SAI1",
                                     mode="master-duplex")
        with self.assertRaisesRegex(ValueError, "Audio master.*auxiliary Q"):
            clock.validate_consumers(selected, (controller,))

    def test_selected_sai_master_consumes_the_one_system_auxiliary_plan(self):
        controller = SimpleNamespace(kind="audio", controller="SAI1",
                                     mode="master-duplex")
        selected = clock.plan("custom", self.profile())
        clock.validate_consumers(selected, (controller,))

    def test_sai2_master_consumes_the_same_q_tuple_without_another_pll(self):
        selected = clock.plan("custom", self.profile())
        controller = SimpleNamespace(kind="audio", controller="SAI2",
                                     mode="master-duplex")
        clock.validate_consumers(selected, (controller,))

    def test_generated_sai2_mux_ownership_follows_the_actual_master(self):
        from tools.configure import configure
        from tools.configure.ir import ClockPlanIR
        assembly = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/sky-baremetal-gpio.toml")
        selected = ClockPlanIR.from_validated(clock.plan("custom", self.profile()))
        for mode, enabled in (("master-tx", 1), ("slave-tx", 0)):
            controller = SimpleNamespace(kind="audio", controller="SAI2",
                                         mode=mode)
            lines = contract.configuration_header(replace(
                assembly, clock_plan=selected, controllers=(controller,)))
            self.assertIn(f"#define NEXUS_CLOCK_PLLSAI_SAI2_ENABLED {enabled}u",
                          lines)

    def test_ltdc_only_retains_no_q_output_and_has_an_exact_pixel_clock(self):
        profile = self.profile()
        profile["pll_sai"] = dict(m=10, n=192,
                                  ltdc=dict(r=4, post_divider=16))
        selected = clock.plan("custom", profile)
        self.assertEqual(dict(m=10, n=192, sai1=None,
                              ltdc=dict(r=4, post_divider=16,
                                        pixel_hz=4800000)), selected["pll_sai"])
        clock.validate_consumers(selected, (SimpleNamespace(
            kind="display", controller="LTDC", mode="rgb-scanout"),))

    def test_two_outputs_share_one_pll_input_multiplier_and_keep_own_dividers(self):
        profile = self.profile()
        profile["pll_sai"]["ltdc"] = dict(r=4, post_divider=16)
        selected = clock.plan("custom", profile)
        self.assertEqual(12288000, selected["pll_sai"]["sai1"]["kernel_hz"])
        self.assertEqual(4800000, selected["pll_sai"]["ltdc"]["pixel_hz"])
        clock.validate_consumers(selected, (
            SimpleNamespace(kind="audio", controller="SAI1", mode="master-tx"),
            SimpleNamespace(kind="display", controller="LTDC", mode="rgb-scanout")))
        with self.assertRaisesRegex(ValueError, "Auxiliary R.*display consumer"):
            clock.validate_consumers(selected, (SimpleNamespace(
                kind="audio", controller="SAI1", mode="master-tx"),))

    def test_another_category_or_mode_cannot_consume_ltdc_clock_authority(self):
        profile = self.profile()
        profile["pll_sai"] = dict(m=10, n=192, ltdc=dict(r=4, post_divider=16))
        selected = clock.plan("custom", profile)
        for kind, mode in (("image", "irq-blit"), ("display", "guessed")):
            with self.subTest(kind=kind, mode=mode), self.assertRaisesRegex(
                    ValueError, "Auxiliary R.*display consumer"):
                clock.validate_consumers(selected, (SimpleNamespace(
                    kind=kind, controller="LTDC", mode=mode),))

    def test_production_typed_auxiliary_plan_is_frozen_and_emits_both_outputs(self):
        from tools.configure import configure
        from tools.configure.ir import ClockPlanIR
        profile = self.profile()
        profile["pll_sai"]["ltdc"] = dict(r=4, post_divider=16)
        record = clock.plan("custom", profile)
        selected = ClockPlanIR.from_validated(record)
        self.assertTrue(hasattr(selected, "pll_sai"), "typed PLLSAI is absent")
        self.assertEqual(12288000, selected.pll_sai.sai1.kernel_hz)
        self.assertEqual(4800000, selected.pll_sai.ltdc.pixel_hz)
        with self.assertRaises(FrozenInstanceError):
            selected.pll_sai.m = 11
        with self.assertRaises(FrozenInstanceError):
            selected.pll_sai.sai1.q = 6
        with self.assertRaises(FrozenInstanceError):
            selected.pll_sai.ltdc.r = 5
        self.assertNotIn("pll_sai", selected.parameters)
        # Actual immutable result shell, replacing only the pure emitter input;
        # this does not resolve or qualify this profile for the shell's Board.
        assembly = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/sky-baremetal-gpio.toml")
        lines = contract.configuration_header(replace(assembly, clock_plan=selected))
        for suffix, value in (("ENABLED", 1), ("SAI1_ENABLED", 1),
                ("LTDC_ENABLED", 1), ("M", 10), ("N", 192), ("Q", 5),
                ("DIVQ", 5), ("SAI1_HZ", 12288000), ("R", 4),
                ("DIVR", 16), ("LTDC_HZ", 4800000)):
            self.assertIn(f"#define NEXUS_CLOCK_PLLSAI_{suffix} {value}u", lines)

    def test_production_typed_plan_emits_no_undeclared_output_parameters(self):
        from tools.configure import configure
        from tools.configure.ir import ClockPlanIR
        assembly = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/sky-baremetal-gpio.toml")
        for output in ("sai1", "ltdc", None):
            with self.subTest(output=output):
                profile = self.profile()
                if output == "ltdc":
                    profile["pll_sai"] = dict(m=10, n=192,
                        ltdc=dict(r=4, post_divider=16))
                elif output is None:
                    del profile["pll_sai"]
                selected = ClockPlanIR.from_validated(clock.plan("custom", profile))
                self.assertTrue(hasattr(selected, "pll_sai"), "typed PLLSAI is absent")
                text = "\n".join(contract.configuration_header(replace(assembly,
                                                          clock_plan=selected)))
                self.assertEqual(output == "sai1",
                                 "#define NEXUS_CLOCK_PLLSAI_Q " in text)
                self.assertEqual(output == "ltdc",
                                 "#define NEXUS_CLOCK_PLLSAI_R " in text)
                self.assertEqual(output is not None,
                                 "#define NEXUS_CLOCK_PLLSAI_M " in text)

    def test_ltdc_divider_and_exact_pixel_frequency_are_validated(self):
        for output in (dict(r=1, post_divider=16),
                       dict(r=8, post_divider=16),
                       dict(r=True, post_divider=16),
                       dict(r=4, post_divider=3),
                       dict(r=4, post_divider=True),
                       dict(r=7, post_divider=16),
                       dict(r=4, post_divider=16, pixel_hz=1)):
            with self.subTest(output=output), self.assertRaisesRegex(
                    ValueError, "(?i)(auxiliary|PLL)"):
                profile = self.profile()
                profile["pll_sai"] = dict(m=10, n=192, ltdc=output)
                clock.plan("custom", profile)

    def test_auxiliary_electrical_and_encoded_bounds_are_checked_exactly(self):
        for fields in (dict(m=1), dict(m=64), dict(m=17), dict(m=7),
                       dict(n=49), dict(n=433), dict(n=50), dict(n=300),
                       dict(sai1=dict(q=1, post_divider=5)),
                       dict(sai1=dict(q=16, post_divider=5)),
                       dict(sai1=dict(q=5, post_divider=0)),
                       dict(sai1=dict(q=5, post_divider=33))):
            with self.subTest(fields=fields), self.assertRaisesRegex(
                    ValueError, "(?i)(auxiliary|PLL)"):
                profile = self.profile()
                profile["pll_sai"].update(fields)
                clock.plan("custom", profile)


if __name__ == "__main__":
    unittest.main(verbosity=2)
