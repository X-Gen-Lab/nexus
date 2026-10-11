"""Optional exact RAM domains have explicit budgets and fail-closed sections."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate, memory


class MemoryRegionConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def authored(self, regions="", board="gd32f470_liangshan"):
        clock = "hxtal25-pll200" if board.startswith("gd32") else "hse8-pll168"
        content = (f'schema=2\nboard="{board}"\nbackend="baremetal"\n'
                   f'clock="{clock}"\n[memory]\nmain_stack_bytes=2048\n'
                   'libc_heap_bytes=0\nstatic_ram_limit_bytes=4096\n'+regions)
        path = self.root / "assembly.toml"
        path.write_text(content)
        return path

    def test_extra_domains_have_separate_immutable_limits_and_physical_facts(self):
        path = self.authored('[memory.regions.addsram]\nlimit_bytes=128\n'
                             '[memory.regions.tcm]\nlimit_bytes=64\n')
        ir = gate.resolve_ir(path)
        selected = {item.id: item.limit_bytes for item in ir.memory_budget.regions}
        self.assertEqual(selected, {"addsram": 128, "tcm": 64})
        with self.assertRaises(FrozenInstanceError):
            ir.memory_budget.regions[0].limit_bytes = 256
        domains = {item["id"]: item for item in ir["memory"]}
        self.assertTrue(domains["addsram"]["linker"])
        self.assertTrue(domains["tcm"]["linker"])
        self.assertFalse(domains["tcm"]["dma"])
        output = self.root / "selected"
        gate.configure(path, output)
        budget = json.loads((output / "resource-budget.json").read_text())
        self.assertEqual(budget["ram_reserved_max"],
                         {"sram": 4096, "addsram": 128, "tcm": 64})
        source = (output / "memory.ld").read_text()
        self.assertIn("__nx_addsram_data_load", source)
        self.assertIn("__nx_tcm_noinit_end", source)
        self.assertIn(".bss.nx_tcm_noinit.*", source)
        self.assertLess(source.index(".nx_tcm_bss"), source.index("_sbss ="))
        self.assertIn("TCM (rw)", source)
        header = (output / "nexus_config.h").read_text()
        self.assertIn("#define NEXUS_MEMORY_ADDSRAM 1", header)
        self.assertIn("#define NEXUS_MEMORY_TCM 1", header)
        cmake = (output / "selection.cmake").read_text()
        self.assertIn("set(NEXUS_MEMORY_ADDSRAM 1)", cmake)
        self.assertIn("set(NEXUS_MEMORY_TCM 1)", cmake)

    def test_default_has_no_selected_extra_budget_and_rejects_live_sections(self):
        path = self.authored()
        output = self.root / "default"
        gate.configure(path, output)
        source = (output / "memory.ld").read_text()
        self.assertIn("memory region is not selected", source)
        self.assertIn(".bss.nx_addsram_noinit.*", source)
        header = (output / "nexus_config.h").read_text()
        self.assertIn("#define NEXUS_MEMORY_ADDSRAM 0", header)
        self.assertIn("#define NEXUS_MEMORY_TCM 0", header)
        cmake = (output / "selection.cmake").read_text()
        self.assertIn("set(NEXUS_MEMORY_ADDSRAM 0)", cmake)
        self.assertIn("set(NEXUS_MEMORY_TCM 0)", cmake)
        ir = gate.resolve_ir(path)
        self.assertEqual(ir.memory_budget.regions, ())
        self.assertEqual([item["id"] for item in ir["memory"] if item["linker"]],
                         ["sram"])

    def test_extra_selection_cannot_replace_the_authoritative_primary_ram(self):
        soc = json.loads((gate.ROOT / "soc/gd32f470/soc.json").read_text())
        physical = soc["variants"]["GD32F470ZGT6"]["memory"]
        original = json.dumps(physical, sort_keys=True)
        selected = memory.select(list(reversed(physical)),
            {"regions": {"tcm": {"limit_bytes": 64}}},
            "gd32f470", "GD32F470ZGT6", obj=gate.obj,
            integer=gate.integer, fail=gate.fail)
        self.assertEqual(next(item["id"] for item in selected if item["linker"]),
                         "sram")
        self.assertEqual(json.dumps(physical, sort_keys=True), original)

    def test_budget_rejects_zero_bool_unaligned_and_physical_overflow(self):
        for value in ("0", "true", "3", "262148"):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "region"):
                    gate.resolve_ir(self.authored('[memory.regions.addsram]\n'
                                                  f'limit_bytes={value}\n'))

    def test_unknown_main_or_unsupported_chip_region_is_not_silently_selected(self):
        for name in ("sram", "missing"):
            with self.subTest(region=name):
                with self.assertRaisesRegex(ValueError, "region"):
                    gate.resolve_ir(self.authored(f'[memory.regions.{name}]\n'
                                                  'limit_bytes=128\n'))
        with self.assertRaisesRegex(ValueError, "region"):
            gate.resolve_ir(self.authored('[memory.regions.ccm]\nlimit_bytes=128\n',
                                         board="stm32f407_qiming_v31"))

    def test_region_contract_rejects_unknown_fields_and_non_table_values(self):
        for content in ('regions=4\n',
                        '[memory.regions.addsram]\nlimit_bytes=128\nextra=1\n',
                        '[memory.regions.addsram]\nsize=128\n'):
            with self.subTest(content=content):
                with self.assertRaises(ValueError):
                    gate.resolve_ir(self.authored(content))


if __name__ == "__main__":
    unittest.main()
