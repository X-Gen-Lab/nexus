"""Exact part policy and emitted physical geometry share validated facts."""
from tools.configure.tests.device_fixture import (load_soc_facts,
                                                   validate_soc_facts)
from tools.configure.tests.native_fixture import native_fixture
import copy
from dataclasses import replace
import unittest

from tools.configure import bindings, configure as gate
from tools.configure.ir import freeze
from tools.configure.providers import descriptor


class PartFactsTests(unittest.TestCase):
    def test_ethernet_memory_window_uses_the_selected_physical_domain(self):
        for fixture in ("stm32-zg",):
            with self.subTest(fixture=fixture):
                result = gate.resolve_ir(gate.ROOT /
                    f"tests/contracts/ethernet_assembly/{fixture}.toml", test_support=native_fixture(gate.ROOT /
                    f"tests/contracts/ethernet_assembly/{fixture}.toml"))
                regions = tuple(freeze({**region, "size": 65536})
                                if region["id"] == "sram" else region
                                for region in result.physical_part.memory)
                result = replace(result, physical_part=replace(
                    result.physical_part, memory=regions))
                source = bindings.emit(result)[1]
                self.assertIn("{ 0x20000000u, 65536u,", source)
                regions = tuple(freeze({**region, "dma": False})
                                if region["id"] == "sram" else region
                                for region in result.physical_part.memory)
                result = replace(result, physical_part=replace(
                    result.physical_part, memory=regions))
                with self.assertRaisesRegex(ValueError, "Ethernet.*memory"):
                    bindings.emit(result)


    def test_unreviewed_part_cannot_copy_a_reviewed_physical_variant(self):
        for family in descriptor.families():
            with self.subTest(family=family.family):
                facts = load_soc_facts(gate.ROOT / family.source_directory / "soc.json")
                unknown = "UNREVIEWED_PART"
                facts["variants"][unknown] = copy.deepcopy(
                    next(iter(facts["variants"].values())))
                with self.assertRaisesRegex(ValueError, "part"):
                    validate_soc_facts(facts, unknown)

    def test_package_identity_cannot_drift_from_exact_part(self):
        facts = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
        facts["variants"]["STM32F407VET6"]["package"] = "LQFP144"
        with self.assertRaisesRegex(ValueError, "package"):
            validate_soc_facts(facts, "STM32F407VET6")

    def test_unselected_variant_obeys_the_complete_physical_contract(self):
        def unknown_field(variant):
            variant["capacity_override"] = 1

        def package(variant):
            variant["package"] = "LQFP100"

        def flash_geometry(variant):
            variant["flash"]["erase_spans"].pop()

        def dma_access(variant):
            variant["memory"][0]["dma"] = "true"

        def overlap(variant):
            variant["memory"][1]["origin"] = variant["memory"][0]["origin"]

        def optional_memory(variant):
            variant["memory"] = [region for region in variant["memory"]
                                 if region["id"] != "ccm"]

        def linker_domains(variant):
            for region in variant["memory"]:
                region["linker"] = False

        for mutate, error in ((unknown_field, "unknown"),
                              (package, "package"),
                              (flash_geometry, "geometry"),
                              (dma_access, "DMA"),
                              (overlap, "Overlapping"),
                              (optional_memory, "Optional memory"),
                              (linker_domains, "default RAM")):
            with self.subTest(contract=mutate.__name__):
                facts = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
                mutate(facts["variants"]["STM32F407ZGT6"])
                with self.assertRaisesRegex(ValueError, error):
                    validate_soc_facts(facts, "STM32F407VET6")

    def test_full_variant_validation_preserves_selected_fact_identity(self):
        for provider in descriptor.families():
            path = gate.ROOT / provider.source_directory / "soc.json"
            for part in gate.load(path)["variants"]:
                with self.subTest(family=provider.family, part=part):
                    facts = load_soc_facts(path, part=part)
                    original = copy.deepcopy(facts)
                    variant = facts["variants"][part]
                    self.assertIs(validate_soc_facts(facts, part), variant)
                    self.assertEqual(facts, original)

    def test_malformed_memory_entries_are_configuration_rejections(self):
        for selected in ("STM32F407VET6", "STM32F407ZGT6"):
            with self.subTest(selected=selected):
                facts = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
                facts["variants"]["STM32F407ZGT6"]["memory"] = [False]
                with self.assertRaisesRegex(ValueError, "memory"):
                    validate_soc_facts(facts, selected)

    def test_unknown_f407_part_cannot_fall_through_to_zg_geometry(self):
        result = gate.resolve_ir(gate.ROOT /
                              "tests/contracts/stm32_assembly/ve-flash.toml", test_support=native_fixture(gate.ROOT /
                              "tests/contracts/stm32_assembly/ve-flash.toml"))
        result = replace(result, physical_part=replace(
            result.physical_part, part="UNREVIEWED_PART"))
        with self.assertRaisesRegex(ValueError, "part"):
            bindings.emit(result)

    def test_f407_emits_selected_geometry_from_physical_facts(self):
        for density, size, count in (("ve", 524288, 8),
                                     ("zg", 1048576, 12)):
            with self.subTest(density=density):
                result = gate.resolve_ir(gate.ROOT /
                    f"tests/contracts/stm32_assembly/{density}-flash.toml", test_support=native_fixture(gate.ROOT /
                    f"tests/contracts/stm32_assembly/{density}-flash.toml"))
                source = bindings.emit(result)[1]
                self.assertIn("static const nx_flash_erase_span_t "
                              "s_nx_flash_erase_spans_flash0[]", source)
                self.assertIn(".base_address = 0x08000000u", source)
                self.assertIn(f".size = {size}u", source)
                self.assertIn(".erase_span_count = 3u", source)
                self.assertEqual(sum(span["count"] for span in
                                     result.flash["erase_spans"]), count)
                for span in result.flash["erase_spans"]:
                    self.assertIn("{ %du, %du, %du }" % (span["offset"],
                        span["erase_bytes"], span["count"]), source)
                self.assertEqual(sum(span["erase_bytes"] * span["count"]
                                     for span in result.flash["erase_spans"]), size)
                self.assertNotIn("g_nx_stm32_flash_", source)


if __name__ == "__main__":
    unittest.main()
