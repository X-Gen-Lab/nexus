"""CPU facts and interrupt policy remain one validated assembly decision."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import configure as gate
from providers import common, maintained


class InterruptProfileTests(unittest.TestCase):
    def test_cpu_counter_fact_matches_the_maintained_implementation(self):
        for family, part in (("native", "NATIVE_MODEL"),
                             ("stm32f407", "STM32F407ZGT6"),
                             ("gd32f470", "GD32F470ZGT6")):
            for value in (1, "enabled", None, family == "native"):
                with self.subTest(family=family, value=value):
                    soc = gate.load(gate.ROOT / "soc" / family / "soc.json")
                    soc["cpu"]["dwt_cyccnt"] = value
                    with self.assertRaisesRegex(gate.ConfigurationError,
                                                "DWT cycle counter"):
                        gate.validate_soc(soc, part)

    def test_irq_width_cannot_contradict_verified_soc_facts(self):
        for value in (True, 0, 9, 3):
            with self.subTest(value=value):
                soc = gate.load(gate.ROOT / "soc/stm32f407/soc.json")
                soc["irq"]["priority_bits"] = value
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "IRQ priority bits"):
                    gate.validate_soc(soc, "STM32F407ZGT6")

    def test_unknown_interrupt_fact_is_rejected(self):
        soc = gate.load(gate.ROOT / "soc/stm32f407/soc.json")
        soc["irq"] = {"priority_bits": 4, "syscall_priority": 1}
        with self.assertRaisesRegex(gate.ConfigurationError, "unknown"):
            gate.validate_soc(soc, "STM32F407ZGT6")

    def test_kernel_profile_derives_the_irq_range_and_ceiling(self):
        provider = maintained("stm32f407")
        for bits in (3, 4, 8):
            with self.subTest(bits=bits):
                irq = common.resolve_interrupts(provider.CPU_ABI,
                                                 {"priority_bits": bits},
                                                 "freertos", provider)
                self.assertEqual(irq["maximum_priority"], (1 << bits) - 1)
                self.assertEqual(irq["syscall_priority"], 10 if bits == 8 else 5)
                self.assertEqual(irq["kernel_port"], "GCC/ARM_CM4F")
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "IRQ priority bits"):
            common.resolve_interrupts(provider.CPU_ABI,
                                      {"priority_bits": 2}, "freertos", provider)

    def test_kernel_profile_cannot_inherit_another_cpu_or_unknown_port(self):
        provider = maintained("stm32f407")
        with self.assertRaisesRegex(gate.ConfigurationError, "kernel CPU ABI"):
            common.resolve_interrupts({"arch": "cortex-m0", "fpu": "none",
                                       "float_abi": "soft"},
                                      {"priority_bits": 4}, "freertos", provider)
        with patch.object(provider, "FREERTOS_PORT", "GCC/ARM_CM0"):
            with self.assertRaisesRegex(gate.ConfigurationError,
                                        "Unmaintained FreeRTOS port"):
                common.resolve_interrupts(provider.CPU_ABI,
                                          {"priority_bits": 4},
                                          "freertos", provider)

    def test_irq_policy_is_typed_immutable_and_exported_for_each_backend(self):
        for stem, backend, dwt in (("native", "native", False),
                                   ("qiming", "baremetal", True),
                                   ("qiming", "freertos", True),
                                   ("liangshan", "freertos", True)):
            name = ("native-empty.toml" if stem == "native" else
                    f"{stem}-{backend}-empty.toml")
            source = gate.ROOT / "tools/configure/assemblies" / name
            with self.subTest(assembly=name), tempfile.TemporaryDirectory() as temp:
                ir = gate.resolve_ir(source)
                self.assertEqual(ir.irq.priority_bits, 4)
                self.assertEqual(ir.irq.maximum_priority, 15)
                self.assertEqual(ir.irq.syscall_priority,
                                 5 if backend == "freertos" else None)
                self.assertEqual(ir["cpu"]["dwt_cyccnt"], dwt)
                with self.assertRaises(FrozenInstanceError):
                    ir.irq.priority_bits = 2
                output = Path(temp) / "bundle"
                gate.configure(source, output)
                selection = (output / "selection.cmake").read_text()
                header = (output / "nexus_config.h").read_text()
                self.assertIn(f'set(NEXUS_ARCH_HAS_DWT_CYCCNT "{int(dwt)}")',
                              selection)
                self.assertIn('#define NEXUS_IRQ_PRIORITY_BITS 4u', header)
                if backend == "freertos":
                    self.assertIn('set(NEXUS_FREERTOS_PORT "GCC/ARM_CM4F")',
                                  selection)
                    self.assertIn('#define NEXUS_IRQ_SYSCALL_PRIORITY 5u', header)
                else:
                    self.assertNotIn('NEXUS_IRQ_SYSCALL_PRIORITY', header)

    def test_abi_assertion_remains_separate_from_cpu_capability_facts(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "assembly.toml"
            source.write_text('schema=2\nboard="native_reference"\n'
                              'backend="native"\nclock="model"\n'
                              '[abi]\narch="native"\nfpu="none"\n'
                              'float_abi="native"\n'
                              '[memory]\nmain_stack_bytes=2048\n')
            ir = gate.resolve_ir(source)
            self.assertFalse(ir["cpu"]["dwt_cyccnt"])

    def test_selected_irq_checks_fact_range_and_os_policy_before_generation(self):
        original = gate.ROOT / "tools/configure/assemblies/qiming-freertos-uart.toml"
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "assembly.toml"
            for priority, calls_os, accepted in ((0, False, True),
                                                 (15, False, True),
                                                 (16, False, False),
                                                 (4, True, False),
                                                 (5, True, True)):
                with self.subTest(priority=priority, calls_os=calls_os):
                    source.write_text(original.read_text().replace(
                        'priority = 5', f'priority = {priority}\n'
                        f'calls_os = {str(calls_os).lower()}'))
                    output = Path(temp) / "bundle"
                    if accepted:
                        gate.configure(source, output)
                    else:
                        with self.assertRaisesRegex(gate.ConfigurationError,
                                                    "IRQ priority|syscall ceiling"):
                            gate.configure(source, output)
                        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
