"""Cold HSI choices bind one consistent CPU, bus and timer configuration."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate


class H743ClockConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.fixture = gate.ROOT / "tests/contracts/h743_assembly/io.toml"

    def assembly(self, clock="hsi32-fixed-cache-off", backend="baremetal"):
        text = self.fixture.read_text().replace(
            '"../../../boards/stm32h743_nucleo_zi2"',
            '"' + str(gate.ROOT / "boards/stm32h743_nucleo_zi2") + '"')
        text = text.replace('clock = "hsi64-fixed-cache-off"',
                            'clock = "' + clock + '"')
        text = text.replace('backend = "baremetal"',
                            'backend = "' + backend + '"')
        path = self.directory / "assembly.toml"
        path.write_text(text)
        return path

    def test_fixed_internal_input_and_consistent_selected_frequencies(self):
        for hz in (32000000, 64000000):
            with self.subTest(hz=hz):
                result = gate.resolve_ir(self.assembly(
                    f"hsi{hz // 1000000}-fixed-cache-off"))
                self.assertEqual(dict(result["clock"]), {
                    "source": "hsi", "input_hz": 64000000,
                    "core_hz": hz, "apb1_hz": hz, "apb2_hz": hz})
                self.assertFalse(result["physical_qualified"])

    def test_cpu_and_uart_initialize_share_selected_clock_for_both_backends(self):
        for backend in ("baremetal", "freertos"):
            with self.subTest(backend=backend):
                output = self.directory / backend
                gate.configure(self.assembly(backend=backend), output)
                source = (output / "bindings.c").read_text()
                config = (output / "nexus_config.h").read_text()
                self.assertIn("#define NEXUS_CORE_HZ 32000000u", config)
                self.assertIn("#define NEXUS_SOC_CACHE_MASK 0u", config)
                self.assertIn(
                    "nx_stm32h743_uart_initialize(&s_nx_port_console, "
                    "32000000u, 1000000u)", source)
                self.assertIn(
                    "nx_stm32h743_system_start(&g_nx_stm32h743_system, "
                    "32000000u, 0u, 1000000u)", source)
                self.assertNotIn("64000000u", source)

    def test_cache_profile_projects_one_exact_compile_and_runtime_selection(self):
        for hz in (32, 64):
            with self.subTest(hz=hz):
                output = self.directory / f"cache-{hz}"
                gate.configure(self.assembly(
                    f"hsi{hz}-fixed-cache-on"), output)
                config = (output / "nexus_config.h").read_text()
                source = (output / "bindings.c").read_text()
                self.assertIn("#define NEXUS_SOC_CACHE_MASK 3u", config)
                self.assertIn(
                    "nx_stm32h743_system_start(&g_nx_stm32h743_system, "
                    f"{hz * 1000000}u, 3u, 1000000u)", source)

    def test_unreviewed_dividers_and_dynamic_names_fail_closed(self):
        for clock in ("hsi16-fixed-cache-off", "hsi48-fixed-cache-off",
                      "hsi32-dynamic", "hse32"):
            with self.subTest(clock=clock), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(clock))

    def test_machine_facts_cannot_change_clock_without_provider_review(self):
        facts = gate.load(gate.ROOT / "soc/stm32h743/soc.json")
        self.assertIn("hsi32-fixed-cache-off", facts["clock_profiles"])
        facts["clock_profiles"]["hsi32-fixed-cache-off"]["apb1_hz"] = 16000000
        with self.assertRaises(ValueError):
            gate.validate_soc(facts, "STM32H743ZIT6")

    def test_selected_flash_owns_cold_timing_inside_platform_lifecycle(self):
        fixture = gate.ROOT / "tests/contracts/h743_assembly/peripherals.toml"
        original = fixture.read_text().replace(
            '"../../../boards/stm32h743zi_hsi_reference"',
            '"' + str(gate.ROOT / "boards/stm32h743zi_hsi_reference") + '"')
        for frequency in (32, 64):
            with self.subTest(frequency=frequency):
                path = self.directory / "flash.toml"
                path.write_text(original.replace("hsi64-fixed-cache-off",
                    f"hsi{frequency}-fixed-cache-off"))
                _, source = bindings.emit(gate.resolve_ir(path))
                self.assertIn("static nx_stm32h743_flash_timing_state_t", source)
                system = source.index("nx_stm32h743_system_start(")
                timing = source.index("nx_stm32h743_flash_timing_initialize(")
                controller = source.index("result.primary = prepare_")
                self.assertLess(system, timing)
                self.assertLess(timing, controller)
                self.assertIn("nx_platform_stop_impl(diagnostic, true)",
                              source[timing:controller])
                self.assertLess(source.index("nx_stm32h743_flash_timing_stop("),
                                source.index("nx_stm32h743_system_stop("))

    def test_unselected_flash_has_no_timing_owner_or_calls(self):
        _, source = bindings.emit(gate.resolve_ir(self.assembly()))
        self.assertNotIn("flash_timing_", source)

    def test_flash_cache_profile_requires_an_implemented_joint_owner(self):
        fixture = gate.ROOT / "tests/contracts/h743_assembly/peripherals.toml"
        original = fixture.read_text().replace(
            '"../../../boards/stm32h743zi_hsi_reference"',
            '"' + str(gate.ROOT / "boards/stm32h743zi_hsi_reference") + '"')
        path = self.directory / "cache-flash.toml"
        path.write_text(original.replace("cache-off", "cache-on"))
        with self.assertRaisesRegex(ValueError, "Flash.*cache|cache.*Flash"):
            gate.resolve_ir(path)


if __name__ == "__main__":
    unittest.main()
