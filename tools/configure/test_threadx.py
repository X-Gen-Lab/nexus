"""ThreadX policy is strict, immutable and independent of FreeRTOS options."""
import importlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import cpu
from tools.configure import kernel
from tools.configure import runtime
from tools.configure.test_cpu_profiles import facts, irq


ROOT = Path(__file__).resolve().parents[2]


class ThreadXPolicyTests(unittest.TestCase):
    def profile(self, arch="cortex-m4", *, fpu="none", abi=None):
        options = facts(arch, fpu=fpu)
        interrupts = irq(arch)
        if arch == "native":
            options["float_abi"] = "native"
            interrupts["external_count"] = 0
        elif fpu != "none":
            options["float_abi"] = abi or "hard"
        return cpu.resolve(options, interrupts, "threadx")

    def resolve(self, choices=None, *, arch="cortex-m4", hz=168000000):
        return kernel.resolve(choices, self.profile(arch), hz)

    def test_native_and_actual_m4_gnu_tuples_are_distinct(self):
        native = self.profile("native")
        self.assertEqual(native.irq.kernel_port, "linux/gnu")
        self.assertEqual(native.definitions["NEXUS_IRQ_KERNEL_POLICY"], 0)
        self.assertFalse(native.compile_options)
        for fpu, abi in (("none", "soft"), ("fpv4-sp-d16", "hard"),
                         ("fpv4-sp-d16", "softfp")):
            with self.subTest(fpu=fpu, abi=abi):
                profile = self.profile(fpu=fpu, abi=abi)
                self.assertEqual(profile.irq.kernel_port, "cortex_m4/gnu")
                self.assertIn("-mcpu=cortex-m4", profile.compile_options)
                self.assertEqual(profile.float_abi, abi)

    def test_other_cores_remain_arch_contracts_not_threadx_ports(self):
        for arch in ("cortex-m0", "cortex-m0plus", "cortex-m3", "cortex-m7",
                     "cortex-m23", "cortex-m33", "cortex-m55", "cortex-m85"):
            with self.subTest(arch=arch), self.assertRaisesRegex(
                    ValueError, "ThreadX.*port"):
                self.profile(arch)

    def test_default_m4_policy_uses_real_static_port_facts(self):
        policy = self.resolve()
        self.assertEqual(policy.backend, "threadx")
        self.assertEqual(policy.profile, "static")
        self.assertEqual(policy.max_priorities, 32)
        self.assertEqual(policy.min_stack_bytes, 200)
        self.assertEqual(policy.stack_alignment_bytes, 8)
        self.assertTrue(policy.timer_process_in_isr)
        self.assertEqual(policy.tick_source, "systick")
        self.assertEqual(policy.tick_reload, 167999)
        self.assertEqual(policy.interrupt_mask, "basepri")
        self.assertEqual(policy.syscall_priority, 5)
        self.assertNotIn("idle_stack_words", policy)
        self.assertNotIn("memory_protection", policy)

    def test_native_clock_is_explicit_host_monotonic_not_fake_systick(self):
        policy = self.resolve(arch="native", hz=1)
        self.assertEqual(policy.tick_source, "host")
        self.assertIsNone(policy.tick_reload)
        self.assertEqual(policy.interrupt_mask, "none")
        self.assertEqual(policy.syscall_priority, 0)
        self.assertEqual(policy.definitions["NEXUS_IRQ_KERNEL_POLICY"], 0)
        for source in ("systick", "external"):
            with self.subTest(source=source), self.assertRaisesRegex(
                    ValueError, "ThreadX.*host"):
                self.resolve({"tick_source": source}, arch="native")
        with self.assertRaisesRegex(ValueError, "nanosecond"):
            self.resolve({"tick_hz": 333}, arch="native")

    def test_profile_and_freertos_feature_fields_are_rejected(self):
        for name, value in (("profile", "minimal"), ("tickless", True),
                            ("memory_protection", True), ("idle_stack_words", 128),
                            ("system_call_stack_words", 128),
                            ("secure_idle_stack_bytes", 128), ("security_model", "split"),
                            ("notification_slots", 1), ("runtime_stats", True),
                            ("timer_process_in_isr", False), ("min_stack_bytes", 1)):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "unknown"):
                self.resolve({name: value})

    def test_priority_count_and_numeric_types_fail_closed(self):
        for name, values in {"max_priorities": (1, 31, 64, True),
                             "tick_hz": (0, 1000001, True),
                             "syscall_priority": (0, 16, True)}.items():
            for value in values:
                with self.subTest(name=name, value=value), self.assertRaises(ValueError):
                    self.resolve({name: value})

    def test_systick_requires_exact_reviewed_clock_and_reload(self):
        for hz in (999, 1000, 1000001, 0xffffffff):
            with self.subTest(hz=hz), self.assertRaisesRegex(ValueError, "SysTick"):
                self.resolve(hz=hz)
        self.assertEqual(self.resolve(hz=2000).tick_reload, 1)

    def test_external_tick_has_no_implicit_hardware_or_truncation(self):
        policy = self.resolve({"tick_source": "external", "tick_hz": 333}, hz=1)
        self.assertIsNone(policy.tick_reload)
        self.assertEqual(policy.definitions["NEXUS_OS_EXTERNAL_TICK"], 1)
        with self.assertRaisesRegex(ValueError, "tick source"):
            self.resolve({"tick_source": "host"})

    def test_primask_policy_does_not_remove_physical_basepri_capability(self):
        physical = self.profile()
        policy = kernel.resolve({"interrupt_mask": "primask"}, physical, 168000000)
        bound = kernel.bind(physical, policy)
        self.assertTrue(bound.has_basepri)
        self.assertEqual(bound.irq.mask_kind, "primask")
        self.assertEqual(bound.irq.syscall_priority, 0)
        self.assertEqual(bound.definitions["NEXUS_IRQ_KERNEL_POLICY"], 1)
        self.assertEqual(physical.irq.mask_kind, "basepri")
        self.assertEqual(physical.irq.syscall_priority, 5)
        with self.assertRaisesRegex(ValueError, "syscall"):
            self.resolve({"interrupt_mask": "primask", "syscall_priority": 1})
        with self.assertRaisesRegex(ValueError, "interrupt mask"):
            self.resolve({"interrupt_mask": "none"})

    def test_native_cannot_claim_interrupt_mask_or_syscall_ceiling(self):
        for choice in ({"interrupt_mask": "basepri"}, {"interrupt_mask": "primask"},
                       {"syscall_priority": 1}):
            with self.subTest(choice=choice), self.assertRaises(ValueError):
                self.resolve(choice, arch="native")

    def test_policy_and_macros_are_immutable(self):
        policy = self.resolve()
        with self.assertRaises((AttributeError, TypeError)):
            policy.tick_hz = 1
        with self.assertRaises(TypeError):
            policy.definitions["NEXUS_OS_TICK_HZ"] = 1

    def test_roundtrip_dispatch_retains_threadx_type_and_identity(self):
        policy = self.resolve()
        restored = kernel.from_validated(policy.to_dict())
        self.assertEqual(restored, policy)
        self.assertEqual(restored.backend, "threadx")
        self.assertNotIn("idle_stack_words", restored)

    def test_generated_policy_has_exact_threadx_contracts(self):
        policy = self.resolve({"syscall_priority": 6})
        bound = kernel.bind(self.profile(), policy)
        text = "\n".join(cpu.header_lines(bound) + kernel.header_lines(policy))
        self.assertIn("#define NEXUS_OS_MIN_STACK_BYTES 200u", text)
        self.assertIn("#define NEXUS_IRQ_SYSCALL_PRIORITY 6u", text)
        self.assertIn("#define NEXUS_IRQ_KERNEL_POLICY 2u", text)
        self.assertNotIn("NEXUS_OS_IDLE_STACK_WORDS", text)
        selected = "\n".join(cpu.selection_lines(bound) + kernel.selection_lines(policy))
        self.assertIn('set(NEXUS_THREADX_PORT "cortex_m4/gnu")', selected)
        self.assertIn('set(NEXUS_FREERTOS_PORT "")', selected)

    def test_editor_schema_is_backend_specific(self):
        module = importlib.import_module("tools.configure.threadx")
        properties = module.editor_schema()["properties"]
        self.assertEqual(properties["max_priorities"], {"const": 32})
        self.assertNotIn("profile", properties)
        self.assertNotIn("tickless", properties)
        self.assertFalse(module.editor_schema()["additionalProperties"])

    def test_platform_scope_does_not_turn_arbitrary_board_into_support(self):
        module = importlib.import_module("tools.configure.threadx")
        for directory, family in (("native_reference", "native"),
                                  ("stm32f407_qiming_v31", "stm32f407"),
                                  ("stm32f407ve_sky_qingchun", "stm32f407"),
                                  ("gd32f470_liangshan", "gd32f470")):
            board = json.loads((ROOT / "boards" / directory / "board.json").read_text())["id"]
            module.validate_platform(board, family)
        for board, family in (("custom_unknown", "stm32f407"),
                              ("stm32f407_qiming_v31", "stm32f407"),
                              ("stm32f407zg-qiming-v31", "gd32f470")):
            with self.subTest(board=board), self.assertRaisesRegex(ValueError, "ThreadX.*platform"):
                module.validate_platform(board, family)

    def test_cpu_runtime_binds_host_policy_into_atomic_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            soc = json.loads((ROOT / "soc/native/soc.json").read_text())
            (root / "cpu.json").write_text(json.dumps({
                "schema_version": 1, "cpu": soc["cpu"], "irq": soc["irq"],
                "clock_hz": 1000000}))
            assembly = root / "runtime.toml"
            assembly.write_text('schema_version = 1\ncpu_facts = "cpu.json"\n'
                                'backend = "threadx"\noptimization = "Os"\n')
            output = root / "generated"
            resolved = runtime.configure(assembly, output)
            self.assertEqual(resolved.kernel.backend, "threadx")
            header = (output / "nexus_config.h").read_text()
            self.assertIn("#define NEXUS_BACKEND_THREADX 1", header)
            self.assertIn("#define NEXUS_OS_TICK_HZ 1000u", header)
            self.assertNotIn("NEXUS_OS_IDLE_STACK_WORDS", header)
            self.assertIn('set(NEXUS_THREADX_PORT "linux/gnu")',
                          (output / "selection.cmake").read_text())
