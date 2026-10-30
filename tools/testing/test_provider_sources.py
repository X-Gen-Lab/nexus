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
                              "drivers/" + name)).as_posix() for name in names}

    def configure_sources(self, family, controllers):
        """Inspect production targets without claiming an ARM link proof."""
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            generated = source / "generated"
            generated.mkdir()
            (generated / "resolved.json").write_text(json.dumps({
                "controllers": [{"kind": kind, "mode": mode}
                                for kind, mode in controllers],
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
add_library(nexus_arch INTERFACE)
add_library(Nexus::Arch ALIAS nexus_arch)
add_library(nexus_build_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
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
            owned_directories = (provider_directory, ROOT / "drivers",
                                 ROOT / "io/src")
            # Observe the actual build output; no provider-to-source registry
            # may decide which shared translation units the test can see.
            provider = {Path(unit["file"]).relative_to(ROOT).as_posix()
                        for unit in units if any(
                            Path(unit["file"]).is_relative_to(directory)
                            for directory in owned_directories)}
            sdk = {Path(unit["file"]).stem for unit in units
                   if "/GD32F4xx_standard_peripheral/Source/" in unit["file"]}
            self.assertEqual(len(units), len({unit["file"] for unit in units}))
            return provider, sdk

    # Cold lifecycle mechanisms are real C units now. Exact expected sets below
    # include their reviewed ownership: selected F407/G474 platforms need the
    # shared cold resource code. GD470 archives that baseline with its timebase
    # even for an empty graph; ELF retention is a separate contract.
    # UART modes share one cold unit. None of these mechanisms selects another
    # device, GPIO face, DMA engine or stream. Keep complete set equality so an
    # unrelated source added by CMake still fails these isolation contracts.
    def test_empty_stm32_compiles_no_device_provider(self):
        self.assertEqual(self.configure_sources("stm32f407", []),
                         (set(), set()))

    def test_empty_gd32_compiles_only_cold_lifecycle_timebase_and_system_sdk(self):
        self.assertEqual(self.configure_sources("gd32f470", []), (
            self.private_sources("gd32f470", {"timebase.c", "platform_cold.c"}),
            {"gd32f4xx_rcu", "gd32f4xx_fmc", "gd32f4xx_timer"},
        ))

    def test_gpio_does_not_compile_communication_or_streams(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("gpio", "output")])
                expected = {"gpio.c", "platform_cold.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                    self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                           "gd32f4xx_timer", "gd32f4xx_gpio"})
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_irq_uart_retains_pin_helpers_without_dma_or_block_provider(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("uart", "irq-byte-event")])
                expected = {"gpio.c", "uart.c", "uart_cold.c",
                            "platform_cold.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                    self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                           "gd32f4xx_timer", "gd32f4xx_gpio",
                                           "gd32f4xx_usart"})
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_dma_modes_compile_base_and_dma_without_rx_stream(self):
        for family, spi_mode in (("stm32f407", "dma"),
                                 ("gd32f470", "dma-full-duplex")):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("uart", "dma-tx"), ("spi", spi_mode)])
                transmit = ("uart_hal.c" if family == "stm32f407" else
                            "uart_dma.c")
                expected = {"gpio.c", "uart.c", transmit, "uart_cold.c",
                            "spi.c", "spi_dma.c", "platform_cold.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                    self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                           "gd32f4xx_timer", "gd32f4xx_gpio",
                                           "gd32f4xx_usart", "gd32f4xx_spi"})
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_rx_and_adc_blocks_retain_base_but_not_pwm_driver(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("uart", "irq-blocks"), ("adc", "trigger-dma")])
                expected = {"gpio.c", "uart.c", "uart_stream.c", "uart_cold.c",
                            "adc.c", "adc_stream.c", "platform_cold.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_gd32_adc_retains_platform_pin_helpers(self):
        for mode in ("single-shot", "low-rate-scan", "trigger-dma",
                     "trigger-dma-continuous"):
            with self.subTest(mode=mode):
                provider, sdk = self.configure_sources(
                    "gd32f470", [("adc", mode)])
                # GPIO compilation supplies the lease, not a GPIO selection.
                expected = {"timebase.c", "adc.c", "gpio.c", "platform_cold.c"}
                if mode in ("trigger-dma", "trigger-dma-continuous"):
                    expected.add("adc_stream.c")
                if mode == "trigger-dma-continuous":
                    expected.add("adc_continuous.c")
                self.assertEqual(provider, self.private_sources(
                    "gd32f470", expected))
                self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                       "gd32f4xx_timer", "gd32f4xx_gpio",
                                       "gd32f4xx_adc"})

    def test_gd32_exti_and_pwm_retain_platform_pin_helpers(self):
        for kind, mode, source in (("exti", "edge-event", "exti.c"),
                                   ("pwm", "fixed-pwm", "timer.c")):
            with self.subTest(kind=kind):
                provider, sdk = self.configure_sources(
                    "gd32f470", [(kind, mode)])
                self.assertEqual(provider, self.private_sources(
                    "gd32f470", {"timebase.c", source, "gpio.c",
                                "platform_cold.c"}))
                self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                       "gd32f4xx_timer", "gd32f4xx_gpio"})

    def test_stm32_adc_exti_pwm_retain_generated_pin_helpers(self):
        for kind, mode, source in (("adc", "low-rate-scan", "adc.c"),
                                   ("exti", "edge-event", "exti.c"),
                                   ("pwm", "fixed-pwm", "timer.c")):
            with self.subTest(kind=kind):
                self.assertEqual(self.configure_sources(
                    "stm32f407", [(kind, mode)]), (
                        self.private_sources("stm32f407", {"gpio.c", source,
                                                          "platform_cold.c"}),
                        set()))

    def test_pinless_devices_do_not_compile_gpio(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("flash", "poll-word"),
                             ("watchdog", "independent")])
                expected = {"flash.c", "watchdog.c", "platform_cold.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                    self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                           "gd32f4xx_timer", "gd32f4xx_fwdgt"})
                else:
                    self.assertEqual(sdk, set())
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_multiple_instances_share_one_compilation_unit(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("uart", "irq-byte-event"),
                             ("uart", "irq-blocks")])
                expected = {"gpio.c", "uart.c", "uart_stream.c", "uart_cold.c",
                            "platform_cold.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                self.assertEqual(provider, self.private_sources(family, expected))

    def test_i2c_selection_observes_the_shared_ip_translation_unit(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("i2c", "irq-messages")])
                expected = {"gpio.c", "i2c.c", "i2c_async.c", "i2c_cold.c",
                            "platform_cold.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                self.assertEqual(provider,
                    self.private_sources(family, expected) |
                    {"drivers/i2c/i2c_v1_async/i2c_v1_async.c"})

    def test_legacy_modes_retain_shared_engines_once_and_private_wrappers(self):
        provider, _ = self.configure_sources("stm32f103", [
            ("uart", "dma-tx"), ("uart", "dma-duplex"),
            ("i2c", "irq-messages"), ("adc", "single-shot"),
            ("pwm", "fixed-pwm"), ("flash", "poll-word")])
        self.assertEqual(provider, self.private_sources("stm32f103", {
            "gpio.c", "uart_dma.c", "i2c_async.c", "devices.c"}) | {
                "drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.c",
                "drivers/i2c/i2c_v1_async/i2c_v1_async.c",
                "drivers/adc/legacy_adc/legacy_adc.c",
                "drivers/timer/legacy_pwm/legacy_pwm.c",
                "drivers/flash/legacy_flash/legacy_flash.c"})

    def test_i2c_v2_retains_its_distinct_shared_engine(self):
        provider, _ = self.configure_sources(
            "stm32g474", [("i2c", "poll-messages")])
        self.assertEqual(provider,
            self.private_sources("stm32g474", {"i2c.c", "platform_cold.c"}) |
            {"drivers/i2c/i2c_v2/i2c_v2.c"})


if __name__ == "__main__":
    unittest.main()
