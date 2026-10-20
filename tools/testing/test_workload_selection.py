"""Minimal workloads select real assembly-local factory IDs without aliases."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure.configure import configure

ROOT = Path(__file__).resolve().parents[2]


class WorkloadSelectionTests(unittest.TestCase):
    def select(self, kind="UART", selected="", assembly="gd32-f303-baremetal-io.toml"):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "generated"
            configure(ROOT / "tools/configure/assemblies" / assembly, output, ROOT)
            script = root / "check.cmake"
            script.write_text(f'include("{ROOT}/cmake/platform/Workload.cmake")\n'
                f'nexus_workload_identity("{output}" "{kind}" "{selected}" result)\n'
                'message(STATUS "SELECTED=${result}")\n')
            return subprocess.run([shutil.which("cmake"), "-P", str(script)],
                                  capture_output=True, text=True, timeout=30)

    def test_real_gd303_logical_serial_id_is_selected(self):
        result = self.select()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SELECTED=NX_UART_ID_SERIAL", result.stdout)

    def test_gpio_output_identity_is_independent_of_led_alias(self):
        result = self.select("GPIO")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SELECTED=NX_GPIO_ID_OUTPUT", result.stdout)

    def test_explicit_valid_id_and_absent_or_wrong_class_ids(self):
        self.assertEqual(self.select(selected="NX_UART_ID_SERIAL").returncode, 0)
        for value in ("NX_UART_ID_UART0", "NX_GPIO_ID_OUTPUT", "0", "bad;id"):
            with self.subTest(value=value):
                self.assertNotEqual(self.select(selected=value).returncode, 0)

    def test_no_uart_cannot_silently_select_a_nonexistent_port(self):
        result = self.select(assembly="gd32-f303-baremetal-empty.toml")
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
