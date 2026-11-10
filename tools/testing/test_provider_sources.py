"""Configure actual SoC targets and inspect their selected compilation units."""
from tools.configure.providers.descriptor import source_directory

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ProviderSourceSelectionTests(unittest.TestCase):
    def private_sources(self, family, names):
        """Spell expected SoC-local sources without erasing their ownership."""
        directory = Path(source_directory(family))
        return {(directory / (name if name == "timebase.c" else
                              "drivers/" + name)).as_posix() for name in names} | {
            (directory / "drivers" / name).as_posix() for name in (
                "platform_coordinator.c", "platform_lifecycle.c",
                "system_lifecycle.c", "platform_cold.c",
                "lifecycle_pins.c", "chip.c")} | {
            (directory / name).as_posix() for name in ("reset.c", "supervision.c")}

    def configure_sources(self, family, controllers):
        """Inspect production targets without claiming an ARM link proof."""
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            generated = source / "generated"
            generated.mkdir()
            (generated / "resolved.json").write_text(json.dumps({
                "controllers": [{"kind": item[0], "mode": item[1],
                                 **({'controller': item[2]} if len(item) == 3 else {})}
                                for item in controllers],
            }))
            # Every family consumes its actual locked SDK and real CMake
            # integration. Configuration inspects selection, not ARM behavior.
            sdk_root = ROOT
            (source / "CMakeLists.txt").write_text(f'''\
cmake_minimum_required(VERSION 3.31)
project(provider_source_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(Python3_EXECUTABLE "{sys.executable}")
set(NEXUS_SOURCE_DIR "{sdk_root.as_posix()}")
set(NEXUS_CONFIG_DIR "{generated.as_posix()}")
add_library(nexus_io INTERFACE)
add_library(Nexus::IOGPIO ALIAS nexus_io)
add_library(nexus_arch INTERFACE)
add_library(Nexus::Arch ALIAS nexus_arch)
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
set(NEXUS_STM32_DEVICE STM32F407xx)
set(NEXUS_STM32_STARTUP startup_stm32f407xx.s)
add_subdirectory("{(ROOT / source_directory(family)).as_posix()}" soc)
''')
            cmake = shutil.which("cmake")
            if cmake is None:
                cmake = str(ROOT / ".venv" / "bin" / "cmake")
            result = subprocess.run(
                [cmake, "-S", str(source), "-B", str(source / "build")],
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(result.returncode, 0,
                             result.stdout + result.stderr)
            database = source / "build" / "compile_commands.json"
            if not database.exists():
                return set(), set()
            units = json.loads(database.read_text())
            provider_directory = ROOT / source_directory(family)
            owned_directories = (provider_directory, ROOT / "io/src")
            # Observe the actual build output; no provider-to-source registry
            # may decide which shared translation units the test can see.
            provider = {Path(unit["file"]).relative_to(ROOT).as_posix()
                        for unit in units if any(
                            Path(unit["file"]).is_relative_to(directory)
                            for directory in owned_directories)}
            sdk = {Path(unit["file"]).stem for unit in units
                   if "/vendors/st/stm32f4_hal/Src/" in unit["file"]}
            self.assertEqual(len(units), len({unit["file"] for unit in units}))
            return provider, sdk

    # Base units own cold lifecycle, reset/fault records and silicon queries.
    # Exact sets retain the shared resource, context and pin mechanisms.
    # Selected GPIO/UART/SPI classes also own their C lifecycle adapters; ELF
    # retention remains a separate contract from compilation-unit selection.
    # UART modes share one cold unit. None of these mechanisms selects another
    # device, GPIO face, DMA engine or stream. Keep complete set equality so an
    # unrelated source added by CMake still fails these isolation contracts.
    def test_empty_f4_compiles_only_base_platform_sources(self):
        self.assertEqual(self.configure_sources("stm32f4", []),
                         (self.private_sources("stm32f4", set()), set()))


    def test_gpio_does_not_compile_communication_or_streams(self):
        for family in ("stm32f4",):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("gpio", "output")])
                expected = {"gpio.c", "platform_cold.c", "lifecycle_gpio.c"}
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_irq_uart_retains_pin_helpers_without_dma_or_block_provider(self):
        for family in ("stm32f4",):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("uart", "irq-byte-event")])
                expected = {"gpio.c", "uart.c", "uart_cold.c",
                            "uart_hardware.c", "lifecycle_uart.c",
                            "platform_cold.c"}
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_dma_modes_compile_base_and_dma_without_rx_stream(self):
        for family, spi_mode in (("stm32f4", "dma"),):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("uart", "dma-tx"), ("spi", spi_mode)])
                expected = {"gpio.c", "uart.c", "uart_hal.c", "uart_cold.c",
                            "uart_hardware.c",
                            "lifecycle_uart.c", "spi.c", "spi_dma.c",
                            "lifecycle_spi.c", "platform_cold.c"}
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_rx_and_adc_blocks_retain_base_but_not_pwm_driver(self):
        for family in ("stm32f4",):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("uart", "irq-blocks"), ("adc", "trigger-dma")])
                expected = {"gpio.c", "uart.c", "uart_stream.c", "uart_cold.c",
                            "uart_hardware.c",
                            "lifecycle_uart.c",
                            "adc.c", "adc_stream.c", "adc_trigger.c", "platform_cold.c"}
                self.assertEqual(provider, self.private_sources(family, expected))



    def test_stm32_adc_exti_pwm_retain_generated_pin_helpers(self):
        for kind, mode, source in (("adc", "low-rate-scan", "adc.c"),
                                   ("exti", "edge-event", "exti.c"),
                                   ("pwm", "fixed-pwm", "timer.c")):
            with self.subTest(kind=kind):
                self.assertEqual(self.configure_sources(
                    "stm32f4", [(kind, mode)]), (
                        self.private_sources("stm32f4", {"gpio.c", source,
                                                          "platform_cold.c"}),
                        set()))

    def test_pinless_devices_do_not_compile_gpio(self):
        for family in ("stm32f4",):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("flash", "poll-word"),
                             ("watchdog", "independent")])
                expected = {"flash.c", "watchdog.c", "platform_cold.c"}
                self.assertEqual(sdk, set())
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_window_watchdog_keeps_reset_but_selects_independent_only_on_request(self):
        for modes in (("window",), ("independent", "window")):
            with self.subTest(modes=modes):
                provider, sdk = self.configure_sources(
                    "stm32f4", [("watchdog", mode) for mode in modes])
                expected = {"window_watchdog.c", "platform_cold.c"}
                if "independent" in modes:
                    expected.add("watchdog.c")
                self.assertEqual(provider, self.private_sources("stm32f4", expected))
                self.assertEqual(sdk, set())

    def test_pvd_selects_no_gpio_and_only_event_mode_retains_exti(self):
        for mode in ('pvd-level', 'pvd-events'):
            with self.subTest(mode=mode):
                provider, sdk = self.configure_sources('stm32f4', [('exti', mode)])
                expected = {'pvd.c', 'platform_cold.c'}
                if mode == 'pvd-events':
                    expected.add('exti.c')
                self.assertEqual(provider, self.private_sources('stm32f4', expected))
                self.assertEqual(sdk, set())

    def test_timers_compile_only_the_selected_controller_specializations(self):
        for kind, mode, names, files in (
                ('advanced_pwm', 'complementary-pwm', ('TIM1', 'TIM8'),
                 ('advanced_pwm.c', 'advanced_pwm_tim8.c')),
                ('timer_capture', 'edge-capture', ('TIM3', 'TIM4', 'TIM5'),
                 ('timer_capture_tim3.c', 'timer_capture.c', 'timer_capture_tim5.c'))):
            sources = dict(zip(names, files))
            for controllers in (*((name,) for name in names), names):
                with self.subTest(kind=kind, controllers=controllers):
                    provider, sdk = self.configure_sources('stm32f4', [
                        (kind, mode, name) for name in controllers])
                    expected = {'gpio.c', 'platform_cold.c'} | {
                        sources[name] for name in controllers}
                    self.assertEqual(provider,
                                     self.private_sources('stm32f4', expected))
                    self.assertEqual(sdk, set())

    def test_multiple_instances_share_one_compilation_unit(self):
        for family in ("stm32f4",):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("uart", "irq-byte-event"),
                             ("uart", "irq-blocks")])
                expected = {"gpio.c", "uart.c", "uart_stream.c", "uart_cold.c",
                            "uart_hardware.c",
                            "lifecycle_uart.c",
                            "platform_cold.c"}
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_i2c_selection_observes_the_shared_ip_translation_unit(self):
        for family in ("stm32f4",):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("i2c", "irq-messages")])
                expected = {"gpio.c", "i2c.c", "i2c_async.c", "i2c_cold.c",
                            "platform_cold.c"}
                self.assertEqual(provider,
                    self.private_sources(family, expected) |
                    {"soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"})




if __name__ == "__main__":
    unittest.main()
