"""The opt-in U575 scalar GPIO SVC retains the pinned wrapper-v2 ABI."""

import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "mpu_gpio_port", ROOT / "os/freertos/mpu/prepare_port.py")
PORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PORT)


class MpuGpioDerivationTests(unittest.TestCase):
    def setUp(self):
        self.storage = tempfile.TemporaryDirectory()
        self.addCleanup(self.storage.cleanup)
        self.directory = Path(self.storage.name)
        self.numbers = ROOT / "ext/freertos/include/mpu_syscall_numbers.h"

    def port_source(self, filename, name="ARM_CM33_NTZ/non_secure"):
        return ROOT / "ext/freertos/portable/GCC" / name / filename

    def test_selected_header_keeps_all_seventy_original_numbers(self):
        output = self.directory / "mpu_syscall_numbers.h"
        original = self.numbers.read_bytes()
        PORT.prepare_numbers(self.numbers, output, gpio_write=True)
        pattern = r"^#define (SYSTEM_CALL_\w+)\s+(\d+)\s*$"
        before = dict(re.findall(pattern, original.decode(), re.MULTILINE))
        after = dict(re.findall(pattern, output.read_text(), re.MULTILINE))
        self.assertEqual(len(before), 70)
        self.assertEqual({key: after.pop(key) for key in before}, before)
        self.assertEqual(after, {"SYSTEM_CALL_NexusGpioWrite": "70"})
        self.assertRegex(output.read_text(), r"#define NUM_SYSTEM_CALLS\s+71\b")
        self.assertEqual(self.numbers.read_bytes(), original)

    def test_unselected_header_is_identical_and_has_seventy_slots(self):
        output = self.directory / "mpu_syscall_numbers.h"
        PORT.prepare_numbers(self.numbers, output)
        self.assertEqual(output.read_bytes(), self.numbers.read_bytes())
        self.assertNotIn("NexusGpioWrite", output.read_text())
        self.assertRegex(output.read_text(), r"#define NUM_SYSTEM_CALLS\s+70\b")

    def test_reconfiguration_replaces_selected_outputs_with_exact_defaults(self):
        numbers = self.directory / "mpu_syscall_numbers.h"
        veneers = self.directory / "veneers.c"
        source = self.port_source("mpu_wrappers_v2_asm.c")
        PORT.prepare_numbers(self.numbers, numbers, gpio_write=True)
        PORT.prepare_veneers(source, veneers, gpio_write=True)
        self.assertIn("NexusGpioWrite", numbers.read_text())
        self.assertIn("NexusGpioWrite", veneers.read_text())
        PORT.prepare_numbers(self.numbers, numbers)
        PORT.prepare_veneers(source, veneers)
        self.assertEqual(numbers.read_bytes(), self.numbers.read_bytes())
        self.assertNotIn("NexusGpioWrite", veneers.read_text())
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
            PORT.prepare_numbers(source, output, gpio_write=True)
        self.assertFalse(output.exists())

    def test_new_naked_scalar_veneer_keeps_the_original_two_bodies(self):
        source = self.port_source("mpu_wrappers_v2_asm.c")
        default = self.directory / "default.c"
        selected = self.directory / "selected.c"
        PORT.prepare_veneers(source, default)
        PORT.prepare_veneers(source, selected, gpio_write=True)
        before, after = default.read_text(), selected.read_text()
        pattern = (r"^[ \t]*(?:void|TickType_t) (MPU_\w+)\([^;]+?;\n"
                   r".*?^[ \t]*}\n")
        bodies = re.findall(pattern, before, re.MULTILINE | re.DOTALL)
        self.assertCountEqual(bodies, ["MPU_vTaskDelay", "MPU_xTaskGetTickCount"])
        for match in re.finditer(pattern, before, re.MULTILINE | re.DOTALL):
            self.assertIn(match.group(), after)
        self.assertRegex(after, r"nx_result_t MPU_NexusGpioWrite\(uint32_t device,\s*"
                         r"uint32_t set_mask,\s*uint32_t reset_mask\)")
        self.assertIn("__attribute__((naked)) FREERTOS_SYSTEM_CALL", after)
        self.assertIn('"svc %0', after)
        self.assertIn('"i"(SYSTEM_CALL_NexusGpioWrite)', after)
        self.assertIn('"i"(NX_ERROR_CONTEXT)', after)
        self.assertNotIn("MPU_NexusGpioWriteImpl", after)
        self.assertEqual(hashlib.sha256(default.read_bytes()).hexdigest(),
                         "35462285967e070d382df217072f80aa51"
                         "ee7877e62322f8d21b0f24864c2365")

    def test_opt_in_rejects_every_other_maintained_port_before_writing(self):
        for name in ("ARM_CM3_MPU", "ARM_CM4_MPU",
                     "ARM_CM23_NTZ/non_secure", "ARM_CM55_NTZ/non_secure",
                     "ARM_CM85_NTZ/non_secure"):
            with self.subTest(port=name):
                output = self.directory / "unsupported.c"
                for method, filename in ((PORT.prepare, "port.c"),
                                         (PORT.prepare_veneers,
                                          "mpu_wrappers_v2_asm.c")):
                    with self.assertRaisesRegex(ValueError, "GPIO.*CM33"):
                        method(self.port_source(filename, name), output,
                               gpio_write=True)
                    self.assertFalse(output.exists())

    def test_selected_real_port_and_context_assembly_stay_identical(self):
        for method, filename in ((PORT.prepare, "port.c"),
                                 (PORT.prepare_assembly, "portasm.c")):
            with self.subTest(source=filename):
                source = self.port_source(filename)
                before = self.directory / ("default-" + filename)
                after = self.directory / ("selected-" + filename)
                method(source, before)
                method(source, after, gpio_write=True)
                self.assertEqual(before.read_bytes(), after.read_bytes())

    def test_cli_accepts_only_the_selected_numbers_and_veneer_paths(self):
        script = Path(PORT.__file__)
        for mode, source in (("--numbers", self.numbers),
                             ("--veneers", self.port_source(
                                 "mpu_wrappers_v2_asm.c"))):
            with self.subTest(mode=mode):
                output = self.directory / (mode[2:] + ".out")
                result = subprocess.run(
                    [sys.executable, "-B", str(script), "--source", str(source),
                     "--output", str(output), mode, "--gpio-write"],
                    capture_output=True, text=True, check=False, timeout=15)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("NexusGpioWrite", output.read_text())


if __name__ == "__main__":
    unittest.main()
