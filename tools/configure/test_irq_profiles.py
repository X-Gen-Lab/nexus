"""CPU facts and interrupt policy remain one validated assembly decision."""
from tools.configure.tests.device_fixture import (load_soc_facts,
                                                   validate_soc_facts)
from tools.configure.tests.native_fixture import native_fixture, canonical_assembly
from tools.configure.providers.descriptor import source_directory
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import configure as gate
from tools.configure import selection
from tools.configure.providers import maintained


class InterruptProfileTests(unittest.TestCase):
    def test_cpu_counter_fact_matches_the_maintained_implementation(self):
        for family, part in (('native', 'NATIVE_MODEL'), ('stm32f4', 'STM32F407ZGT6')):
            for value in (1, "enabled", None, family == "native"):
                with self.subTest(family=family, value=value):
                    soc = load_soc_facts(gate.ROOT / ("tests/support/native" if family == "native"
                                               else source_directory(family)) / "soc.json")
                    soc["cpu"]["dwt_cyccnt"] = value
                    with self.assertRaisesRegex(gate.ConfigurationError,
                                                "DWT cycle counter"):
                        validate_soc_facts(soc, part, gate.maintained(
                            family, test_support=family == "native"))

    def test_irq_width_cannot_contradict_verified_soc_facts(self):
        for value in (True, 0, 9, 3):
            with self.subTest(value=value):
                soc = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
                soc["irq"]["priority_bits"] = value
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "IRQ priority bits"):
                    validate_soc_facts(soc, "STM32F407ZGT6")

    def test_unknown_interrupt_fact_is_rejected(self):
        soc = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
        soc["irq"] = {"priority_bits": 4, "syscall_priority": 1}
        with self.assertRaisesRegex(gate.ConfigurationError, "unknown"):
            validate_soc_facts(soc, "STM32F407ZGT6")

    def selected_cpu_profile(self, bits):
        from tools.configure import cpu
        provider = maintained("stm32f4")
        facts = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
        profile = cpu.resolve({**provider.CPU_FEATURES, **provider.CPU_ABI},
                              {**facts["irq"], "priority_bits": bits},
                              "freertos", enum_abi=provider.ENUM_ABI)
        return profile

    def test_kernel_profile_derives_the_irq_range_and_ceiling(self):
        provider = maintained("stm32f4")
        for bits in (3, 4, 8):
            with self.subTest(bits=bits):
                profile = self.selected_cpu_profile(bits)
                irq = selection.validate_interrupt_profile(profile, provider)
                self.assertEqual(irq.maximum_priority, (1 << bits) - 1)
                self.assertEqual(irq.syscall_priority, 10 if bits == 8 else 5)
                self.assertEqual(irq.kernel_port, "GCC/ARM_CM4F")
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "IRQ priority bits"):
            self.selected_cpu_profile(2)

    def test_kernel_profile_cannot_inherit_another_cpu_or_unknown_port(self):
        from tools.configure import cpu
        provider = maintained("stm32f4")
        m0 = dict(arch="cortex-m0", fpu="none", float_abi="soft",
                  dwt_cyccnt=False, mpu_version=0, mpu_regions=0,
                  icache_line_bytes=0, dcache_line_bytes=0,
                  security="single", sau=False, mve="none", dsp=False)
        profile = cpu.resolve(m0, dict(priority_bits=2, external_count=32),
                              "freertos")
        with self.assertRaisesRegex(gate.ConfigurationError, "kernel CPU ABI"):
            selection.validate_interrupt_profile(profile, provider)
        profile = self.selected_cpu_profile(4)
        with patch.object(type(provider), "FREERTOS_PORT", "GCC/ARM_CM0"):
            with self.assertRaisesRegex(gate.ConfigurationError,
                                        "Unmaintained FreeRTOS port"):
                selection.validate_interrupt_profile(profile, provider)

    def test_irq_policy_is_typed_immutable_and_exported_for_each_backend(self):
        for stem, backend, dwt in (('native', 'native', False), ('qiming', 'baremetal', True), ('qiming', 'freertos', True)):
            name = ("native-empty.toml" if stem == "native" else
                    f"{stem}-{backend}-empty.toml")
            source = canonical_assembly(gate.ROOT, name)
            with self.subTest(assembly=name), tempfile.TemporaryDirectory() as temp:
                ir = gate.resolve_ir(source, test_support=native_fixture(source))
                self.assertEqual(ir.irq.priority_bits, 4)
                self.assertEqual(ir.irq.maximum_priority, 15)
                self.assertEqual(ir.irq.syscall_priority,
                                 5 if backend == "freertos" else None)
                self.assertEqual(ir.cpu["dwt_cyccnt"], dwt)
                with self.assertRaises(FrozenInstanceError):
                    ir.irq.priority_bits = 2
                output = Path(temp) / "bundle"
                gate.configure(source, output, test_support=native_fixture(source))
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
            ir = gate.resolve_ir(source, test_support=native_fixture(source))
            self.assertFalse(ir.cpu["dwt_cyccnt"])

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
                        gate.configure(source, output, test_support=native_fixture(source))
                    else:
                        with self.assertRaisesRegex(gate.ConfigurationError,
                                                    "IRQ priority|syscall ceiling"):
                            gate.configure(source, output, test_support=native_fixture(source))
                        self.assertFalse(output.exists())


class PreflightIrqGeometryTests(unittest.TestCase):
    """Serializer fragments do not grant new hardware modes or exact parts."""

    def emit(self, count, vector, number):
        from types import SimpleNamespace
        from tools.configure import resource_graph
        from tools.configure.ir import InterruptBindingIR
        from tools.configure.policies.gpio_phase import GpioPhasePolicy
        facts = gate.load(gate.ROOT / "soc/stm32/stm32f4/bindings/gpio_policy.json")
        policy = GpioPhasePolicy.from_validated(facts["domains"]["gpio_phase"])
        claim = resource_graph.ResourceClaimIR(
            "irq:" + vector, "serializer-owner", "fixture",
            "serializer-input", "exclusive", (), None)
        # This is a serializer fixture, not a new selected part/provider.
        binding = SimpleNamespace(vector_bindings=lambda _irqs: (
            InterruptBindingIR(vector + "_IRQn", vector + "_IRQHandler", number),))
        selected = SimpleNamespace(
            controllers=(SimpleNamespace(id="serializer-owner", kind="crc",
                controller="CRC", integration=SimpleNamespace(dma_bindings=())),),
            provider_context=SimpleNamespace(binding_policy=binding),
            resource_plan=resource_graph.ResourcePlanIR((claim,)),
            cpu_profile=SimpleNamespace(irq=SimpleNamespace(external_count=count)))
        config = policy.admission_declarations(selected, None, 0)[-1]
        return dict(config.initializer.fields)["irq_masks"].values

    def test_existing_82_slot_profile_keeps_three_mask_words(self):
        self.assertEqual(self.emit(82, "FPU", 81), (0, 0, 1 << 17))

    def test_97_slot_profile_keeps_the_fourth_bank_vector(self):
        self.assertEqual(self.emit(97, "FMPI2C1_ER", 96), (0, 0, 0, 1))


if __name__ == "__main__":
    unittest.main()
