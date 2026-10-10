"""Nine Cortex-M CPU contracts reject optional features without exact facts."""
from dataclasses import FrozenInstanceError
import importlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import configure as gate


def facts(arch, **changes):
    cpu = {"arch": arch, "fpu": "none", "float_abi": "soft",
           "dwt_cyccnt": False, "mpu_version": 0,
           "icache_line_bytes": 0, "dcache_line_bytes": 0,
           "security": "single", "sau": False, "mve": "none",
           "dsp": arch in {"cortex-m4", "cortex-m7", "cortex-m55",
                            "cortex-m85"}}
    cpu.update(changes)
    return cpu


def irq(arch):
    baseline = arch in {"cortex-m0", "cortex-m0plus", "cortex-m23"}
    return {"priority_bits": 2 if baseline else 4,
            "external_count": 32 if baseline else 64}


class CpuProfileTests(unittest.TestCase):
    def setUp(self):
        self.cpu = importlib.import_module("cpu")

    def test_m33_dsp_fact_controls_emitted_instruction_capabilities(self):
        for enabled in (False, True):
            with self.subTest(enabled=enabled):
                profile = self.cpu.resolve(facts("cortex-m33", dsp=enabled),
                                           irq("cortex-m33"), "baremetal")
                expected = "-mcpu=cortex-m33" + ("" if enabled else "+nodsp")
                self.assertIn(expected, profile.compile_options)
                self.assertEqual(profile.definitions["NEXUS_CPU_HAS_DSP"],
                                 int(enabled))
                self.assertIn(f"#define NEXUS_CPU_HAS_DSP {int(enabled)}u",
                              self.cpu.header_lines(profile))

    def test_dsp_fact_is_required_without_default_inference(self):
        cpu = facts("cortex-m33")
        cpu.pop("dsp", None)
        with self.assertRaisesRegex(ValueError, "missing"):
            self.cpu.resolve(cpu, irq("cortex-m33"), "baremetal")

    def test_invalid_and_contradictory_dsp_facts_are_rejected(self):
        for arch, enabled in (("native", True), ("cortex-m0", True),
                              ("cortex-m0plus", True),
                              ("cortex-m3", True), ("cortex-m23", True),
                              ("cortex-m4", False), ("cortex-m7", False),
                              ("cortex-m55", False), ("cortex-m85", False),
                              ("cortex-m33", 1), ("cortex-m33", "yes")):
            with (self.subTest(arch=arch, enabled=enabled),
                  self.assertRaisesRegex(ValueError, "DSP")):
                cpu = facts(arch, dsp=enabled)
                record = irq(arch)
                if arch == "native":
                    cpu["float_abi"] = "native"
                    record["external_count"] = 0
                self.cpu.resolve(cpu, record,
                                 "baremetal")

    def test_all_nine_cores_have_exact_distinct_software_profiles(self):
        for arch in ("cortex-m0", "cortex-m0plus", "cortex-m3", "cortex-m4",
                     "cortex-m7", "cortex-m23", "cortex-m33", "cortex-m55",
                     "cortex-m85"):
            with self.subTest(arch=arch):
                profile = self.cpu.resolve(facts(arch), irq(arch), "baremetal")
                self.assertEqual(profile.arch, arch)
                self.assertEqual(profile.atomic_backend,
                                 "irq" if arch in {"cortex-m0", "cortex-m0plus"}
                                 else "exclusive")
                self.assertIn("-mthumb", profile.compile_options)
                self.assertIn("-mfloat-abi=soft", profile.compile_options)
                self.assertFalse(profile.definitions["NEXUS_CPU_HAS_FPU"])
                with self.assertRaises(FrozenInstanceError):
                    profile.arch = "other"
                with self.assertRaises(TypeError):
                    profile.definitions["NEXUS_CPU_HAS_FPU"] = 1

    def test_optional_capabilities_are_explicit_and_chip_specific(self):
        variants = (
            facts("cortex-m0", mpu_version=7),
            facts("cortex-m3", fpu="fpv4-sp-d16", float_abi="hard"),
            facts("cortex-m4", icache_line_bytes=32),
            facts("cortex-m33", dcache_line_bytes=32),
            facts("cortex-m7", mpu_version=8),
            facts("cortex-m33", security="single", sau=True),
            facts("cortex-m23", mve="integer"),
            facts("cortex-m55", mve="float"),
            facts("cortex-m4", float_abi="hard"),
            facts("cortex-m7", dcache_line_bytes=64),
            facts("cortex-m55", dwt_cyccnt=1),
            facts("cortex-m55", mve=[]),
            facts("cortex-m55", security={}),
            facts("cortex-m55", float_abi=[]),
        )
        for cpu in variants:
            with self.subTest(cpu=cpu), self.assertRaises(ValueError):
                self.cpu.resolve(cpu, irq(cpu["arch"]), "baremetal")
        for missing in facts("cortex-m55"):
            cpu = facts("cortex-m55")
            del cpu[missing]
            with self.subTest(missing=missing), self.assertRaisesRegex(
                    ValueError, "missing"):
                self.cpu.resolve(cpu, irq("cortex-m55"), "baremetal")
        with self.assertRaisesRegex(ValueError, "unknown"):
            self.cpu.resolve(facts("cortex-m55", heap=True), irq("cortex-m55"),
                             "baremetal")

    def test_valid_hardware_features_export_arch_and_kernel_definitions(self):
        profile = self.cpu.resolve(
            facts("cortex-m85", fpu="auto", float_abi="hard", mve="float",
                  mpu_version=8, security="secure", sau=True,
                  icache_line_bytes=32, dcache_line_bytes=32, dwt_cyccnt=True),
            irq("cortex-m85"), "freertos")
        self.assertEqual(profile.definitions["NEXUS_ARCH_MPU_VERSION"], 8)
        self.assertEqual(profile.definitions["NEXUS_ARCH_SECURITY_STATE"], 1)
        self.assertEqual(profile.definitions["NEXUS_ARCH_HAS_SAU"], 1)
        self.assertEqual(profile.definitions["NEXUS_CPU_HAS_MVE"], 1)
        self.assertEqual(profile.definitions["NEXUS_CPU_HAS_FPU"], 1)
        self.assertEqual(profile.irq.kernel_port,
                         "GCC/ARM_CM85_NTZ/non_secure")
        self.assertEqual(profile.irq.syscall_priority, 5)
        self.assertEqual(profile.irq.external_count, 64)
        self.assertIn("-mcmse", profile.compile_options)

    def test_vector_integer_and_float_abi_do_not_enable_absent_features(self):
        integer = self.cpu.resolve(facts("cortex-m55", mve="integer",
                                        float_abi="softfp"),
                                   irq("cortex-m55"), "freertos")
        self.assertIn("-mcpu=cortex-m55+nofp", integer.compile_options)
        self.assertFalse(integer.definitions["NEXUS_CPU_HAS_FPU"])
        self.assertTrue(integer.definitions["NEXUS_CPU_HAS_MVE"])
        scalar = self.cpu.resolve(facts("cortex-m85", fpu="auto",
                                       float_abi="hard"),
                                  irq("cortex-m85"), "baremetal")
        self.assertIn("-mcpu=cortex-m85+nomve", scalar.compile_options)
        self.assertFalse(scalar.definitions["NEXUS_CPU_HAS_MVE"])

    def test_kernel_port_matches_baseline_integer_fp_and_v8_contexts(self):
        matrix = (("cortex-m0", "GCC/ARM_CM0"),
                  ("cortex-m0plus", "GCC/ARM_CM0"),
                  ("cortex-m3", "GCC/ARM_CM3"),
                  ("cortex-m4", "GCC/ARM_CM3"),
                  ("cortex-m7", "nexus/ARM_CM7_integer"),
                  ("cortex-m23", "GCC/ARM_CM23_NTZ/non_secure"),
                  ("cortex-m33", "GCC/ARM_CM33_NTZ/non_secure"),
                  ("cortex-m55", "GCC/ARM_CM55_NTZ/non_secure"),
                  ("cortex-m85", "GCC/ARM_CM85_NTZ/non_secure"))
        for arch, port in matrix:
            with self.subTest(arch=arch):
                profile = self.cpu.resolve(facts(arch), irq(arch), "freertos")
                self.assertEqual(profile.irq.kernel_port, port)
                self.assertEqual(profile.irq.syscall_priority,
                                 0 if not profile.has_basepri else 5)
        for arch, fpu, port in (("cortex-m4", "fpv4-sp-d16", "GCC/ARM_CM4F"),
                                ("cortex-m7", "fpv5-d16",
                                 "GCC/ARM_CM7/r0p1")):
            profile = self.cpu.resolve(facts(arch, fpu=fpu, float_abi="hard"),
                                       irq(arch), "freertos")
            self.assertEqual(profile.irq.kernel_port, port)

    def test_irq_width_and_count_do_not_overstate_the_architecture(self):
        for arch, record in (("cortex-m0", {"priority_bits": 4,
                                             "external_count": 32}),
                             ("cortex-m3", {"priority_bits": 4,
                                             "external_count": 241}),
                             ("cortex-m33", {"priority_bits": 4,
                                              "external_count": 481}),
                             ("cortex-m33", {"priority_bits": True,
                                              "external_count": 32}),
                             ("cortex-m33", {"priority_bits": 4,
                                              "external_count": 0})):
            with (self.subTest(arch=arch, record=record),
                  self.assertRaises(ValueError)):
                self.cpu.resolve(facts(arch), record, "freertos")
        profile = self.cpu.resolve(facts("cortex-m33"),
                                   {"priority_bits": 8, "external_count": 480},
                                   "freertos")
        self.assertEqual(profile.irq.syscall_priority % 2, 0)
        self.assertGreater(profile.irq.syscall_priority, 0)

    def test_v8_baseline_irq_count_is_independent_of_mask_kind(self):
        for count in (32, 240):
            with self.subTest(count=count):
                profile = self.cpu.resolve(
                    facts("cortex-m23"),
                    {"priority_bits": 2, "external_count": count}, "freertos")
                self.assertEqual(profile.irq.mask_kind, "primask")
                self.assertEqual(profile.irq.syscall_priority, 0)
                self.assertEqual(profile.irq.external_count, count)
        with self.assertRaises(ValueError):
            self.cpu.resolve(facts("cortex-m23"),
                             {"priority_bits": 2, "external_count": 241},
                             "freertos")

    def test_external_irq_limit_excludes_system_exceptions(self):
        for arch, maximum in (("cortex-m23", 240), ("cortex-m33", 480),
                              ("cortex-m55", 480), ("cortex-m85", 480)):
            with self.subTest(arch=arch):
                exact = {**irq(arch), "external_count": maximum}
                profile = self.cpu.resolve(facts(arch), exact, "freertos")
                self.assertEqual(profile.irq.external_count, maximum)
                for count in (maximum + 1, maximum + 16):
                    with (self.subTest(count=count),
                          self.assertRaises(ValueError)):
                        self.cpu.resolve(facts(arch),
                                         {**exact, "external_count": count},
                                         "freertos")

    def test_baseline_processors_implement_exactly_four_priority_levels(self):
        for arch in ("cortex-m0", "cortex-m0plus", "cortex-m23"):
            with self.subTest(arch=arch):
                profile = self.cpu.resolve(facts(arch), irq(arch), "freertos")
                self.assertEqual(profile.irq.maximum_priority, 3)
                with self.assertRaises(ValueError):
                    self.cpu.resolve(facts(arch),
                                     {**irq(arch), "priority_bits": 3},
                                     "freertos")

    def test_cpu_cli_does_not_invalidate_source_parent_or_symlink_owner(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            source = path / "cpu.json"
            source.write_text(json.dumps(
                {"schema_version": 1, "cpu": facts("cortex-m55"),
                 "irq": irq("cortex-m55"), "clock_hz": 100000000}))
            marker = path / ".nexus-cpu-bundle"
            marker.write_text("Nexus CPU runtime bundle\n")
            command = [str(Path(gate.ROOT / ".venv/bin/python")), "-B",
                       str(gate.ROOT / "tools/configure/cpu.py"),
                       "--facts", str(source), "--backend", "baremetal",
                       "--output", str(path)]
            process = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(process.returncode, 0)
            self.assertTrue(source.is_file(), "Source facts were deleted")
            self.assertTrue(marker.is_file())
            output = path / "output"
            output.mkdir()
            (output / ".nexus-cpu-bundle").symlink_to(marker)
            command[-1] = str(output)
            process = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(process.returncode, 0)
            self.assertTrue(output.is_dir(), "Unowned output was deleted")

    def test_cpu_cli_rejects_symlink_ancestors_without_writing_output(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            actual = path / "actual"
            actual.mkdir()
            alias = path / "alias"
            alias.symlink_to(actual, target_is_directory=True)
            source = actual / "cpu.json"
            source.write_text(json.dumps(
                {"schema_version": 1, "cpu": facts("cortex-m55"),
                 "irq": irq("cortex-m55"), "clock_hz": 100000000}))
            command = [str(Path(gate.ROOT / ".venv/bin/python")), "-B",
                       str(gate.ROOT / "tools/configure/cpu.py"),
                       "--facts", str(alias / "cpu.json"), "--backend",
                       "baremetal", "--output", str(path / "bundle")]
            process = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(process.returncode, 0)
            self.assertFalse((path / "bundle").exists())
            command[4] = str(source)
            command[-1] = str(alias / "bundle")
            process = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(process.returncode, 0)
            self.assertFalse((actual / "bundle").exists())

    def test_external_cpu_bundle_is_atomic_and_uses_the_same_resolution(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            source = path / "cpu.json"
            document = {"schema_version": 1, "cpu": facts("cortex-m55"),
                        "irq": irq("cortex-m55"), "clock_hz": 100000000}
            source.write_text(json.dumps(document))
            output = path / "bundle"
            command = [str(Path(gate.ROOT / ".venv/bin/python")), "-B",
                       str(gate.ROOT / "tools/configure/cpu.py"),
                       "--facts", str(source), "--backend", "baremetal",
                       "--output", str(output)]
            process = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(process.returncode, 0, process.stderr)
            resolved = json.loads((output / "resolved-cpu.json").read_text())
            profile = self.cpu.resolve(document["cpu"], document["irq"],
                                       "baremetal")
            self.assertEqual(resolved["cpu_profile"], profile.to_dict())
            self.assertIn('set(NEXUS_CPU_COMPILE_OPTIONS "',
                          (output / "selection.cmake").read_text())
            self.assertIn('#define NEXUS_CORE_HZ 100000000u',
                          (output / "nexus_config.h").read_text())
            document["cpu"]["sau"] = "yes"
            source.write_text(json.dumps(document))
            process = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(process.returncode, 0)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
