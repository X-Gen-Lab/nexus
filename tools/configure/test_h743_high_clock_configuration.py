"""Selected H743 high clocks are immutable distinct facts and cold owners.

Python cases verify resolver and emitted contracts only. Genuine provider and
whole generated Platform GoogleTests remain required for lifecycle qualification.
"""
from dataclasses import FrozenInstanceError
import copy
from pathlib import Path
import re
import tempfile
import unittest

from tools.configure import bindings, clocks, configure as gate, ir

PROFILE = "hsi64-pll384-cache-off"
EXPECTED = {
    "cpu": ("pll1p", 384000000, 364560000, 397800000),
    "hclk": ("cpu", 192000000, 182280000, 198900000),
    **{f"apb{i}": ("hclk", 96000000, 91140000, 99450000)
       for i in range(1, 5)},
    "tim2": ("hclk", 192000000, 182280000, 198900000),
    "tim3": ("hclk", 192000000, 182280000, 198900000),
    "usart3": ("hsi", 64000000, 62000000, 65000000),
    "i2c1": ("hsi", 64000000, 62000000, 65000000),
    "spi4": ("apb2", 96000000, 91140000, 99450000),
    # The joint mux selects the input only. ADC CCR and active16MHz belong to
    # an actually selected/initialized ADC provider, never the empty Platform.
    "adc12_input": ("per_ck", 64000000, 62000000, 65000000),
}


class H743HighClockConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.fixture = gate.ROOT / "tests/contracts/h743_high_assembly/empty.toml"

    def assembly(self, profile=PROFILE):
        text = self.fixture.read_text().replace(
            '"../../../boards/stm32h743_nucleo_zi2"',
            '"' + str(gate.ROOT / "boards/stm32h743_nucleo_zi2") + '"')
        path = self.directory / "assembly.toml"
        path.write_text(text.replace(PROFILE, profile))
        return path

    def resolved(self, profile=PROFILE):
        return gate.resolve_ir(self.assembly(profile))

    def emitted(self, profile=PROFILE):
        return bindings.emit(self.resolved(profile))[1]

    def test_one_selected_profile_has_a_frozen_typed_domain_plan(self):
        result = self.resolved()
        plan = result.clock_plan
        self.assertIsInstance(plan, ir.ClockPlanIR)
        self.assertEqual(plan.profile, PROFILE)
        self.assertEqual(plan.reviewed_revision_ids, (0x2003,))
        self.assertFalse(plan.physical_qualified)
        self.assertEqual(plan.provenance, "reviewed_source_assumptions")
        with self.assertRaises(FrozenInstanceError):
            plan.profile = "foreign"
        with self.assertRaises(TypeError):
            plan.domains["tim2"] = plan.domains["cpu"]
        with self.assertRaises(FrozenInstanceError):
            plan.domains["tim2"].nominal_hz = 384000000

    def test_cpu_bus_timer_and_local_kernel_sources_are_distinct(self):
        result = self.resolved()
        plan = result.clock_plan
        self.assertIsInstance(plan.input, ir.ClockDomainIR)
        self.assertEqual((plan.input.source, plan.input.nominal_hz,
                          plan.input.minimum_hz, plan.input.maximum_hz),
                         ("hsi", 64000000, 62000000, 65000000))
        self.assertEqual(set(plan.domains), set(EXPECTED))
        for name, expected in EXPECTED.items():
            with self.subTest(domain=name):
                domain = plan.domains[name]
                self.assertIsInstance(domain, ir.ClockDomainIR)
                self.assertEqual((domain.source, domain.nominal_hz,
                                  domain.minimum_hz, domain.maximum_hz), expected)
        self.assertNotIn("adc1", plan.domains)
        self.assertFalse(result["physical_qualified"])

    def test_legacy_projections_have_one_reviewed_nominal_authority(self):
        facts = gate.load(gate.ROOT / "soc/stm32h743/soc.json")
        profile = facts["clock_profiles"][PROFILE]
        self.assertTrue(all(name not in profile for name in
                            ("core_hz", "apb1_hz", "apb2_hz")))
        normalized = clocks.normalize(profile)
        plan = self.resolved().clock_plan
        self.assertEqual(profile["reviewed_revision_ids"], [0x2003])
        self.assertEqual(normalized["core_hz"], plan.domains["cpu"].nominal_hz)
        for name in ("apb1", "apb2"):
            self.assertEqual(normalized[name + "_hz"],
                             plan.domains[name].nominal_hz)
        for key in ("core_hz", "apb1_hz", "apb2_hz"):
            with self.subTest(duplicate=key):
                competing = copy.deepcopy(profile)
                competing[key] = normalized[key]
                with self.assertRaises(ValueError):
                    clocks.normalize(competing)

    def test_foreign_or_unknown_domain_bounds_fail_before_publication(self):
        original = gate.load(gate.ROOT / "soc/stm32h743/soc.json")
        for domain, field, value in (("tim2", "nominal_hz", 384000000),
                                     ("spi4", "maximum_hz", None),
                                     ("usart3", "minimum_hz", 0),
                                     ("cpu", "source", "hse")):
            with self.subTest(domain=domain, field=field):
                facts = copy.deepcopy(original)
                facts["clock_profiles"][PROFILE]["domains"][domain][field] = value
                with self.assertRaises(ValueError):
                    gate.validate_soc(facts, "STM32H743ZIT6")
        # Electrical bounds are reviewed for V only. A known register layout
        # does not disambiguate Y/W=1003 or qualify X=2001 for this profile.
        for revisions in ([0x1003], [0x2001], [0x2003, 0x1003],
                          [0x2003, 0x2001], [0x3000], [], [True]):
            with self.subTest(unreviewed_revisions=revisions):
                facts = copy.deepcopy(original)
                facts["clock_profiles"][PROFILE]["reviewed_revision_ids"] = revisions
                with self.assertRaises(ValueError):
                    gate.validate_soc(facts, "STM32H743ZIT6")

    def test_unreviewed_high_cache_choice_is_not_a_silent_fallback(self):
        with self.assertRaises(ValueError):
            self.resolved("hsi64-pll384-cache-on")

    def test_high_empty_platform_emits_only_its_joint_clock_owner(self):
        source = self.emitted()
        owners = re.findall(r"static nx_stm32h743_pll384_state_t (\w+);", source)
        self.assertEqual(len(owners), 1)
        owner = owners[0]
        self.assertIn('"stm32h743_clock.h"', source)
        acquire = source.index(f"nx_stm32h743_pll384_start(&{owner},")
        system = source.index(f"nx_stm32h743_pll384_system_start(&{owner}, 0u,")
        self.assertLess(acquire, system)
        self.assertNotIn("flash_timing_", source)
        self.assertNotIn("nx_stm32h743_system_start(", source)

    def test_system_failure_unwinds_both_responsibilities_in_order(self):
        source = self.emitted()
        call = re.search(r"nx_stm32h743_start_result_t (\w+) = "
                         r"nx_stm32h743_pll384_system_start\(", source)
        self.assertIsNotNone(call)
        variable = call.group(1)
        branch = re.search(rf"if \({variable}\.primary != NX_SUCCESS\) \{{"
                           r"(.*?)\n    \}", source[call.end():], re.S)
        self.assertIsNotNone(branch)
        body = branch.group(1)
        self.assertIn(f"result.primary = {variable}.primary;", body)
        self.assertIn("nx_platform_stop_impl(diagnostic, true)", body)
        self.assertIn("return result;", body)

    def test_system_withdrawal_is_proved_before_lowering_high_clocks(self):
        source = self.emitted()
        system = re.search(r"nx_result_t (\w+) = nx_stm32h743_system_stop\(", source)
        self.assertIsNotNone(system)
        clock = source.index("nx_stm32h743_pll384_stop(")
        self.assertLess(system.start(), clock)
        between = source[system.start():clock]
        variable = system.group(1)
        self.assertIn(f"if ({variable} != NX_SUCCESS) {{ return {variable}; }}", between)

    def test_fixed_hsi_profiles_keep_existing_call_shape_and_zero_clock_owner(self):
        for mhz in (32, 64):
            for cache in ("off", "on"):
                name = f"hsi{mhz}-fixed-cache-{cache}"
                with self.subTest(profile=name):
                    result = self.resolved(name)
                    self.assertIsNone(getattr(result, "clock_plan", None))
                    self.assertEqual(dict(result["clock"]), {
                        "source":"hsi", "input_hz":64000000,
                        "core_hz":mhz*1000000, "apb1_hz":mhz*1000000,
                        "apb2_hz":mhz*1000000})
                    source = bindings.emit(result)[1]
                    self.assertNotIn("pll384_", source)
                    self.assertNotIn("flash_timing_", source)
                    self.assertIn("nx_stm32h743_system_start(", source)

    def test_high_selection_reaches_the_real_atomic_configuration_bundle(self):
        output = self.directory / "generated"
        gate.configure(self.assembly(), output)
        header = (output / "nexus_config.h").read_text()
        self.assertIn("#define NEXUS_CORE_HZ 384000000u", header)
        source = (output / "bindings.c").read_text()
        self.assertIn("nx_stm32h743_pll384_system_start(", source)
        self.assertNotIn("flash_timing_", source)


class H743SelectedKernelConfigurationTests(unittest.TestCase):
    """Exercise ordinary TOML consumers of the reviewed local clock plan."""

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def resolved(self, selection):
        prefix = (gate.ROOT /
                  "tests/contracts/h743_high_assembly/empty.toml").read_text()
        prefix = prefix.replace(
            '"../../../boards/stm32h743_nucleo_zi2"',
            '"' + str(gate.ROOT / "boards/stm32h743zi_hsi_reference") + '"')
        path = self.directory / "selected.toml"
        path.write_text(prefix + "\n" + selection)
        return gate.resolve_ir(path)

    def test_high_profile_cannot_drop_its_domains_and_bypass_the_joint_owner(self):
        facts = gate.load(gate.ROOT / "soc/stm32h743/soc.json")
        facts["clock_profiles"][PROFILE] = {
            "source": "hsi", "input_hz": 64000000,
            "core_hz": 384000000, "apb1_hz": 96000000,
            "apb2_hz": 96000000}
        with self.assertRaisesRegex(ValueError, "clock|profile|domain"):
            gate.validate_soc(facts, "STM32H743ZIT6")

    def i2c(self, **changes):
        selection = ('[i2c.bus1]\nbinding = "i2c1"\n'
                     'mode = "poll-messages"\nmax_hz = 100000\n'
                     '[i2c_device.device1]\ncontroller = "bus1"\n'
                     'driver = "i2c-endpoint"\naddress = 80\n')
        if not changes.pop("explicit", True):
            return self.resolved(selection)
        values = dict(kernel_hz_min=62000000, kernel_hz_max=65000000,
                      pclk_hz_min=91140000, bus_hz_min=60000,
                      rise_ns_max=1000, fall_ns_max=300,
                      source="declared test envelope; electrical facts unmeasured")
        values.update(changes)
        selection += "\n[i2c.bus1.timing]\n"
        selection += "".join(
            key + " = " + ('"' + value + '"' if isinstance(value, str)
                            else str(value)) + "\n"
            for key, value in values.items())
        return self.resolved(selection)

    def spi(self, rate):
        return self.resolved(
            '[spi.bus4]\nbinding = "spi4"\nmode = "short-poll"\n'
            f'max_hz = {rate}\n\n[spi_device.device4]\n'
            'controller = "bus4"\ndriver = "spi-endpoint"\n'
            f'cs_binding = "spi4-cs"\nmode = 0\nmax_hz = {rate}\n')

    def test_uart_uses_its_hsi_kernel_instead_of_the_ninety_six_mhz_bus(self):
        result = self.resolved(
            '[uart.console]\nbinding = "uart3"\nmode = "irq-bytes"\n'
            'baud = 115200\n[uart.console.rx]\nformat = "bytes"\n'
            'capacity = 64\n[uart.console.irq]\npriority = 5\ncalls_os = false\n')
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32h743_uart_initialize(&s_nx_port_console, "
                      "64000000u, 1000000u)", source)
        self.assertNotIn("nx_stm32h743_uart_initialize(&s_nx_port_console, "
                         "96000000u", source)

    def test_spi_projects_its_apb_kernel_while_retaining_the_upper_bound(self):
        result = self.spi(1000000)
        source = bindings.emit(result)[1]
        self.assertIn("s_nx_port_bus4.clock_hz = 96000000u;", source)
        domain = result.clock_plan.domains["spi4"]
        self.assertEqual((domain.source, domain.maximum_hz),
                         ("apb2", 99450000))
        self.assertNotIn("s_nx_port_bus4.clock_hz = 384000000u;", source)

    def test_spi_budget_must_be_representable_at_the_reviewed_upper_bound(self):
        # Even the slowest /256 clock is above250kHz for this source envelope.
        with self.assertRaisesRegex(ValueError, "SPI.*(clock|divider|speed)"):
            self.spi(250000)

    def test_high_i2c_cannot_silently_reuse_a_fixed_apb_timing_literal(self):
        with self.assertRaisesRegex(ValueError, "I2C.*(timing|envelope)"):
            self.i2c(explicit=False)

    def test_high_i2c_uses_declared_hsi_and_independent_apb_bounds(self):
        result = self.i2c()
        item = next(item for item in result.controllers if item.kind == "i2c")
        timing = item.options.timing
        self.assertEqual(timing.provenance, "authored_declared_assumptions")
        self.assertFalse(timing.physical_qualified)
        self.assertEqual(timing.cr1_filter_bits, 0)
        self.assertEqual((timing["assumptions"]["kernel_hz_min"],
                          timing["assumptions"]["kernel_hz_max"],
                          timing["assumptions"]["pclk_hz_min"]),
                         (62000000, 65000000, 91140000))
        self.assertIn(f"s_nx_port_bus1.timing = 0x{timing.timingr:08x}u;",
                      bindings.emit(result)[1])

    def test_i2c_assumptions_must_cover_the_whole_reviewed_clock_envelope(self):
        for changes in (dict(kernel_hz_min=63000000, kernel_hz_max=64000000),
                        dict(pclk_hz_min=96000000)):
            with self.subTest(changes=changes), self.assertRaisesRegex(
                    ValueError, "I2C.*(clock|envelope|PCLK|source)"):
                self.i2c(**changes)


if __name__ == "__main__":
    unittest.main()
