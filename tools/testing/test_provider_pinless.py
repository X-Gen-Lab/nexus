"""Production F4 links omit unselected GPIO and shared-clock hardware owners."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ProviderPinlessLinkTests(unittest.TestCase):
    def link(self, assembly, *, gpio_selected=False):
        """Use the normal ARM preset and its actual firmware target flags."""
        compiler = os.environ.get("NEXUS_ARM_GCC") or shutil.which(
            "arm-none-eabi-gcc")
        self.assertIsNotNone(compiler, "Prepared ARM compiler is required")
        compiler = Path(compiler).resolve()
        cmake = shutil.which("cmake") or str(ROOT / ".venv/bin/cmake")
        environment = dict(os.environ)
        environment["PATH"] = str(compiler.parent) + os.pathsep + environment["PATH"]
        environment["CMAKE_BUILD_PARALLEL_LEVEL"] = "1"
        with tempfile.TemporaryDirectory(prefix="nexus-pinless-arm-") as temporary:
            build = Path(temporary)
            commands = (
                [cmake, "--preset", "stm32-sky-baremetal", "-B", str(build),
                 "-DNEXUS_ASSEMBLY_FILE=" + str(ROOT / assembly),
                 "-DPython3_EXECUTABLE=" + sys.executable],
                [cmake, "--build", str(build), "--target", "nexus_firmware",
                 "--parallel", "1"])
            for command in commands:
                result = subprocess.run(command, cwd=ROOT, env=environment,
                                        capture_output=True, text=True,
                                        check=False, timeout=120)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
            units = json.loads((build / "compile_commands.json").read_text())
            self.assertEqual(any(Path(unit["file"]).as_posix().endswith(
                "/soc/stm32/stm32f4/drivers/gpio.c") for unit in units),
                gpio_selected,
                "Only selections with pads may compile the GPIO owner")
            elf = build / "bin/nexus_firmware.elf"
            self.assertGreater(elf.stat().st_size, 0)
            symbols = subprocess.run(
                [str(compiler.with_name("arm-none-eabi-nm")),
                 "--defined-only", str(elf)],
                capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(symbols.returncode, 0, symbols.stderr)
            self.assertRegex(symbols.stdout, r"\bnx_stm32_system_start\b")
            self.assertNotRegex(
                symbols.stdout, r"\bnx_platform_lifecycle_(?:start|stop)\b",
                "A fixed assembly must specialize its handwritten coordinator")
            self.assertNotRegex(
                symbols.stdout, r"\bs_nx_platform_plan\b",
                "Plan identity must not retain a redundant callback table")
            if not gpio_selected:
                self.assertIsNone(re.search(
                    r"\bnx_stm32_(?:pin_(?:capture|restore|configure)|"
                    r"platform_pins_(?:acquire|abort|release))\b", symbols.stdout),
                    "Unselected GPIO algorithms must not remain in the ELF")
            self.assertIsNone(re.search(
                r"\bnx_stm32_platform_(?:resources|dma|syscfg)_"
                r"(?:acquire|release)\b", symbols.stdout),
                "Unselected shared-clock algorithms must not remain in the ELF")

    def test_empty_production_image_links_without_gpio(self):
        self.link("tests/contracts/physical_platform_assembly/f407-empty-arm.toml")

    def test_flash_watchdog_image_links_without_gpio(self):
        self.link("tests/contracts/physical_platform_assembly/f407-pinless-arm.toml")

    def test_irq_uart_image_omits_unselected_dma_and_syscfg_clocks(self):
        self.link("tests/contracts/physical_platform_assembly/f407-uart-arm.toml",
                  gpio_selected=True)


if __name__ == "__main__":
    unittest.main()
