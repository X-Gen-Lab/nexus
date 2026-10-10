"""Maintained provider boundaries and immutable mode decisions."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import bindings
import configure as gate


class ProviderContractTests(unittest.TestCase):
    def test_unknown_family_cannot_inherit_cortex_m4_abi(self):
        soc = gate.load(gate.ROOT / "soc/stm32f407/soc.json")
        soc["id"] = "unmaintained-m4"
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "Unmaintained SoC provider"):
            gate.validate_soc(soc, "STM32F407ZGT6")

    def test_unknown_emitter_cannot_fall_back_to_native(self):
        result = gate.resolve(gate.ROOT /
                              "tools/configure/assemblies/native-empty.toml")
        result["soc_family"] = "unmaintained"
        with self.assertRaisesRegex(ValueError, "Unmaintained SoC provider"):
            bindings.emit(result)

    def test_family_contract_has_explicit_abi_and_irq_identity(self):
        from providers import maintained
        for family, arch, vector in (("native", "native", None),
                                     ("stm32f407", "cortex-m4", "Stream"),
                                     ("gd32f470", "cortex-m4", "Channel")):
            with self.subTest(family=family):
                provider = maintained(family)
                self.assertEqual(provider.CPU_ABI["arch"], arch)
                self.assertEqual(provider.DMA_VECTOR, vector)
                with self.assertRaises(TypeError):
                    provider.CPU_ABI["arch"] = "other"
        with self.assertRaisesRegex(ValueError, "Unmaintained SoC provider"):
            maintained("future")

    def test_facts_cannot_advertise_an_unimplemented_mode(self):
        from providers import maintained
        for family in ("native", "stm32f407", "gd32f470"):
            with self.subTest(family=family):
                with self.assertRaisesRegex(ValueError,
                                            "Unmaintained provider mode"):
                    maintained(family).validate_selection(
                        {"mode": "experimental"}, {"kind": "gpio"})

    def test_uart_decision_is_named_and_immutable(self):
        ir = gate.resolve_ir(gate.ROOT /
                             "tools/configure/assemblies/native-uart.toml")
        uart, = ir.controllers
        self.assertEqual(uart.options.baud, uart["baud"])
        self.assertEqual(uart.options.rx_profile, uart["rx_profile"])
        self.assertEqual(uart.options.rx_capacity, uart["rx_capacity"])
        self.assertEqual(uart.options.irq.priority, uart["irq_priority"])
        with self.assertRaises(FrozenInstanceError):
            uart.options.baud = 9600
        with self.assertRaises(FrozenInstanceError):
            uart.options.irq.priority = 0
        self.assertEqual(ir.to_dict(), gate.resolve(gate.ROOT /
                         "tools/configure/assemblies/native-uart.toml"))

    def test_adc_and_pwm_decisions_preserve_json_projection(self):
        for path, kind in (("stm32_assembly/ve-adc-scan.toml", "adc"),
                           ("gd32_assembly/adc-scan.toml", "adc"),
                           ("stm32_assembly/ve-pwm.toml", "pwm")):
            with self.subTest(fixture=path):
                ir = gate.resolve_ir(gate.ROOT / "tests/contracts" / path)
                item, = (item for item in ir.controllers if item.kind == kind)
                if kind == "adc":
                    self.assertEqual(item.options.channels,
                                     tuple(item["channels"]))
                    self.assertEqual(item.options.sample_times,
                                     tuple(item["sample_times"]))
                else:
                    self.assertEqual(item.options.tick_hz, item["tick_hz"])
                with self.assertRaises(FrozenInstanceError):
                    if kind == "adc":
                        item.options.reference_mv = 0
                    else:
                        item.options.period_ticks = 1
                self.assertEqual(ir.to_dict(), gate.resolve(
                    gate.ROOT / "tests/contracts" / path))

    def test_rejected_family_invalidates_previous_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            board = gate.load(gate.ROOT / "boards/native_reference/board.json")
            (root / "board.json").write_text(json.dumps(board))
            assembly = root / "assembly.toml"
            assembly.write_text('schema=2\nboard_package="."\n'
                                'backend="native"\nclock="model"\n'
                                '[memory]\nmain_stack_bytes=2048\n')
            output = root / "bundle"
            gate.configure(assembly, output)
            board["soc_family"] = "unmaintained"
            (root / "board.json").write_text(json.dumps(board))
            with self.assertRaisesRegex(gate.ConfigurationError,
                                        "Unmaintained SoC provider"):
                gate.configure(assembly, output)
            self.assertFalse(output.exists())

    def test_cpu_facts_are_exported_to_the_build_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "bundle"
            gate.configure(gate.ROOT /
                           "tools/configure/assemblies/native-empty.toml",
                           output)
            selection = (output / "selection.cmake").read_text()
            self.assertIn('set(NEXUS_CPU_ARCH "native")', selection)
            self.assertIn('set(NEXUS_CPU_FPU "none")', selection)
            self.assertIn('set(NEXUS_FLOAT_ABI "native")', selection)
            self.assertIn('set(NEXUS_ENUM_ABI "native-int")', selection)

    def test_arch_build_rejects_unmaintained_cpu(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.31)\nproject(probe C)\n'
                'set(NEXUS_SOC_FAMILY future)\n'
                'set(NEXUS_CPU_ARCH future-core)\n'
                'add_library(nexus_abi_options INTERFACE)\n'
                'add_library(nexus_build_options INTERFACE)\n'
                'add_library(nexus_owned_warnings INTERFACE)\n'
                f'add_subdirectory("{gate.ROOT / "arch"}" arch)\n')
            result = subprocess.run(["cmake", "-S", str(root), "-B",
                                     str(root / "build")], capture_output=True,
                                    text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Unsupported CPU architecture", result.stderr)


if __name__ == "__main__":
    unittest.main()
