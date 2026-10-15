#!/usr/bin/env python3
"""Keep the derived MPU-v7 peripheral overlay pinned and fail closed."""
import importlib.util
import hashlib
import re
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "mpu_port", ROOT / "os/freertos/mpu/prepare_port.py"
)
PORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PORT)


EXPECTED_VENEER_BYTES = {'ARM_CM3_MPU': '35462285967e070d382df217072f80aa51ee7877e62322f8d21b0f24864c2365', 'ARM_CM4_MPU': '35462285967e070d382df217072f80aa51ee7877e62322f8d21b0f24864c2365', 'ARM_CM23_NTZ/non_secure': 'c2f80289a323f339270c32e15ac7fce9222b069d33614c4f349a42dee22573de', 'ARM_CM33_NTZ/non_secure': '35462285967e070d382df217072f80aa51ee7877e62322f8d21b0f24864c2365', 'ARM_CM55_NTZ/non_secure': '35462285967e070d382df217072f80aa51ee7877e62322f8d21b0f24864c2365', 'ARM_CM85_NTZ/non_secure': '35462285967e070d382df217072f80aa51ee7877e62322f8d21b0f24864c2365'}

class MpuPort(unittest.TestCase):
    def test_both_actual_ports_keep_user_peripherals_inaccessible(self):
        for name in ("ARM_CM3_MPU", "ARM_CM4_MPU"):
            with self.subTest(port=name), tempfile.TemporaryDirectory() as temporary:
                source = ROOT / "ext/freertos/portable/GCC" / name / "port.c"
                before = source.read_bytes()
                output = pathlib.Path(temporary) / "port.c"
                PORT.prepare(source, output)
                self.assertEqual(source.read_bytes(), before)
                derived = output.read_text()
                self.assertIn("__nexus_user_flash_start__", derived)
                self.assertIn("__nexus_syscall_flash_start__", derived)
                self.assertNotIn("= ( portPERIPHERALS_START_ADDRESS )", derived)
                self.assertEqual(derived.count("( portMPU_REGION_READ_ONLY ) |"), 2)
                self.assertNotIn(PORT.ORIGINAL, output.read_text())

    def test_v7_svc_window_uses_the_same_normal_flash_memory_attributes(self):
        for name in ("ARM_CM3_MPU", "ARM_CM4_MPU"):
            with self.subTest(port=name), tempfile.TemporaryDirectory() as temporary:
                output = pathlib.Path(temporary) / "port.c"
                PORT.prepare(ROOT / "ext/freertos/portable/GCC" / name / "port.c", output)
                derived = output.read_text()
                windows = re.findall(
                    r"portMPU_REGION_ATTRIBUTE_REG = \( portMPU_REGION_READ_ONLY \) \|\n([^;]+);",
                    derived)
                self.assertEqual(len(windows), 2)
                attribute = ("( portMPU_REGION_CACHEABLE_BUFFERABLE ) |" if name == "ARM_CM3_MPU"
                             else "( ( configTEX_S_C_B_FLASH & portMPU_RASR_TEX_S_C_B_MASK ) << portMPU_RASR_TEX_S_C_B_LOCATION ) |")
                for window in windows:
                    self.assertIn(attribute, window)

    def test_actual_veneer_derivation_exports_only_the_two_narrow_services(self):
        for name in ("ARM_CM3_MPU", "ARM_CM4_MPU",
                     "ARM_CM23_NTZ/non_secure", "ARM_CM33_NTZ/non_secure",
                     "ARM_CM55_NTZ/non_secure", "ARM_CM85_NTZ/non_secure"):
            with self.subTest(port=name), tempfile.TemporaryDirectory() as temporary:
                source = ROOT / "ext/freertos/portable/GCC" / name / "mpu_wrappers_v2_asm.c"
                before = source.read_bytes()
                output = pathlib.Path(temporary) / "veneers.c"
                PORT.prepare_veneers(source, output)
                self.assertEqual(source.read_bytes(), before)
                derived = output.read_text()
                self.assertIn("MPU_vTaskDelayImpl", derived)
                self.assertIn("MPU_xTaskGetTickCountImpl", derived)
                self.assertNotIn("MPU_xTaskGetCurrentTaskHandle", derived)
                self.assertNotIn("MPU_xQueue", derived)
                self.assertIn('"     svc %0', derived)
                self.assertEqual(hashlib.sha256(output.read_bytes()).hexdigest(),
                                 EXPECTED_VENEER_BYTES[name])
                declarations = re.findall(r"^\s*(?:void|TickType_t) (MPU_\w+)\(",
                                          derived, re.MULTILINE)
                self.assertCountEqual(declarations, ["MPU_vTaskDelay"] * 2 +
                                      ["MPU_xTaskGetTickCount"] * 2)



    def test_every_real_port_guards_cold_start_and_special_svc_before_restore(self):
        for name in ("ARM_CM3_MPU", "ARM_CM4_MPU", "ARM_CM23_NTZ/non_secure",
                     "ARM_CM33_NTZ/non_secure", "ARM_CM55_NTZ/non_secure",
                     "ARM_CM85_NTZ/non_secure"):
            with self.subTest(port=name), tempfile.TemporaryDirectory() as temporary:
                source = ROOT / "ext/freertos/portable/GCC" / name / "port.c"
                output = pathlib.Path(temporary) / "port.c"
                PORT.prepare(source, output)
                derived = output.read_text()
                self.assertEqual(derived.count("nx_freertos_mpu_start_arm(&xNexusMpuStart)"), 1)
                self.assertEqual(derived.count("nx_freertos_mpu_start_consume(&xNexusMpuStart"), 1)
                branch = derived[derived.index("case portSVC_START_SCHEDULER:"):]
                guard = branch.index("nx_freertos_mpu_start_consume")
                restore = branch.index("RestoreContextOfFirstTask")
                self.assertLess(guard, restore)
                self.assertIn("uint32_t ulNexusExceptionReturn", derived)
                if "_NTZ" in name:
                    PORT.prepare_assembly(source.with_name("portasm.c"),
                                          pathlib.Path(temporary) / "portasm.c")
                else:
                    self.assertIn("__nexus_mpu_start_svc_return:", derived)

    def test_vendor_drift_rejects_before_writing_derived_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = pathlib.Path(temporary)
            source = directory / "ARM_CM4_MPU" / "port.c"
            source.parent.mkdir()
            source.write_text(PORT.ORIGINAL + "\n")
            output = directory / "output.c"
            with self.assertRaises(ValueError):
                PORT.prepare(source, output)
            self.assertFalse(output.exists())

    def test_unreviewed_port_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = pathlib.Path(temporary) / "unreviewed" / "port.c"
            source.parent.mkdir()
            source.write_text(PORT.ORIGINAL)
            with self.assertRaises(ValueError):
                PORT.prepare(source, pathlib.Path(temporary) / "output.c")


if __name__ == "__main__":
    unittest.main()
