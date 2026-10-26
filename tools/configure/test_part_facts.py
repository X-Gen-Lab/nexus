"""Exact part policy and emitted physical geometry share validated facts."""
import copy
import unittest

from tools.configure import bindings, configure as gate
from tools.configure.providers import descriptor


class PartFactsTests(unittest.TestCase):
    def test_ethernet_memory_window_uses_the_selected_physical_domain(self):
        for fixture in ("stm32-zg", "gd32"):
            with self.subTest(fixture=fixture):
                result = gate.resolve(gate.ROOT /
                    f"tests/contracts/ethernet_assembly/{fixture}.toml")
                memory = next(region for region in result["memory"]
                              if region["id"] == "sram")
                memory["size"] = 65536
                source = bindings.emit(result)[1]
                self.assertTrue("{ 0x20000000u, 65536u," in source)
                memory["dma"] = False
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
                result = gate.resolve(gate.ROOT / "tests/contracts" / fixture)
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

    def test_unknown_f407_part_cannot_fall_through_to_zg_geometry(self):
        result = gate.resolve(gate.ROOT /
                              "tests/contracts/stm32_assembly/ve-flash.toml")
        result["part"] = "UNREVIEWED_PART"
        with self.assertRaisesRegex(ValueError, "part"):
            bindings.emit(result)

    def test_f407_emits_selected_geometry_from_physical_facts(self):
        for density, size, count in (("ve", 524288, 8),
                                     ("zg", 1048576, 12)):
            with self.subTest(density=density):
                result = gate.resolve(gate.ROOT /
                    f"tests/contracts/stm32_assembly/{density}-flash.toml")
                source = bindings.emit(result)[1]
                self.assertIn("static const nx_flash_sector_t "
                              "s_nx_flash_sectors_flash0[]", source)
                self.assertIn(".base_address = 0x08000000u", source)
                self.assertIn(f".size = {size}u", source)
                self.assertIn(f".sector_count = {count}u", source)
                offset = 0
                for block in result["flash"]["erase_blocks"]:
                    self.assertIn(f"{{ {offset}u, {block}u }}", source)
                    offset += block
                self.assertEqual(offset, size)
                self.assertNotIn("g_nx_stm32_flash_", source)


if __name__ == "__main__":
    unittest.main()
