"""Resource audit checks selected backing consistency without MCU admission."""
import copy
import unittest

from tools.configure import configure, memory
from tools.configure.tests.native_fixture import native_fixture
from tools.measurement.resources import memory_contract


class ExternalMemoryProjectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = configure.ROOT / "boards/stm32f407zg_qiming_v41/assemblies/sram-startup.toml"
        cls.base = configure.resolve_ir(path, test_support=native_fixture(path)).to_dict()

    def projection(self):
        return copy.deepcopy(self.base)

    @staticmethod
    def geometry(value, origin, size):
        endpoint = value["external_memory"][0]["endpoint"]
        endpoint.update(address=origin, bytes=size)
        value["devices"][0].update(address=origin, bytes=size)
        value["memory"][-1].update(origin=origin, size=size)

    def test_renamed_family_part_controller_and_region_remain_a_consistent_audit(self):
        value = self.projection()
        value.update(soc_family="reference-family", part="REFERENCE_PART",
                     backend="freertos", optimization="Os-lto")
        region = value["external_memory"][0]
        region["id"] = value["memory"][-1]["id"] = "placed_ram"
        value["memory_budgets"]["regions"]["placed_ram"] = value["memory_budgets"]["regions"].pop("addsram")
        endpoint = region["endpoint"]
        endpoint.update(id="memory-bank", controller="memory-owner")
        value["devices"][0].update(id="memory-bank", controller="memory-owner")
        value["controllers"][0].update(id="memory-owner", controller="MEMORY_PORT",
                                          mode="selected-window")
        memory.validate_external_projection(value)
        self.assertEqual("placed_ram", memory_contract(value)[1][-1]["id"])

    def test_another_explicit_window_image_partition_and_budget_are_audited(self):
        value = self.projection()
        self.geometry(value, 0x72000000, 0x40000)
        value["layout"].update(origin=0x08020000, size=0xE0000)
        value["memory_budgets"].update(main_stack_bytes=128, libc_heap_bytes=512,
            image_prefix_bytes=256, image_suffix_bytes=512)
        value["memory_budgets"]["regions"]["addsram"]["limit_bytes"] = 0x20000
        self.assertEqual((0x72000000, 0x40000), tuple(
            memory_contract(value)[1][-1][key] for key in ("origin", "size")))
        # The verifier audits a one-way artifact only. This call does not ask
        # the production resolver to admit this geometry or startup policy.

    def test_backing_references_and_unique_instance_owners_cannot_be_detached(self):
        mutations = (
            lambda value: value.pop("external_memory"),
            lambda value: value.update(external_memory=[]),
            lambda value: value["external_memory"][0]["endpoint"].update(id="missing"),
            lambda value: value["devices"][0].update(id="different"),
            lambda value: value["devices"].append(copy.deepcopy(value["devices"][0])),
            lambda value: value["devices"].append({**value["devices"][0], "id": "untracked"}),
            lambda value: value["external_memory"].append(copy.deepcopy(value["external_memory"][0])),
            lambda value: value["controllers"].clear(),
            lambda value: value["controllers"].append(copy.deepcopy(value["controllers"][0])),
            lambda value: value["controllers"][0].update(kind="uart"),
            lambda value: value["controllers"][0].update(controller=""),
            lambda value: value["external_memory"][0]["endpoint"].update(mode="cpu-window"),
            lambda value: value["memory"][-1].update(origin=0x69000000),
            lambda value: value["memory"][-1].update(dma=True),
            lambda value: value["memory"][-1].update(linker=False),
        )
        for mutate in mutations:
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                value = self.projection()
                mutate(value)
                memory_contract(value)

    def test_geometry_budget_and_flash_layout_corruption_are_not_repaired(self):
        mutations = (
            lambda value: self.geometry(value, 0xFFFF0000, 0x20000),
            lambda value: self.geometry(value, 0x68000000, 0),
            lambda value: self.geometry(value, 0x68000002, 0x100000),
            lambda value: value["memory_budgets"]["regions"].clear(),
            lambda value: value["memory_budgets"]["regions"]["addsram"].update(limit_bytes=0x100004),
            lambda value: value["memory_budgets"]["regions"]["addsram"].update(limit_bytes=5),
            lambda value: value["memory_budgets"]["regions"]["addsram"].update(limit_bytes=True),
            lambda value: value["memory_budgets"].update(main_stack_bytes=0x30000),
            lambda value: value["memory_budgets"].update(image_prefix_bytes=0x100000),
            lambda value: value["memory_budgets"].update(image_suffix_bytes=-4),
            lambda value: value["memory_budgets"].update(flash_load_bytes=0x100004),
            lambda value: value["layout"].update(origin=0x07FF0000),
            lambda value: value["layout"].update(size=0x100004),
        )
        for mutate in mutations:
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                value = self.projection()
                mutate(value)
                memory_contract(value)

    def test_self_consistent_external_window_still_cannot_overlap_other_memory(self):
        value = self.projection()
        self.geometry(value, 0x20001000, 0x100000)
        with self.assertRaisesRegex(ValueError, "overlap|external"):
            memory_contract(value)


if __name__ == "__main__":
    unittest.main()
