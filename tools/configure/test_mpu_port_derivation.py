"""Generic MPU derivation preserves the pinned port and narrow syscall ABI."""

import hashlib
import importlib.util
from pathlib import Path
import re
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "mpu_reviewed_port", ROOT / "os/freertos/mpu/prepare_port.py")
PORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PORT)


class MpuPortDerivationTests(unittest.TestCase):
    def setUp(self):
        self.storage = tempfile.TemporaryDirectory()
        self.addCleanup(self.storage.cleanup)
        self.directory = Path(self.storage.name)
        self.numbers = ROOT / "ext/freertos/include/mpu_syscall_numbers.h"
        self.veneers = (ROOT / "ext/freertos/portable/GCC/ARM_CM33_NTZ/"
                        "non_secure/mpu_wrappers_v2_asm.c")

    def test_number_header_is_identical_and_has_seventy_slots(self):
        output = self.directory / "mpu_syscall_numbers.h"
        PORT.prepare_numbers(self.numbers, output)
        self.assertEqual(output.read_bytes(), self.numbers.read_bytes())
        numbers = re.findall(r"^#define SYSTEM_CALL_\w+\s+\d+\s*$",
                             output.read_text(), re.MULTILINE)
        self.assertEqual(len(numbers), 70)
        self.assertRegex(output.read_text(), r"#define NUM_SYSTEM_CALLS\s+70\b")

    def test_reconfiguration_replaces_stale_generated_outputs(self):
        numbers = self.directory / "mpu_syscall_numbers.h"
        veneers = self.directory / "veneers.c"
        numbers.write_text("stale generated syscall header\n")
        veneers.write_text("stale generated veneer source\n")
        PORT.prepare_numbers(self.numbers, numbers)
        PORT.prepare_veneers(self.veneers, veneers)
        self.assertEqual(numbers.read_bytes(), self.numbers.read_bytes())
        self.assertEqual(hashlib.sha256(veneers.read_bytes()).hexdigest(),
                         "35462285967e070d382df217072f80aa51"
                         "ee7877e62322f8d21b0f24864c2365")

    def test_number_header_drift_is_rejected_before_output(self):
        source = self.directory / "mpu_syscall_numbers.h"
        source.write_bytes(self.numbers.read_bytes().replace(
            b"SYSTEM_CALL_vTaskDelay                             6",
            b"SYSTEM_CALL_vTaskDelay                             7"))
        output = self.directory / "derived.h"
        with self.assertRaisesRegex(ValueError, "pinned.*numbers"):
            PORT.prepare_numbers(source, output)
        self.assertFalse(output.exists())

    def test_only_two_pinned_veneer_bodies_are_exported(self):
        output = self.directory / "veneers.c"
        PORT.prepare_veneers(self.veneers, output)
        pattern = (r"^[ \t]*(?:void|TickType_t) (MPU_\w+)\([^;]+?;\n"
                   r".*?^[ \t]*}\n")
        self.assertCountEqual(
            re.findall(pattern, output.read_text(), re.MULTILINE | re.DOTALL),
            ["MPU_vTaskDelay", "MPU_xTaskGetTickCount"])


if __name__ == "__main__":
    unittest.main()
