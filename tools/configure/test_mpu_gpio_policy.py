"""Exact normal U575 MPU GPIO selection never broadens the default ABI."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, configure, kernel, memory, runtime

ROOT = Path(__file__).resolve().parents[2]
SIZES = {"privileged_flash_bytes": 131072, "syscall_flash_bytes": 256,
         "user_flash_bytes": 2097152 - 131328,
         "privileged_ram_bytes": 163840, "user_ram_bytes": 32768}


class MpuGpioPolicyTests(unittest.TestCase):
    def cpu(self):
        return configure.resolve_ir(
            ROOT / "tools/configure/assemblies/nucleo-u575-freertos-empty.toml",
            root=ROOT).cpu_profile

    def policy(self, **changes):
        return kernel.resolve({"memory_protection": True,
                               "user_io": "gpio-write", **changes},
                              self.cpu(), 4000000)

    def assembly(self, *, os_extra="", memory_extra="", backend="freertos",
                 board="stm32u575_nucleo_zi_q", clock="msis4"):
        return (f'schema=2\nboard="{board}"\nbackend="{backend}"\n'
                f'clock="{clock}"\n[memory]\nmain_stack_bytes=2048\n'
                'libc_heap_bytes=0\nstatic_ram_limit_bytes=32768\n'
                'flash_load_limit_bytes=262144\n' + memory_extra +
                '[memory.mpu]\n' +
                ''.join(f'{key}={value}\n' for key, value in SIZES.items()) +
                '[os]\nprofile="standard"\nmemory_protection=true\n'
                'user_io="gpio-write"\n' + os_extra +
                '[gpio.led]\nbinding="led0"\nmode="output"\n')

    def resolve(self, text):
        with tempfile.TemporaryDirectory() as value:
            path = Path(value) / "assembly.toml"
            path.write_text(text)
            return configure.resolve_ir(path, root=ROOT)

    def test_selected_policy_is_typed_immutable_and_emits_only_explicit_flag(self):
        result = self.policy()
        self.assertEqual(result.user_io, "gpio-write")
        self.assertEqual(result.definitions["NEXUS_OS_MPU_GPIO"], 1)
        with self.assertRaises((AttributeError, TypeError)):
            result.user_io = "none"
        default = kernel.resolve(None, self.cpu(), 4000000)
        self.assertEqual(default.user_io, "none")
        self.assertEqual(default.definitions["NEXUS_OS_MPU_GPIO"], 0)

    def test_requires_protected_single_world_running_tick_reference(self):
        for changes in ({"memory_protection": False}, {"tickless": True},
                        {"tick_source": "external"}, {"user_io": True},
                        {"user_io": "all"}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.policy(**changes)

    def test_real_normal_ir_contains_exact_explicit_reservations(self):
        result = self.resolve(self.assembly())
        self.assertEqual({key: getattr(result.memory_budget.mpu, key) for key in SIZES}, SIZES)
        self.assertEqual(result.kernel.user_io, "gpio-write")
        script = memory.linker(result)
        for name in ("privileged_flash", "privileged_ram", "user_flash",
                     "user_ram", "syscall_flash"):
            self.assertIn("__nexus_" + name + "_start__", script)
        self.assertIn("__nx_mpu_user_data_load", script)
        self.assertIn("__nx_mpu_user_bss_end", script)

    def test_explicit_underfill_leaves_physical_tails_unmapped(self):
        text = self.assembly().replace('user_flash_bytes=1965824',
                                       'user_flash_bytes=4096')
        text = text.replace('privileged_ram_bytes=163840',
                            'privileged_ram_bytes=65536')
        text = text.replace('user_ram_bytes=32768', 'user_ram_bytes=8192')
        result = self.resolve(text)
        self.assertEqual(result.memory_budget.mpu.user_flash_bytes, 4096)
        self.assertEqual(result.memory_budget.mpu.user_ram_bytes, 8192)
        script = memory.linker(result)
        self.assertIn('USER_FLASH (rx) : ORIGIN = 0x08020100, LENGTH = 4096',
                      script)
        self.assertIn('USER_RAM (rw) : ORIGIN = 0x20010000, LENGTH = 8192',
                      script)

    def test_missing_reservations_or_unselected_profile_never_gets_implicit_carveout(self):
        text = self.assembly()
        missing = text[:text.index('[memory.mpu]')] + text[text.index('[os]'):]
        with self.assertRaisesRegex(ValueError, "MPU|protected"):
            self.resolve(missing)
        without = text.replace('user_io="gpio-write"\n', '')
        with self.assertRaisesRegex(ValueError, "MPU|protected"):
            self.resolve(without)

    def test_reservations_reject_alignment_overflow_unknown_and_incomplete(self):
        text = self.assembly()
        for before, after in (("user_ram_bytes=32768", "user_ram_bytes=32767"),
                              ("user_ram_bytes=32768", "user_ram_bytes=true"),
                              ("user_ram_bytes=32768", "user_ram_bytes=196608"),
                              ("user_ram_bytes=32768", "user_ram_bytes=4294967296"),
                              ("user_ram_bytes=32768", "other_ram_bytes=32768")):
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.resolve(text.replace(before, after))

    def test_optional_sram4_and_exhausted_privileged_stack_reject(self):
        with self.assertRaises(ValueError):
            self.resolve(self.assembly(memory_extra=
                '[memory.regions.sram4]\nlimit_bytes=256\n'))
        with self.assertRaises(ValueError):
            self.resolve(self.assembly().replace('main_stack_bytes=2048',
                                                'main_stack_bytes=163840'))

    def test_exact_reference_requires_the_single_pc7_output_binding(self):
        text = self.assembly()
        for changed in (text[:text.index('[gpio.led]')],
                        text.replace('mode="output"', 'mode="input"')):
            with self.subTest(text=changed), self.assertRaisesRegex(
                    ValueError, "GPIO|gpio"):
                self.resolve(changed)

    def test_m7_remains_a_real_negative_not_a_fabricated_mpu_port(self):
        with self.assertRaisesRegex(ValueError, "MPU|GPIO"):
            self.resolve(self.assembly(board="stm32h743_nucleo_zi2",
                                       clock="hsi64-fixed-cache-off"))

    def test_cpu_only_runtime_cannot_claim_the_physical_gateway_profile(self):
        resolved = configure.resolve_ir(
            ROOT / "tools/configure/assemblies/nucleo-u575-freertos-empty.toml",
            root=ROOT)
        with tempfile.TemporaryDirectory() as value:
            directory = Path(value)
            (directory / "cpu.json").write_text(json.dumps({
                "schema_version": 1, "cpu": dict(resolved.cpu),
                "irq": {key: getattr(resolved.irq, key)
                        for key in ("priority_bits", "external_count")},
                "clock_hz": 4000000}))
            assembly = directory / "runtime.toml"
            assembly.write_text('schema_version=1\ncpu_facts="cpu.json"\n'
                'backend="freertos"\noptimization="Os"\n'
                '[os]\nmemory_protection=true\nuser_io="gpio-write"\n')
            with self.assertRaisesRegex(runtime.RuntimeError, "GPIO|platform"):
                runtime.resolve(assembly)

    def test_editor_schema_describes_the_same_explicit_closed_fields(self):
        schema = authored.editor_schema()
        mpu = schema["properties"]["memory"]["properties"]["mpu"]
        self.assertEqual(set(mpu["required"]), set(SIZES))
        self.assertFalse(mpu["additionalProperties"])
        for value in mpu["properties"].values():
            self.assertEqual(value["multipleOf"], 32)
        props = kernel.editor_schema()["properties"]
        self.assertEqual(props["user_io"]["enum"], ["none", "gpio-write"])


if __name__ == "__main__":
    unittest.main()
