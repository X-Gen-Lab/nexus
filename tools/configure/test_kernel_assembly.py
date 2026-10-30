"""Backend-specific assembly policy, resource claims and emitted contracts."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import authored, configure as gate


class KernelAssemblyTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def assembly(self, *, board="stm32f407_qiming_v31", backend="threadx",
                 options="", uart=True):
        clock = {"native_reference": "model",
                 "gd32f470_liangshan": "hxtal25-pll200"}.get(board, "hse8-pll168")
        content = (f'schema=2\nboard="{board}"\nbackend="{backend}"\n'
                   f'clock="{clock}"\n[memory]\nmain_stack_bytes=2048\n')
        if uart:
            binding = "console" if board == "gd32f470_liangshan" else "uart1"
            content += (f'[uart.link]\nbinding="{binding}"\nmode="irq"\n'
                        'baud=115200\nrx={format="events",capacity=16}\n'
                        'irq={priority=5,calls_os=true}\n')
        content += "[os]\n" + options
        path = self.root / "assembly.toml"
        path.write_text(content)
        return path

    def resolve(self, path):
        # This suite observes the checkout boundary; real locked source checkout
        # and ARM linking are separate qualification, never implied by this stub.
        identity = gate.dependency_identity
        def dependency(root, relative, expected=None):
            if relative == "ext/threadx":
                return {"path": relative, "commit": "1" * 40}
            return identity(root, relative, expected)
        with patch.object(gate, "dependency_identity", side_effect=dependency):
            return gate.resolve_ir(path)

    def test_threadx_assembly_retains_distinct_type_claims_and_source_identity(self):
        for board in ("stm32f407_qiming_v31", "stm32f407ve_sky_qingchun",
                      "gd32f470_liangshan"):
            with self.subTest(board=board):
                result = self.resolve(self.assembly(board=board))
                self.assertEqual(result.kernel.backend, "threadx")
                self.assertFalse(hasattr(result.kernel, "idle_stack_words"))
                claims = {item.key for item in result.claims}
                self.assertIn("irq:PendSV", claims)
                self.assertIn("irq:SysTick", claims)
                self.assertNotIn("irq:SVC", claims)
                self.assertIn("ext/threadx", {item["path"] for item in
                                               result.source_dependencies})

    def test_threadx_native_is_host_architecture_with_no_fake_exceptions(self):
        result = self.resolve(self.assembly(board="native_reference", uart=False))
        self.assertEqual(result.cpu_profile.arch, "native")
        self.assertEqual(result.kernel.tick_source, "host")
        self.assertFalse(any(item.key.startswith("irq:") for item in result.claims))

    def test_external_threadx_tick_does_not_claim_systick(self):
        result = self.resolve(self.assembly(options='tick_source="external"\n'))
        claims = {item.key for item in result.claims}
        self.assertIn("irq:PendSV", claims)
        self.assertNotIn("irq:SysTick", claims)

    def test_emitted_threadx_bundle_uses_own_policy_not_freertos_fields(self):
        result = self.resolve(self.assembly(options='interrupt_mask="primask"\n'))
        output = self.root / "generated"
        output.mkdir()
        gate.generate(result, output)
        header = (output / "nexus_config.h").read_text()
        self.assertIn("#define NEXUS_BACKEND_THREADX 1", header)
        self.assertIn("#define NEXUS_OS_MIN_STACK_BYTES 200u", header)
        self.assertIn("#define NEXUS_IRQ_KERNEL_POLICY 1u", header)
        self.assertNotIn("NEXUS_OS_IDLE_STACK_WORDS", header)
        self.assertIn('set(NEXUS_THREADX_PORT "cortex_m4/gnu")',
                      (output / "selection.cmake").read_text())

    def test_unknown_threadx_board_is_not_promoted_by_cpu_port(self):
        path = self.assembly(uart=False)
        board = gate.load(gate.ROOT / "boards/stm32f407_qiming_v31/board.json")
        board["id"] = "unknown-board"
        package = self.root / "board"
        package.mkdir()
        (package / "board.json").write_text(json.dumps(board))
        path.write_text(path.read_text().replace('board="stm32f407_qiming_v31"',
                                                'board_package="board"'))
        with self.assertRaisesRegex(ValueError, "ThreadX.*platform"):
            self.resolve(path)

    def test_os_isr_ceiling_uses_selected_threadx_mask_policy(self):
        path = self.assembly(options='syscall_priority=6\n')
        with self.assertRaisesRegex(ValueError, "syscall ceiling"):
            self.resolve(path)

    def test_timebase_lowpower_requires_actual_fixed_mcu_alarm_family(self):
        path = self.assembly(backend="freertos", uart=False,
                             options='tickless=true\nlowpower_port="timebase"\n')
        result = self.resolve(path)
        self.assertEqual(result.kernel.lowpower_port, "timebase")
        self.assertEqual(result.kernel.definitions["NEXUS_OS_TIMEBASE_LOWPOWER"], 1)
        path = self.assembly(backend="freertos", board="native_reference", uart=False,
                             options='tickless=true\nlowpower_port="timebase"\n')
        with self.assertRaisesRegex(ValueError, "host model"):
            self.resolve(path)

    def test_editor_schema_keeps_backend_specific_os_fields(self):
        schema = authored.editor_schema()
        self.assertIn("threadx", schema["properties"]["backend"]["enum"])
        policies = schema["properties"]["os"]["anyOf"]
        self.assertEqual(len(policies), 2)
        self.assertTrue(any("interrupt_mask" in policy["properties"] and
                            "profile" not in policy["properties"] for policy in policies))
        self.assertTrue(any("lowpower_port" in policy["properties"] for policy in policies))
        selected = schema["allOf"]
        self.assertTrue(any(rule["if"]["properties"]["backend"] == {"const": "threadx"}
                            for rule in selected))

    def test_can_cold_policy_uses_threadx_selected_mask_without_backend_guess(self):
        source = (gate.ROOT / "tests/contracts/can_assembly/stm32-first.toml").read_text()
        path = self.root / "can.toml"
        path.write_text(source.replace('backend="baremetal"', 'backend="threadx"') +
                        '\n[os]\ninterrupt_mask="primask"\n')
        result = self.resolve(path)
        output = self.root / "can-generated"
        output.mkdir()
        gate.generate(result, output)
        self.assertIn(".irq_policy = { NX_IRQ_KERNEL_PRIMASK, 82u, 4u, 0u }",
                      (output / "bindings.c").read_text())

    def test_baremetal_physical_basepri_is_not_a_configured_kernel_policy(self):
        source = gate.ROOT / "tests/contracts/can_assembly/stm32-first.toml"
        result = gate.resolve_ir(source)
        self.assertEqual(result.cpu_profile.irq.mask_kind, "basepri")
        self.assertIsNone(result.kernel)
        output = self.root / "can-baremetal"
        output.mkdir()
        gate.generate(result, output)
        self.assertIn(".irq_policy = { NX_IRQ_KERNEL_NONE, 82u, 4u, 0u }",
                      (output / "bindings.c").read_text())


if __name__ == "__main__":
    unittest.main()
