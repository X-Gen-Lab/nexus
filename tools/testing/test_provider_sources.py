"""Configure actual SoC targets and inspect their selected compilation units."""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ProviderSourceSelectionTests(unittest.TestCase):
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
            if family == "stm32f407":
                # Only configure is executed; minimal paths satisfy SDK presence
                # checks without compiling counterfeit vendor declarations.
                for name in (
                    "vendors/st/cmsis_device_f4/Include/stm32f407xx.h",
                    "vendors/arm/CMSIS_5/CMSIS/Core/Include/core_cm4.h",
                    "vendors/st/cmsis_device_f4/Source/Templates/gcc/"
                    "startup_stm32f407xx.s",
                ):
                    path = source / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.touch()
                sdk_root = source
            else:
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
add_subdirectory("{(ROOT / 'soc' / family).as_posix()}" soc)
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
            provider = {Path(unit["file"]).name for unit in units
                        if "/soc/" + family + "/" in unit["file"]}
            sdk = {Path(unit["file"]).stem for unit in units
                   if "/GD32F4xx_standard_peripheral/Source/" in unit["file"]}
            self.assertEqual(len(units), len({unit["file"] for unit in units}))
            return provider, sdk

    def test_empty_stm32_compiles_no_device_provider(self):
        self.assertEqual(self.configure_sources("stm32f407", []),
                         (set(), set()))

    def test_empty_gd32_retains_only_timebase_and_system_sdk(self):
        self.assertEqual(self.configure_sources("gd32f470", []), (
            {"timebase.c"},
            {"gd32f4xx_rcu", "gd32f4xx_fmc", "gd32f4xx_timer"},
        ))

    def test_gpio_does_not_compile_communication_or_streams(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("gpio", "output")])
                expected = {"gpio.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                    self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                           "gd32f4xx_timer", "gd32f4xx_gpio"})
                self.assertEqual(provider, expected)

    def test_irq_uart_retains_pin_helpers_without_dma_or_block_provider(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("uart", "irq-byte-event")])
                expected = {"gpio.c", "uart.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                    self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                           "gd32f4xx_timer", "gd32f4xx_gpio",
                                           "gd32f4xx_usart"})
                self.assertEqual(provider, expected)

    def test_dma_modes_compile_base_and_dma_without_rx_stream(self):
        for family, spi_mode in (("stm32f407", "dma"),
                                 ("gd32f470", "dma-full-duplex")):
            with self.subTest(family=family):
                provider, sdk = self.configure_sources(
                    family, [("uart", "dma-tx"), ("spi", spi_mode)])
                expected = {"gpio.c", "uart.c", "uart_dma.c", "spi.c",
                            "spi_dma.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                    self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                           "gd32f4xx_timer", "gd32f4xx_gpio",
                                           "gd32f4xx_usart", "gd32f4xx_spi"})
                self.assertEqual(provider, expected)

    def test_rx_and_adc_blocks_retain_base_but_not_pwm_driver(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("uart", "irq-blocks"), ("adc", "trigger-dma")])
                expected = {"gpio.c", "uart.c", "uart_stream.c", "adc.c",
                            "adc_stream.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                self.assertEqual(provider, expected)

    def test_gd32_adc_uses_vendor_gpio_without_an_unreferenced_provider(self):
        for mode in ("single-shot", "low-rate-scan", "trigger-dma"):
            with self.subTest(mode=mode):
                provider, sdk = self.configure_sources(
                    "gd32f470", [("adc", mode)])
                expected = {"timebase.c", "adc.c"}
                if mode == "trigger-dma":
                    expected.add("adc_stream.c")
                self.assertEqual(provider, expected)
                self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                       "gd32f4xx_timer", "gd32f4xx_gpio",
                                       "gd32f4xx_adc"})

    def test_gd32_exti_and_pwm_use_sdk_pins_without_gpio_provider(self):
        for kind, mode, source in (("exti", "edge-event", "exti.c"),
                                   ("pwm", "fixed-pwm", "timer.c")):
            with self.subTest(kind=kind):
                provider, sdk = self.configure_sources(
                    "gd32f470", [(kind, mode)])
                self.assertEqual(provider, {"timebase.c", source})
                self.assertEqual(sdk, {"gd32f4xx_rcu", "gd32f4xx_fmc",
                                       "gd32f4xx_timer", "gd32f4xx_gpio"})

    def test_stm32_adc_exti_pwm_retain_generated_pin_helpers(self):
        for kind, mode, source in (("adc", "low-rate-scan", "adc.c"),
                                   ("exti", "edge-event", "exti.c"),
                                   ("pwm", "fixed-pwm", "timer.c")):
            with self.subTest(kind=kind):
                self.assertEqual(self.configure_sources(
                    "stm32f407", [(kind, mode)]), ({"gpio.c", source}, set()))

    def test_pinless_devices_do_not_compile_gpio(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("flash", "poll-word"),
                             ("watchdog", "independent")])
                expected = {"flash.c", "watchdog.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                self.assertEqual(provider, expected)

    def test_multiple_instances_share_one_compilation_unit(self):
        for family in ("stm32f407", "gd32f470"):
            with self.subTest(family=family):
                provider, _ = self.configure_sources(
                    family, [("uart", "irq-byte-event"),
                             ("uart", "irq-blocks")])
                expected = {"gpio.c", "uart.c", "uart_stream.c"}
                if family == "gd32f470":
                    expected.add("timebase.c")
                self.assertEqual(provider, expected)


if __name__ == "__main__":
    unittest.main()
