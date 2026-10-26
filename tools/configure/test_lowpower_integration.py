"""SoC-backed lowpower selector never silently binds a CPU-only timer port."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import cpu
from tools.configure import kernel
from tools.configure import runtime
from tools.configure.test_cpu_profiles import facts, irq


class LowpowerIntegrationTests(unittest.TestCase):
    def profile(self):
        return cpu.resolve(facts("cortex-m4"), irq("cortex-m4"), "freertos")

    def test_default_custom_port_preserves_existing_weak_null_contract(self):
        policy = kernel.resolve(None, self.profile(), 168000000)
        self.assertEqual(policy.lowpower_port, "custom")
        self.assertEqual(policy.definitions["NEXUS_OS_TIMEBASE_LOWPOWER"], 0)
        custom = kernel.resolve({"profile": "lowpower"}, self.profile(), 168000000)
        self.assertEqual(custom.lowpower_port, "custom")
        self.assertTrue(custom.tickless)

    def test_timebase_port_is_explicit_and_emits_one_selector(self):
        policy = kernel.resolve({"profile": "lowpower", "lowpower_port": "timebase"},
                                self.profile(), 168000000)
        self.assertEqual(policy.lowpower_port, "timebase")
        self.assertEqual(policy.definitions["NEXUS_OS_TIMEBASE_LOWPOWER"], 1)
        self.assertIn("#define NEXUS_OS_TIMEBASE_LOWPOWER 1u", kernel.header_lines(policy))

    def test_selector_type_unknown_value_and_incompatible_policy_are_rejected(self):
        for choices in ({"lowpower_port": True}, {"lowpower_port": "guess"},
                        {"lowpower_port": "timebase"},
                        {"lowpower_port": "timebase", "profile": "lowpower", "tick_source": "external"}):
            with self.subTest(choices=choices), self.assertRaises(ValueError):
                kernel.resolve(choices, self.profile(), 168000000)

    def test_editor_selector_uses_same_explicit_fields(self):
        self.assertEqual(kernel.editor_schema()["properties"]["lowpower_port"],
                         {"enum": ["custom", "timebase"]})

    def test_cpu_only_runtime_rejects_missing_soc_hardware_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "cpu.json").write_text(json.dumps({
                "schema_version": 1, "cpu": facts("cortex-m4"),
                "irq": irq("cortex-m4"), "clock_hz": 168000000}))
            assembly = root / "runtime.toml"
            assembly.write_text('schema_version = 1\ncpu_facts = "cpu.json"\n'
                                'backend = "freertos"\noptimization = "Os"\n'
                                '[os]\nprofile = "lowpower"\nlowpower_port = "timebase"\n')
            output = root / "generated"
            with self.assertRaisesRegex(runtime.RuntimeError, "CPU-only.*SoC"):
                runtime.configure(assembly, output)
            self.assertFalse(output.exists())

    def test_cpu_emitter_cannot_publish_a_soc_port_without_soc_facts(self):
        profile = self.profile()
        policy = kernel.resolve({"profile": "lowpower", "lowpower_port": "timebase"},
                                profile, 168000000)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "CPU-only.*SoC"):
                cpu.emit_profile(profile, Path(directory), clock_hz=168000000,
                                 inputhash="0" * 64, kernel_profile=policy)
            self.assertFalse(list(Path(directory).iterdir()))
