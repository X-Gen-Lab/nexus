"""Maintained provider boundaries and immutable mode decisions."""
from tools.configure.tests.device_fixture import (load_soc_facts,
                                                   validate_soc_facts)
from tools.configure.tests.native_fixture import native_fixture
from tools.configure.providers.descriptor import source_directory
from dataclasses import FrozenInstanceError, replace
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.configure.providers import descriptor, maintained
from tools.configure import bindings
from tools.configure import configure as gate
from tools.configure import selection as selection_rules


class ProviderContractTests(unittest.TestCase):
    def test_unselected_mode_cannot_invent_a_software_capability(self):
        for family in ("native", "stm32f4"):
            with self.subTest(family=family):
                soc = load_soc_facts(gate.ROOT / descriptor.maintained(family, test_support=(family == "native")).source_directory / "soc.json")
                part = next(iter(soc["variants"]))
                controller = next(item for item in soc["controllers"].values()
                                  if item["kind"] == "gpio")
                controller["modes"].append("invented-unselected-mode")
                with self.assertRaisesRegex(ValueError, "provider mode|mode"):
                    validate_soc_facts(soc, part, maintained(family, test_support=(family == "native")))

    def test_mcu_clock_facts_must_match_the_actual_fixed_startup_profile(self):
        for family, part, profile in (('stm32f4', 'STM32F407ZGT6', 'hse8-pll168'),):
            for field, value in (('core_hz', 1), ('apb1_hz', 1),
                                 ('apb2_hz', 1), ('apb1_hz', False),
                                 ('apb2_hz', 100000000.0),
                                 ('profile_alias', 'unimplemented-clock')):
                with self.subTest(family=family, field=field, value=value):
                    soc = load_soc_facts(gate.ROOT / f'{source_directory(family)}/soc.json')
                    if field == 'profile_alias':
                        soc['clock_profiles'][value] = dict(
                            soc['clock_profiles'][profile])
                    else:
                        soc['clock_profiles'][profile][field] = value
                    with self.assertRaisesRegex(ValueError, '(?i)clock'):
                        validate_soc_facts(soc, part, maintained(family, test_support=(family == "native")))

    def test_physical_key_normalization_has_no_silicon_topology(self):
        self.assertEqual(
            gate.normalize_resource("dma:GPDMA3:stream31:channel255"),
            "dma:GPDMA3:stream31")
        self.assertEqual(gate.normalize_resource("dma:DMA3:stream31:channel9"),
                         "dma:DMA3:stream31")

    def test_selected_provider_owns_dma_range_and_selector_checks(self):
        from tools.configure.providers import maintained
        for family in ("stm32f4",):
            provider = maintained(family, test_support=(family == "native"))
            provider.validate_dma_resource("dma:DMA1:stream7:channel7")
            for invalid in ("dma:DMA3:stream7", "dma:DMA1:stream8",
                            "dma:DMA1:stream0:channel8"):
                with self.subTest(family=family, resource=invalid):
                    with self.assertRaisesRegex(ValueError, "topology"):
                        provider.validate_dma_resource(invalid)
        with self.assertRaisesRegex(ValueError, "topology"):
            maintained("native", test_support=True).validate_dma_resource("dma:DMA1:stream0")

    def test_physical_dma_controller_names_follow_f4_topology(self):
        from tools.configure.providers import maintained
        stm = maintained("stm32f4", test_support=False)
        stm.validate_dma_resource("dma:DMA2:stream7:channel4")
        with self.assertRaisesRegex(ValueError, "topology"):
            stm.validate_dma_resource("dma:DMA0:stream0:channel0")

    def test_adc_scan_and_single_shot_have_distinct_channel_contracts(self):
        from tools.configure.providers import common, maintained
        for family in ("stm32f4",):
            provider = maintained(family, test_support=(family == "native"))
            for mode, channels in (("low-rate-scan", [0]),
                                   ("single-shot", [0, 1])):
                selection = {"id": "adc", "binding": "adc", "mode": mode,
                             "channels": channels,
                             "sample_times": [7] * len(channels),
                             "reference_mv": 3300, "timeout_ms": 100}
                route = {"kind": "adc", "controller": "ADC1", "pins": [
                    {"pin": f"PA{channel}"} for channel in channels]}
                with self.subTest(family=family, mode=mode):
                    with self.assertRaisesRegex(ValueError, "channel count"):
                        selection_rules.validate_selection(selection, route, provider,
                                                  131072)

    def test_adc_constructor_sequences_are_validated_before_generation(self):
        from tools.configure.providers import common, maintained
        selection = {"id": "adc", "binding": "adc", "mode": "low-rate-scan",
                     "channels": [1, 0], "sample_times": [7, 7],
                     "reference_mv": 3300, "timeout_ms": 100}
        route = {"kind": "adc", "controller": "ADC1",
                 "pins": [{"pin": "PA1"}, {"pin": "PA0"}]}
        for family in ("stm32f4",):
            with self.subTest(family=family):
                selection_rules.validate_selection(
                    selection, route, maintained(family), 131072)
                selection["channels"] = [1, 2]
                with self.assertRaisesRegex(ValueError, "channel sequence"):
                    selection_rules.validate_selection(selection, route,
                                              maintained(family, test_support=(family == "native")), 131072)

    def test_mode_traits_are_explicit_immutable_and_directional(self):
        from tools.configure.providers import common
        for kind, mode, tx, rx, continuous in (
                ("uart", "dma-tx", True, False, False),
                ("uart", "rx-dma-idle", False, True, False),
                ("uart", "dma-duplex", True, True, False),
                ("uart", "rx-dma-continuous", False, True, True),
                ("uart", "dma-duplex-continuous", True, True, True),
                ("adc", "trigger-dma", False, True, False),
                ("adc", "trigger-dma-continuous", False, True, True)):
            with self.subTest(kind=kind, mode=mode):
                traits = common.mode_traits(kind, mode)
                self.assertTrue(traits.dma)
                self.assertTrue(traits.irq)
                self.assertEqual(traits.tx_dma, tx)
                self.assertEqual(traits.rx_dma, rx)
                self.assertEqual(traits.continuous, continuous)
                with self.assertRaises(FrozenInstanceError):
                    traits.tx_dma = False
        self.assertFalse(common.mode_traits("spi", "short-poll").irq)
        with self.assertRaisesRegex(ValueError, "mode"):
            common.mode_traits("uart", "dma-unimplemented")

    def test_unknown_family_cannot_inherit_cortex_m4_abi(self):
        soc = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
        soc["id"] = "unmaintained-m4"
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "Unmaintained SoC integration (?:descriptor|provider)"):
            validate_soc_facts(soc, "STM32F407ZGT6")

    def test_unknown_emitter_cannot_fall_back_to_native(self):
        result = gate.resolve_ir(gate.ROOT /
                              "tests/support/native/assemblies/native-empty.toml", test_support=True)
        result = replace(result, soc_family="unmaintained")
        with self.assertRaisesRegex(ValueError, "Selected provider context differs"):
            bindings.emit(result)

    def test_family_contract_has_explicit_abi_and_irq_identity(self):
        from tools.configure.providers import maintained
        for family, arch, vector in (("native", "native", None), ("stm32f4", "cortex-m4", "Stream"),):
            with self.subTest(family=family):
                provider = maintained(family, test_support=(family == "native"))
                self.assertEqual(provider.CPU_ABI["arch"], arch)
                self.assertEqual(provider.DMA_VECTOR, vector)
                # ABI inspection may return a fresh projection. Mutating it
                # never changes the selected immutable source facts.
                abi = provider.CPU_ABI
                try:
                    abi["arch"] = "other"
                except TypeError:
                    pass
                self.assertEqual(provider.CPU_ABI["arch"], arch)
                if hasattr(provider, "soc"):
                    with self.assertRaises(TypeError):
                        provider.soc["cpu"]["arch"] = "other"
        with self.assertRaisesRegex(ValueError, "Unmaintained SoC integration (?:descriptor|provider)"):
            maintained("future", test_support=False)

    def test_facts_cannot_advertise_an_unimplemented_mode(self):
        from tools.configure.providers import maintained
        for family in ("native", "stm32f4"):
            with self.subTest(family=family):
                with self.assertRaisesRegex(ValueError,
                                            "provider mode|package binding"):
                    maintained(family, test_support=(family == "native")).validate_selection(
                        {"mode": "experimental"}, {"kind": "gpio", "controller": "GPIOA"})

    def test_uart_decision_is_named_and_immutable(self):
        ir = gate.resolve_ir(gate.ROOT /
                             "tests/support/native/assemblies/native-uart.toml", test_support=True)
        uart, = ir.controllers
        self.assertEqual(uart.options.baud, uart.to_dict()["baud"])
        self.assertEqual(uart.options.rx_profile, uart.to_dict()["rx_profile"])
        self.assertEqual(uart.options.rx_capacity, uart.to_dict()["rx_capacity"])
        self.assertEqual(uart.options.irq.priority, uart.to_dict()["irq_priority"])
        with self.assertRaises(FrozenInstanceError):
            uart.options.baud = 9600
        with self.assertRaises(FrozenInstanceError):
            uart.options.irq.priority = 0
        self.assertEqual(ir.to_dict(), gate.resolve_ir(gate.ROOT /
                         "tests/support/native/assemblies/native-uart.toml", test_support=True).to_dict())

    def test_adc_and_pwm_decisions_preserve_json_projection(self):
        for path, kind in (("stm32_assembly/ve-adc-scan.toml", "adc"), ("stm32_assembly/ve-pwm.toml", "pwm"),):
            with self.subTest(fixture=path):
                ir = gate.resolve_ir(gate.ROOT / "tests/contracts" / path, test_support=native_fixture(gate.ROOT / "tests/contracts" / path))
                item, = (item for item in ir.controllers if item.kind == kind)
                if kind == "adc":
                    self.assertEqual(item.options.channels,
                                     tuple(item.to_dict()["channels"]))
                    self.assertEqual(item.options.sample_times,
                                     tuple(item.to_dict()["sample_times"]))
                else:
                    self.assertEqual(item.options.tick_hz, item.to_dict()["tick_hz"])
                with self.assertRaises(FrozenInstanceError):
                    if kind == "adc":
                        item.options.reference_mv = 0
                    else:
                        item.options.period_ticks = 1
                self.assertEqual(ir.to_dict(), gate.resolve_ir(
                    gate.ROOT / "tests/contracts" / path, test_support=native_fixture(gate.ROOT / "tests/contracts" / path)).to_dict())

    def test_rejected_family_invalidates_previous_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "board"
            package.mkdir()
            board = gate.load(gate.ROOT / "tests/support/native/board/board.json")
            (package / "board.json").write_text(json.dumps(board))
            assembly = root / "assembly.toml"
            assembly.write_text('schema=2\nboard_package="board"\n'
                                'backend="native"\nclock="model"\n'
                                '[memory]\nmain_stack_bytes=2048\n')
            output = root / "bundle"
            gate.configure(assembly, output, test_support=native_fixture(assembly))
            board["soc_family"] = "unmaintained"
            (package / "board.json").write_text(json.dumps(board))
            with self.assertRaisesRegex(gate.ConfigurationError,
                                        "declarative software bindings"):
                gate.configure(assembly, output, test_support=native_fixture(assembly))
            self.assertFalse(output.exists())

    def test_cpu_facts_are_exported_to_the_build_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "bundle"
            gate.configure(gate.ROOT /
                           "tests/support/native/assemblies/native-empty.toml",
                           output, test_support=True)
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
