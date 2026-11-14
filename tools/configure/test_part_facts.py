"""Exact part policy and emitted physical geometry share validated facts."""
from tools.configure.tests.device_fixture import (load_soc_facts,
                                                   validate_soc_facts)
from tools.configure.tests.native_fixture import native_fixture
import copy
from dataclasses import replace
import shutil
import unittest

from tools.configure import bindings, configure as gate
from tools.configure.providers import descriptor


class PartFactsTests(unittest.TestCase):
    def test_ethernet_memory_window_uses_the_selected_physical_domain(self):
        from tools.configure.test_device_catalog import DeviceCatalogConfigurationTests
        fixture = DeviceCatalogConfigurationTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        original = gate.ROOT / "tests/contracts/ethernet_assembly"
        board = fixture.root / "eth-board"
        board.mkdir()
        shutil.copyfile(original / "stm32-zg-board/board.json", board / "board.json")
        fixture.assembly.write_text((original / "stm32-zg.toml").read_text().replace(
            'board_package="stm32-zg-board"', 'board_package="eth-board"'))

        def memory(soc, **changes):
            region = next(item for item in soc["variants"]["STM32F407ZGT6"]["memory"]
                          if item["id"] == "sram")
            region.update(changes)

        # Change the author fact and resolve a new sealed selection. Replacing
        # only one downstream IR projection would leave its provider unchanged.
        fixture.update("soc/stm32/stm32f4/soc.json", lambda soc: memory(soc, size=65536))
        source = bindings.emit(fixture.resolve())[1]
        self.assertIn(".begin = 536870912u, .size = 65536u,", source)
        fixture.update("soc/stm32/stm32f4/soc.json", lambda soc: memory(soc, dma=False))
        with self.assertRaisesRegex(ValueError, "Ethernet.*memory"):
            bindings.emit(fixture.resolve())


    def test_variant_schema_does_not_require_a_public_part_whitelist(self):
        for family in descriptor.families():
            with self.subTest(family=family.family):
                facts = load_soc_facts(gate.ROOT / family.source_directory / "soc.json")
                unknown = "UNREVIEWED_PART"
                facts["variants"][unknown] = copy.deepcopy(
                    next(iter(facts["variants"].values())))
                self.assertIs(validate_soc_facts(facts, unknown),
                              facts["variants"][unknown])
                # This is data-schema admission, not a software support grant.

    def test_f407_package_and_ccm_facts_match_reviewed_ds8626_and_rm0090(self):
        facts = load_soc_facts(gate.ROOT / "soc/stm32/stm32f4/soc.json")
        for part, package in (("STM32F407VET6", "LQFP100"),
                              ("STM32F407ZGT6", "LQFP144")):
            with self.subTest(part=part):
                variant = facts["variants"][part]
                self.assertEqual(variant["package"], package)
                ccm, = (row for row in variant["memory"] if row["id"] == "ccm")
                self.assertEqual((ccm["origin"], ccm["size"], ccm["dma"]),
                                 (0x10000000, 65536, False))

    def test_unselected_variant_obeys_the_complete_physical_contract(self):
        def unknown_field(variant):
            variant["capacity_override"] = 1

        def flash_geometry(variant):
            variant["flash"]["erase_spans"].pop()

        def dma_access(variant):
            variant["memory"][0]["dma"] = "true"

        def overlap(variant):
            variant["memory"][1]["origin"] = variant["memory"][0]["origin"]

        def linker_domains(variant):
            for region in variant["memory"]:
                region["linker"] = False

        for mutate, error in ((unknown_field, "unknown"),
                              (flash_geometry, "geometry"),
                              (dma_access, "DMA"),
                              (overlap, "Overlapping"),
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

    def test_emitter_uses_resolved_geometry_without_part_name_dispatch(self):
        result = gate.resolve_ir(gate.ROOT /
                              "tests/contracts/stm32_assembly/ve-flash.toml", test_support=native_fixture(gate.ROOT /
                              "tests/contracts/stm32_assembly/ve-flash.toml"))
        expected = bindings.emit(result)
        renamed = replace(result, physical_part=replace(
            result.physical_part, part="UNREVIEWED_PART"))
        # Unknown authored parts are rejected by fact admission above. Once
        # resolved, geometry must not be guessed from a part-name suffix.
        self.assertEqual(bindings.emit(renamed), expected)

    def test_f407_emits_selected_geometry_from_physical_facts(self):
        for density, size, count in (("ve", 524288, 8),
                                     ("zg", 1048576, 12)):
            with self.subTest(density=density):
                result = gate.resolve_ir(gate.ROOT /
                    f"tests/contracts/stm32_assembly/{density}-flash.toml", test_support=native_fixture(gate.ROOT /
                    f"tests/contracts/stm32_assembly/{density}-flash.toml"))
                source = bindings.emit(result)[1]
                self.assertIn("static const nx_flash_erase_span_t "
                              "s_nx_flash_erase_spans_flash0[3]", source)
                self.assertIn(".base_address = 134217728u", source)
                self.assertIn(f".size = {size}u", source)
                self.assertIn(".erase_span_count = 3u", source)
                self.assertEqual(sum(span["count"] for span in
                                     result.flash["erase_spans"]), count)
                for span in result.flash["erase_spans"]:
                    self.assertIn("{ .offset = %du, .erase_bytes = %du, .count = %du }" % (span["offset"],
                        span["erase_bytes"], span["count"]), source)
                self.assertEqual(sum(span["erase_bytes"] * span["count"]
                                     for span in result.flash["erase_spans"]), size)
                self.assertNotIn("g_nx_stm32_flash_", source)


if __name__ == "__main__":
    unittest.main()
