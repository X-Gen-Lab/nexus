"""Maintained kernel policy, startup frame and Tick admission regressions."""
import importlib
import json
from pathlib import Path
import re
import tempfile
import unittest

import configure
import cpu
import runtime
from test_cpu_profiles import facts, irq


ROOT = Path(__file__).resolve().parents[2]


class KernelProfileTests(unittest.TestCase):
    def resolve(self, choices=None, *, arch="cortex-m4", hz=168000000,
                fpu="none", bits=None):
        features = facts(arch, fpu=fpu)
        if fpu != "none":
            features["float_abi"] = "hard"
        interrupts = irq(arch)
        if bits is not None:
            interrupts["priority_bits"] = bits
        profile = cpu.resolve(features, interrupts, "freertos")
        return importlib.import_module("kernel").resolve(choices, profile, hz)

    def test_standard_preserves_reviewed_existing_policy(self):
        result = self.resolve()
        self.assertEqual(result.profile, "standard")
        self.assertEqual(result.tick_hz, 1000)
        self.assertEqual(result.tick_reload, 167999)
        self.assertEqual(result.max_priorities, 8)
        self.assertEqual(result.idle_stack_words, 128)
        self.assertEqual(result.max_task_name_len, 16)
        self.assertEqual(result.syscall_priority, 5)
        self.assertTrue(result.mutexes)
        self.assertTrue(result.counting_semaphores)
        self.assertTrue(result.task_notifications)
        self.assertFalse(result.trace)
        self.assertFalse(result.tickless)

    def test_profiles_are_small_explicit_feature_templates(self):
        minimal = self.resolve({"profile": "minimal"})
        self.assertEqual(minimal.max_priorities, 4)
        self.assertEqual(minimal.max_task_name_len, 8)
        self.assertFalse(minimal.mutexes)
        self.assertFalse(minimal.counting_semaphores)
        self.assertFalse(minimal.task_notifications)
        self.assertTrue(self.resolve({"profile": "diagnostic"}).trace)
        self.assertTrue(self.resolve({"profile": "lowpower"}).tickless)

    def test_ir_and_definitions_are_immutable(self):
        result = self.resolve()
        with self.assertRaises((AttributeError, TypeError)):
            result.tick_hz = 1
        with self.assertRaises(TypeError):
            result.definitions["NEXUS_OS_TICK_HZ"] = 1

    def test_unknown_policy_and_macro_overrides_are_rejected(self):
        for field in ("cpu", "priority_bits", "worker", "configMAX_PRIORITIES"):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "unknown"):
                self.resolve({field: 1})
        with self.assertRaisesRegex(ValueError, "profile"):
            self.resolve({"profile": "automatic"})

    def test_policy_numeric_ranges_reject_bool_and_overflow(self):
        for field, values in {
                "tick_hz": (0, True, 1000001),
                "max_priorities": (0, True, 33),
                "idle_stack_words": (0, True, 65537),
                "max_task_name_len": (0, True, 65),
                "notification_slots": (0, True, 9)}.items():
            for value in values:
                with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                    self.resolve({field: value})

    def test_feature_switches_require_actual_booleans(self):
        for field in ("mutexes", "counting_semaphores", "task_notifications",
                      "trace", "runtime_stats", "tickless"):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "boolean"):
                self.resolve({field: 1})

    def test_systick_rejects_zero_reload_overflow_and_frequency_truncation(self):
        for hz in (999, 1000, 1000001, 0xFFFFFFFF):
            with self.subTest(hz=hz), self.assertRaisesRegex(ValueError, "SysTick"):
                self.resolve(hz=hz)
        self.assertEqual(self.resolve(hz=2000).tick_reload, 1)
        self.assertEqual(self.resolve({"tick_hz": 100}, hz=1677721600).tick_reload,
                         0xFFFFFF)

    def test_external_tick_is_explicit_and_has_no_systick_inference(self):
        result = self.resolve({"tick_source": "external", "tick_hz": 333}, hz=1)
        self.assertEqual(result.tick_source, "external")
        self.assertIsNone(result.tick_reload)
        self.assertEqual(result.definitions["NEXUS_OS_EXTERNAL_TICK"], 1)
        with self.assertRaisesRegex(ValueError, "tick source"):
            self.resolve({"tick_source": "automatic"})

    def test_tickless_requires_exact_microsecond_period(self):
        with self.assertRaisesRegex(ValueError, "microsecond"):
            self.resolve({"tick_source": "external", "tick_hz": 333,
                          "tickless": True})

    def test_actual_port_frame_controls_minimum_stack(self):
        expected = {"cortex-m0": 20, "cortex-m0plus": 20,
                    "cortex-m3": 18, "cortex-m4": 18, "cortex-m7": 18,
                    "cortex-m23": 20, "cortex-m33": 20,
                    "cortex-m55": 20, "cortex-m85": 20}
        for arch, words in expected.items():
            with self.subTest(arch=arch):
                result = self.resolve(arch=arch)
                self.assertEqual(result.min_stack_words, words)
                self.assertEqual(result.stack_alignment_bytes, 8)
        self.assertEqual(self.resolve(fpu="fpv4-sp-d16").min_stack_words, 20)

    def test_startup_frame_facts_match_locked_kernel_sources(self):
        ports = {"ARM_CM0": 17, "ARM_CM3": 16, "ARM_CM4F": 17,
                 "ARM_CM7/r0p1": 17,
                 **{f"ARM_CM{core}_NTZ/non_secure": 18
                    for core in (23, 33, 55, 85)}}
        for port, decrement in ports.items():
            with self.subTest(port=port):
                source = (ROOT / "ext/freertos/portable/GCC" / port / "port.c").read_text()
                # Select the MPU-disabled initialization, then the maintained
                # default register-preload branch, not Secure-context extras.
                functions = source.split("StackType_t * pxPortInitialiseStack(")[1:]
                function = functions[-1].split("return pxTopOfStack;")[0]
                if "#if ( portPRELOAD_REGISTERS == 0 )" in function:
                    function = function.split("#if ( portPRELOAD_REGISTERS == 0 )", 1)[1]
                    function = function.split("#else", 1)[0]
                function = function.split("#if ( configENABLE_TRUSTZONE", 1)[0]
                actual = len(re.findall(r"pxTopOfStack\s*--;", function))
                actual += sum(map(int, re.findall(r"pxTopOfStack\s*-=\s*(\d+);", function)))
                self.assertEqual(actual, decrement)

    def test_idle_stack_must_fit_initial_frame_and_alignment(self):
        for words in (17, 19):
            with self.subTest(words=words), self.assertRaisesRegex(ValueError, "stack"):
                self.resolve({"idle_stack_words": words})
        self.assertEqual(self.resolve({"idle_stack_words": 18}).idle_stack_words, 18)

    def test_basepri_priority_count_and_syscall_policy_are_validated(self):
        self.assertEqual(self.resolve({"max_priorities": 32}).max_priorities, 32)
        self.assertEqual(self.resolve(bits=8).syscall_priority, 10)
        self.assertEqual(self.resolve({"syscall_priority": 7}).syscall_priority, 7)
        for ceiling in (0, 16, True):
            with self.subTest(ceiling=ceiling), self.assertRaises(ValueError):
                self.resolve({"syscall_priority": ceiling})

    def test_baseline_has_explicit_primask_policy_without_fake_ceiling(self):
        result = self.resolve(arch="cortex-m0")
        self.assertEqual(result.kernel_policy, "primask")
        self.assertEqual(result.syscall_priority, 0)
        self.assertEqual(result.definitions["NEXUS_IRQ_KERNEL_POLICY"], 1)
        with self.assertRaises(ValueError):
            self.resolve({"syscall_priority": 1}, arch="cortex-m23")

    def test_every_cpu_backend_emits_explicit_kernel_irq_policy(self):
        for arch, capability in cpu.PROFILES.items():
            cpu_facts = facts(arch)
            interrupts = irq(arch)
            if arch == "native":
                cpu_facts["float_abi"] = "native"
                interrupts["external_count"] = 0
            for backend in (("native", "baremetal") if arch == "native"
                            else ("baremetal", "freertos")):
                with self.subTest(arch=arch, backend=backend):
                    profile = cpu.resolve(cpu_facts, interrupts, backend)
                    expected = ((2 if capability.basepri else 1)
                                if backend == "freertos" else 0)
                    self.assertEqual(profile.definitions["NEXUS_IRQ_KERNEL_POLICY"], expected)
                    self.assertIn(f"#define NEXUS_IRQ_KERNEL_POLICY {expected}u",
                                  cpu.header_lines(profile))
                    if backend == "freertos" and not capability.basepri:
                        self.assertIn("#define NEXUS_IRQ_SYSCALL_PRIORITY 0u",
                                      cpu.header_lines(profile))

    def test_mpu_policy_requires_explicit_physical_region_count(self):
        features = facts("cortex-m3", mpu_version=7, mpu_regions=8)
        profile = cpu.resolve(features, irq("cortex-m3"), "freertos")
        result = importlib.import_module("kernel").resolve(
            {"memory_protection": True}, profile, 72000000)
        self.assertTrue(result.memory_protection)
        self.assertEqual(result.kernel_port, "GCC/ARM_CM3_MPU")
        self.assertEqual(result.system_call_stack_words, 128)
        features.pop("mpu_regions")
        profile = cpu.resolve(features, irq("cortex-m3"), "freertos")
        with self.assertRaisesRegex(ValueError, "explicit MPU"):
            importlib.import_module("kernel").resolve(
                {"memory_protection": True}, profile, 72000000)

    def test_mpu_port_selection_tracks_exact_cpu_and_fpu(self):
        for arch, fpu, port in (("cortex-m3", "none", "GCC/ARM_CM3_MPU"),
                                ("cortex-m4", "none", "GCC/ARM_CM3_MPU"),
                                ("cortex-m4", "fpv4-sp-d16", "GCC/ARM_CM4_MPU"),
                                ("cortex-m33", "none", "GCC/ARM_CM33_NTZ/non_secure")):
            with self.subTest(arch=arch, fpu=fpu):
                features = facts(arch, fpu=fpu, mpu_regions=8,
                                 mpu_version=cpu.PROFILES[arch].mpu)
                if fpu != "none":
                    features["float_abi"] = "hard"
                profile = cpu.resolve(features, irq(arch), "freertos")
                module = importlib.import_module("kernel")
                result = module.resolve({"memory_protection": True}, profile, 48000000)
                self.assertEqual(module.bind(profile, result).irq.kernel_port, port)
        for words in (63, 65, 65537, True):
            with self.subTest(words=words), self.assertRaises(ValueError):
                self.resolve({"system_call_stack_words": words})

    def test_unsupported_kernel_mpu_tuple_and_false_physical_regions_reject(self):
        for arch in ("cortex-m0plus", "cortex-m7"):
            features = facts(arch, mpu_version=cpu.PROFILES[arch].mpu, mpu_regions=8)
            profile = cpu.resolve(features, irq(arch), "freertos")
            with self.subTest(arch=arch), self.assertRaisesRegex(ValueError, "MPU kernel"):
                importlib.import_module("kernel").resolve(
                    {"memory_protection": True}, profile, 48000000)
        for regions in (True, 7, 16):
            with self.subTest(regions=regions), self.assertRaisesRegex(ValueError, "MPU region"):
                cpu.resolve(facts("cortex-m3", mpu_version=7, mpu_regions=regions),
                            irq("cortex-m3"), "freertos")

    def test_split_security_requires_v8_nonsecure_and_has_actual_context_slot(self):
        for arch in ("cortex-m23", "cortex-m33", "cortex-m55", "cortex-m85"):
            features = facts(arch, security="nonsecure")
            profile = cpu.resolve(features, irq(arch), "freertos")
            result = importlib.import_module("kernel").resolve(
                {"security_model": "split"}, profile, 48000000)
            self.assertEqual(result.security_model, "split")
            self.assertEqual(result.kernel_port,
                             f"GCC/ARM_CM{arch[8:]}/non_secure")
            self.assertEqual(result.min_stack_words, 22)
            self.assertEqual(result.definitions["NEXUS_OS_TRUSTZONE"], 1)
        for choices in ({"security_model": "split"}, {"security_model": "automatic"}):
            with self.subTest(choices=choices), self.assertRaises(ValueError):
                self.resolve(choices)

    def test_mpu_and_split_security_are_not_silently_combined(self):
        profile = cpu.resolve(facts("cortex-m33", security="nonsecure",
                                    mpu_version=8, mpu_regions=8),
                              irq("cortex-m33"), "freertos")
        with self.assertRaisesRegex(ValueError, "combination"):
            importlib.import_module("kernel").resolve(
                {"security_model": "split", "memory_protection": True},
                profile, 48000000)

    def test_split_idle_secure_budget_is_explicit_aligned_and_bounded(self):
        profile = cpu.resolve(facts("cortex-m33", security="nonsecure"),
                              irq("cortex-m33"), "freertos")
        module = importlib.import_module("kernel")
        default = module.resolve({"security_model": "split"}, profile, 48000000)
        self.assertEqual(default.secure_idle_stack_bytes, 128)
        self.assertEqual(default.definitions["NEXUS_OS_SECURE_IDLE_STACK_BYTES"], 128)
        selected = module.resolve({"security_model": "split",
                                   "secure_idle_stack_bytes": 256},
                                  profile, 48000000)
        self.assertEqual(selected.secure_idle_stack_bytes, 256)
        for value in (0, 63, 65, 65537, True):
            with self.subTest(value=value), self.assertRaises(ValueError):
                module.resolve({"security_model": "split",
                                "secure_idle_stack_bytes": value},
                               profile, 48000000)

    def test_single_world_has_no_secure_idle_budget_or_authoring_knob(self):
        result = self.resolve()
        self.assertEqual(result.secure_idle_stack_bytes, 0)
        self.assertEqual(result.definitions["NEXUS_OS_SECURE_IDLE_STACK_BYTES"], 0)
        for value in (0, 128, 256):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "split"):
                self.resolve({"secure_idle_stack_bytes": value})

    def test_bind_updates_irq_policy_without_changing_cpu_abi(self):
        module = importlib.import_module("kernel")
        profile = cpu.resolve(facts("cortex-m3"), irq("cortex-m3"), "freertos")
        policy = module.resolve({"syscall_priority": 7}, profile, 72000000)
        bound = module.bind(profile, policy)
        self.assertEqual(bound.irq.syscall_priority, 7)
        self.assertEqual(bound.compile_options, profile.compile_options)
        self.assertEqual(bound["cpu"], profile["cpu"])


class KernelAssemblyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="kernel profile ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        soc = json.loads((ROOT / "soc/stm32f407/soc.json").read_text())
        self.facts = self.root / "cpu.json"
        self.facts.write_text(json.dumps({"schema_version": 1, "cpu": soc["cpu"],
                                         "irq": soc["irq"], "clock_hz": 168000000}))
        self.assembly = self.root / "runtime.toml"
        self.assembly.write_text('schema_version=1\ncpu_facts="cpu.json"\n'
                                 'backend="freertos"\noptimization="Os"\n')

    def test_runtime_emits_the_same_typed_profile_to_header_and_cmake(self):
        self.assembly.write_text(self.assembly.read_text() +
                                 '[os]\nprofile="minimal"\ntick_hz=2000\n'
                                 'syscall_priority=7\nnotification_slots=2\n')
        output = self.root / "generated"
        result = runtime.configure(self.assembly, output)
        self.assertEqual(result.kernel.max_priorities, 4)
        self.assertEqual(result.profile.irq.syscall_priority, 7)
        for name in ("nexus_config.h", "selection.cmake"):
            text = (output / name).read_text()
            self.assertIn("NEXUS_OS_TICK_HZ", text)
            self.assertIn("NEXUS_OS_MIN_STACK_WORDS", text)
        resolved = json.loads((output / "resolved-cpu.json").read_text())
        self.assertEqual(resolved["kernel_profile"]["tick_hz"], 2000)

    def test_rejected_kernel_policy_invalidates_old_runtime_bundle(self):
        output = self.root / "generated"
        runtime.configure(self.assembly, output)
        self.assembly.write_text(self.assembly.read_text() + '[os]\ntick_hz=0\n')
        with self.assertRaises(runtime.RuntimeError):
            runtime.configure(self.assembly, output)
        self.assertFalse(output.exists())

    def test_kernel_policy_changes_configuration_identity(self):
        before = runtime.resolve(self.assembly)
        self.assembly.write_text(self.assembly.read_text() + '[os]\nprofile="minimal"\n')
        after = runtime.resolve(self.assembly)
        self.assertNotEqual(before.configuration_sha256, after.configuration_sha256)
        self.assertEqual(before.profile.compile_options, after.profile.compile_options)

    def test_non_kernel_backends_reject_os_policy(self):
        self.assembly.write_text(self.assembly.read_text().replace('"freertos"', '"baremetal"') +
                                 '[os]\nprofile="standard"\n')
        with self.assertRaisesRegex(runtime.RuntimeError, "FreeRTOS"):
            runtime.resolve(self.assembly)

    def test_platform_assembly_resolves_same_kernel_policy(self):
        assembly = self.root / "platform.toml"
        text = (ROOT / "tools/configure/assemblies/qiming-freertos-empty.toml").read_text()
        assembly.write_text(text + '\n[os]\nprofile="minimal"\nsyscall_priority=7\n')
        result = configure.resolve_ir(assembly)
        self.assertEqual(result.kernel.profile, "minimal")
        self.assertEqual(result.irq.syscall_priority, 7)
        self.assertEqual(result.cpu_profile.irq.syscall_priority, 7)

    def test_missing_maintained_soc_mpu_fact_is_a_clear_configuration_rejection(self):
        soc = json.loads((ROOT / "soc/stm32f407/soc.json").read_text())
        soc["cpu"].pop("mpu_regions")
        with self.assertRaisesRegex(ValueError, "capabilities differ"):
            configure.validate_soc(soc, "STM32F407ZGT6")


if __name__ == "__main__":
    unittest.main()
