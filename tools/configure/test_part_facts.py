"""Exact part policy and emitted physical geometry share validated facts."""
import copy
from dataclasses import replace
import unittest

from tools.configure import bindings, configure as gate
from tools.configure.ir import freeze
from tools.configure.providers import descriptor


class PartFactsTests(unittest.TestCase):
    def test_ethernet_memory_window_uses_the_selected_physical_domain(self):
        for fixture in ("stm32-zg", "gd32"):
            with self.subTest(fixture=fixture):
                result = gate.resolve_ir(gate.ROOT /
                    f"tests/contracts/ethernet_assembly/{fixture}.toml")
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

    def test_fixed_flash_exports_are_defined_from_selected_facts(self):
        fixtures = (
            ("stm32f103", "mainstream_assembly/f103-devices.toml", 131072, 128, 2),
            ("gd32f303", "mainstream_assembly/gd303-devices.toml", 262144, 128, 2),
            ("gd32f470", "gd32_assembly/flash.toml", 1048576, 256, 2),
            ("stm32g474", "g474_assembly/acquisition.toml", 524288, 256, 8),
            ("stm32h743", "h743_assembly/peripherals.toml", 2097152, 16, 32),
            ("stm32u575", "u575_assembly/peripherals.toml", 2097152, 256, 16),
        )
        for family, fixture, size, count, unit in fixtures:
            with self.subTest(family=family):
                result = gate.resolve_ir(gate.ROOT / "tests/contracts" / fixture)
                source = bindings.emit(result)[1]
                symbol = "g_nx_" + family + "_flash_geometry"
                self.assertTrue(f"const nx_flash_geometry_t {symbol} = {{"
                                in source, symbol)
                self.assertIn(f".size = {size}u", source)
                self.assertIn(f".sector_count = {count}u", source)
                self.assertIn(f".program_unit = {unit}u", source)

    def test_unreviewed_part_cannot_copy_a_reviewed_physical_variant(self):
        for family in descriptor.families():
            with self.subTest(family=family.family):
                facts = gate.load(gate.ROOT / family.source_directory / "soc.json")
                unknown = "UNREVIEWED_PART"
                facts["variants"][unknown] = copy.deepcopy(
                    next(iter(facts["variants"].values())))
                with self.assertRaisesRegex(ValueError, "part"):
                    gate.validate_soc(facts, unknown)

    def test_package_identity_cannot_drift_from_exact_part(self):
        facts = gate.load(gate.ROOT / "soc/stm32/stm32f407/soc.json")
        facts["variants"]["STM32F407VET6"]["package"] = "LQFP144"
        with self.assertRaisesRegex(ValueError, "package"):
            gate.validate_soc(facts, "STM32F407VET6")

    def test_unselected_variant_obeys_the_complete_physical_contract(self):
        def unknown_field(variant):
            variant["capacity_override"] = 1

        def package(variant):
            variant["package"] = "LQFP100"

        def flash_geometry(variant):
            variant["flash"]["erase_blocks"].pop()

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
                facts = gate.load(gate.ROOT / "soc/stm32/stm32f407/soc.json")
                mutate(facts["variants"]["STM32F407ZGT6"])
                with self.assertRaisesRegex(ValueError, error):
                    gate.validate_soc(facts, "STM32F407VET6")

    def test_full_variant_validation_preserves_selected_fact_identity(self):
        for provider in descriptor.families():
            facts = gate.load(gate.ROOT / provider.source_directory / "soc.json")
            original = copy.deepcopy(facts)
            for part, variant in facts["variants"].items():
                with self.subTest(family=provider.family, part=part):
                    self.assertIs(gate.validate_soc(facts, part), variant)
                    self.assertEqual(facts, original)

    def test_malformed_memory_entries_are_configuration_rejections(self):
        for selected in ("STM32F407VET6", "STM32F407ZGT6"):
            with self.subTest(selected=selected):
                facts = gate.load(gate.ROOT / "soc/stm32/stm32f407/soc.json")
                facts["variants"]["STM32F407ZGT6"]["memory"] = [False]
                with self.assertRaisesRegex(ValueError, "memory"):
                    gate.validate_soc(facts, selected)

    def test_unknown_f407_part_cannot_fall_through_to_zg_geometry(self):
        result = gate.resolve_ir(gate.ROOT /
                              "tests/contracts/stm32_assembly/ve-flash.toml")
        result = replace(result, physical_part=replace(
            result.physical_part, part="UNREVIEWED_PART"))
        with self.assertRaisesRegex(ValueError, "part"):
            bindings.emit(result)

    def test_f407_emits_selected_geometry_from_physical_facts(self):
        for density, size, count in (("ve", 524288, 8),
                                     ("zg", 1048576, 12)):
            with self.subTest(density=density):
                result = gate.resolve_ir(gate.ROOT /
                    f"tests/contracts/stm32_assembly/{density}-flash.toml")
                source = bindings.emit(result)[1]
                self.assertIn("static const nx_flash_sector_t "
                              "s_nx_flash_sectors_flash0[]", source)
                self.assertIn(".base_address = 0x08000000u", source)
                self.assertIn(f".size = {size}u", source)
                self.assertIn(f".sector_count = {count}u", source)
                offset = 0
                for block in result.flash["erase_blocks"]:
                    self.assertIn(f"{{ {offset}u, {block}u }}", source)
                    offset += block
                self.assertEqual(offset, size)
                self.assertNotIn("g_nx_stm32_flash_", source)


if __name__ == "__main__":
    unittest.main()
