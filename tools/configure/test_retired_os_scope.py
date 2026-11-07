"""Current configuration rejects owner-specific features without a target."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.configure import configure, cpu, kernel
from tools.configure.test_cpu_profiles import facts, irq

ROOT = Path(__file__).resolve().parents[2]


class RetiredOsScopeTests(unittest.TestCase):
    def test_removed_gpio_policy_cannot_expand_generic_mpu_ports(self):
        profile = cpu.resolve(
            facts("cortex-m33", mpu_version=8, mpu_regions=8),
            irq("cortex-m33"), "freertos")
        with self.assertRaisesRegex(ValueError, "unknown"):
            kernel.resolve({"memory_protection": True,
                            "user_io": "gpio-write"}, profile, 64000000)

    def test_removed_gpio_derivation_flag_rejects_before_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "numbers.h"
            result = subprocess.run(
                [sys.executable, "-B", str(ROOT / "os/freertos/mpu/prepare_port.py"),
                 "--source", str(ROOT / "ext/freertos/include/mpu_syscall_numbers.h"),
                 "--output", str(output), "--numbers", "--gpio-write"],
                capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertFalse(output.exists())

    def test_removed_target_reservations_reject_at_authored_entry(self):
        with tempfile.TemporaryDirectory() as directory:
            assembly = Path(directory) / "assembly.toml"
            assembly.write_text(
                (ROOT / "tools/configure/assemblies/qiming-baremetal.toml").read_text()
                + "\n[memory.mpu]\nprivileged_flash_bytes=4096\n"
                "user_flash_bytes=4096\nsyscall_flash_bytes=4096\n"
                "privileged_ram_bytes=4096\nuser_ram_bytes=4096\n")
            with self.assertRaisesRegex(ValueError, "unknown"):
                configure.load_assembly(assembly)


if __name__ == "__main__":
    unittest.main()
