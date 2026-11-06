"""Explicit placed SRAM binds one boot-owned child without altering chip RAM."""
from dataclasses import FrozenInstanceError
import copy
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.configure import bindings, configure as gate, emission, memory
from tools.measurement.resources import memory_contract


BOARDS = ("stm32f407zg_qiming_core", "stm32f407zg_qiming_v41")
TIMING = "{address_setup=2,address_hold=1,data_setup=9,bus_turn=1}"


class F407SramStartupTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="f407 placed sram ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def text(self, board=BOARDS[1], *, budget=4096, mode="startup-memory",
             child=True, ccm=False):
        text = (f'schema=2\nboard="{board}"\nbackend="baremetal"\n'
                'clock="hse8-pll168"\noptimization="Os"\n'
                '[hardware]\nsram_population="is62wv51216"\n'
                '[memory]\nmain_stack_bytes=2048\nlibc_heap_bytes=0\n'
                'flash_load_limit_bytes=65536\nstatic_ram_limit_bytes=32768\n')
        if budget is not None:
            text += f'[memory.regions.addsram]\nlimit_bytes={budget}\n'
        if ccm:
            text += '[memory.regions.ccm]\nlimit_bytes=4096\n'
        if child:
            text += ('[external_bus.ext0]\nbinding="fsmc0"\nmode="async16"\n'
                     '[memory_window.ram0]\ncontroller="ext0"\nbinding="sram0"\n'
                     f'mode="{mode}"\nread_timing={TIMING}\nwrite_timing={TIMING}\n')
        return text

    def resolve(self, text=None):
        path = self.directory / "assembly.toml"
        path.write_text(self.text() if text is None else text)
        return gate.resolve_ir(path)

    def test_both_boards_bind_a_separate_immutable_selected_external_region(self):
        for board in BOARDS:
            with self.subTest(board=board):
                result = self.resolve(self.text(board))
                external, = result.external_memory
                self.assertEqual("addsram", external.id)
                self.assertIs(external.endpoint, result.devices[0])
                self.assertEqual((0x68000000, 0x100000),
                                 (external.origin, external.size))
                self.assertEqual(["sram", "ccm"],
                                 [item["id"] for item in result.physical_part.memory])
                region = result.memory[-1]
                self.assertEqual(("addsram", 0x68000000, 0x100000, False, True),
                                 tuple(region[key] for key in
                                       ("id", "origin", "size", "dma", "linker")))
                with self.assertRaises(FrozenInstanceError):
                    external.id = "sram"
                with self.assertRaises(TypeError):
                    region["size"] = 16

    def test_checked_in_software_profiles_select_only_explicit_startup_backing(self):
        for board in BOARDS:
            with self.subTest(board=board):
                result = gate.resolve_ir(gate.ROOT / "boards" / board /
                                         "assemblies/sram-startup.toml")
                selected, = result.external_memory
                self.assertEqual("startup-memory", selected.endpoint.options.mode)
                self.assertEqual(4096,
                    memory.resource_budget(result)["ram_reserved_max"]["addsram"])

    def test_budget_and_header_select_only_the_explicit_external_placement(self):
        result = self.resolve()
        self.assertEqual(memory.resource_budget(result)["ram_reserved_max"]["addsram"],
                         4096)
        self.assertIn("#define NEXUS_MEMORY_ADDSRAM 1", memory.header_lines(result))
        self.assertIn("set(NEXUS_MEMORY_ADDSRAM 1)", memory.selection_lines(result))
        linker = memory.linker(result)
        self.assertIn("ADDSRAM (rw) : ORIGIN = 0x68000000, LENGTH = 1048576", linker)
        self.assertIn(".nx_addsram_data", linker)
        self.assertIn(".nx_addsram_noinit", linker)
        self.assertIn("__nx_msp_start = _estack - _Min_Stack_Size", linker)

    def test_readonly_boot_plan_uses_one_runtime_bank_and_real_linker_bounds(self):
        result = self.resolve()
        source = bindings.emit(result)[1]
        self.assertIn("const nx_stm32_sram_boot_plan_t g_nx_stm32_sram_boot_plan", source)
        self.assertIn(".bank = &g_nx_fsmc_bank_config_ext0[0u]", source)
        self.assertIn(".region_count = 2u", source)
        self.assertIn(".maximum_hclk_hz = 168000000u", source)
        for symbol in ("_sidata", "_sdata", "_edata", "_sbss", "_ebss",
                       "__nx_primary_region_start", "__nx_primary_region_end",
                       "__nx_msp_start", "_estack", "g_pfnVectors",
                       "__nx_addsram_data_load", "__nx_addsram_data_start",
                       "__nx_addsram_data_end", "__nx_addsram_bss_start",
                       "__nx_addsram_bss_end", "__nx_addsram_noinit_start",
                       "__nx_addsram_noinit_end"):
            self.assertIn(symbol, source)
        self.assertIn("nx_stm32_fsmc_adopt_boot(", source)
        self.assertNotIn("FSMC_Bank1->", source)
        self.assertNotIn("memcpy(", source)
        self.assertNotIn("memset(", source)

    def test_boot_owned_only_pads_do_not_reference_an_absent_gpio_plan(self):
        source = bindings.emit(self.resolve())[1]
        self.assertNotIn("s_nx_platform_config", source)
        self.assertNotIn("nx_stm32_platform_selected_preflight(", source)
        self.assertNotIn("s_nx_fsmc_pin_state_ext0[0u]", source)

    def test_ccm_inserts_its_existing_copy_bounds_once_without_external_dma(self):
        result = self.resolve(self.text(ccm=True))
        source = bindings.emit(result)[1]
        self.assertIn(".region_count = 3u", source)
        self.assertIn("__nx_ccm_data_load", source)
        self.assertIn("__nx_ccm_noinit_end", source)
        self.assertEqual(["sram", "ccm", "addsram"],
                         [item["id"] for item in result.memory])
        dma_source, count = memory.dma_regions("test", result)
        self.assertEqual(count, 1)
        self.assertNotIn("68000000", "\n".join(dma_source))
        self.assertNotIn("10000000", "\n".join(dma_source))

    def test_runtime_window_and_empty_assembly_have_no_startup_memory_cost(self):
        for text in (self.text(budget=None, mode="cpu-window"),
                     self.text(budget=None, child=False)):
            with self.subTest(text=text):
                result = self.resolve(text)
                self.assertEqual((), result.external_memory)
                self.assertIn("#define NEXUS_MEMORY_ADDSRAM 0", memory.header_lines(result))
                self.assertNotIn("g_nx_stm32_sram_boot_plan", bindings.emit(result)[1])
                self.assertNotIn("ADDSRAM (rw)", memory.linker(result))

    def test_selected_startup_memory_requires_its_own_explicit_placement_budget(self):
        with self.assertRaisesRegex(gate.ConfigurationError, "addsram.*budget|budget.*addsram"):
            self.resolve(self.text(budget=None))

    def test_external_budget_requires_exact_startup_child_not_a_runtime_alias(self):
        for text in (self.text(child=False), self.text(mode="cpu-window")):
            with self.subTest(text=text), self.assertRaisesRegex(
                    gate.ConfigurationError, "startup-memory|startup memory"):
                self.resolve(text)

    def test_external_budget_validates_type_alignment_and_exact_capacity(self):
        for value in ("0", "-1", "true", "1048580", "3"):
            with self.subTest(value=value), self.assertRaisesRegex(
                    gate.ConfigurationError, "addsram"):
                self.resolve(self.text(budget=value))

    def test_startup_profile_refuses_image_reservations_and_nondefault_layout(self):
        for field in ("image_prefix_bytes", "image_suffix_bytes"):
            with self.subTest(field=field), self.assertRaisesRegex(
                    gate.ConfigurationError, "startup|SRAM|addsram"):
                self.resolve(self.text().replace('[memory]\n',
                                                  '[memory]\n' + field + '=512\n'))
        layout = self.directory / "layout.json"
        layout.write_text(json.dumps({"schema_version": 1, "regions": [
            {"id": "app", "origin": 0x08020000, "size": 0xe0000, "kind": "image"}]}))
        text = self.text().replace('optimization="Os"',
                                   'optimization="Os"\nlayout="layout.json"')
        with self.assertRaisesRegex(gate.ConfigurationError,
                                    "startup|SRAM|addsram"):
            self.resolve(text)

    def test_selected_reset_bootstrap_requires_the_proven_minimum_msp_budget(self):
        for size in (8, 256, 504):
            with self.subTest(size=size), self.assertRaisesRegex(
                    gate.ConfigurationError, "512|stack|MSP"):
                self.resolve(self.text().replace("main_stack_bytes=2048",
                                                 "main_stack_bytes=" + str(size)))
        result = self.resolve(self.text().replace("main_stack_bytes=2048",
                                                  "main_stack_bytes=512"))
        self.assertEqual(512, result.memory_budget.main_stack_bytes)

    def test_startup_lto_requires_a_separate_actual_post_lto_stack_proof(self):
        for optimization in ("O2-lto", "Os-lto", "O3-lto"):
            with self.subTest(optimization=optimization), self.assertRaisesRegex(
                    gate.ConfigurationError, "LTO|lto|stack"):
                self.resolve(self.text().replace('optimization="Os"',
                                                 f'optimization="{optimization}"'))

    def test_sealed_memory_projection_cannot_detach_or_expand_its_selected_bank(self):
        resolved = self.resolve().to_dict()
        self.assertEqual(3, len(memory_contract(resolved)[1]))
        mutations = (
            lambda value: value.pop("external_memory"),
            lambda value: value["external_memory"][0]["endpoint"].update(mode="cpu-window"),
            lambda value: value["external_memory"][0]["endpoint"].update(bytes=0x200000),
            lambda value: value["memory"][-1].update(dma=True),
            lambda value: value["memory"][-1].update(origin=0x69000000),
            lambda value: value["devices"][0].update(id="different"),
            lambda value: value["devices"].append({**value["devices"][0], "id": "alias"}),
            lambda value: value["memory_budgets"]["regions"]["addsram"].update(limit_bytes=0x100004))
        for mutate in mutations:
            invalid = copy.deepcopy(resolved)
            mutate(invalid)
            with self.subTest(mutate=mutate), self.assertRaisesRegex(
                    ValueError, "external|SRAM|addsram"):
                memory_contract(invalid)

    def test_malformed_external_audit_records_fail_as_checked_resource_errors(self):
        resolved = self.resolve().to_dict()
        mutations = (
            lambda value: value.update(optimization=None),
            lambda value: value.update(layout=None),
            lambda value: value["memory_budgets"].update(regions=None),
            lambda value: value.update(controllers=[None]))
        for mutate in mutations:
            invalid = copy.deepcopy(resolved)
            mutate(invalid)
            with self.subTest(mutate=mutate), self.assertRaisesRegex(
                    ValueError, "external|SRAM|addsram"):
                memory_contract(invalid)

    def test_placed_linker_sections_keep_msp_heap_and_code_internal(self):
        compiler = shutil.which("gcc")
        self.assertIsNotNone(compiler, "Maintained actual GNU linker required")
        result = self.resolve()
        linker = self.directory / "memory.ld"
        linker.write_text(memory.linker(result))
        source = self.directory / "placed.c"
        source.write_text('void Reset_Handler(void) {}\n'
                          'int primary_data=3; int primary_bss;\n'
                          '__attribute__((section(".nx_addsram_data"))) int data=7;\n'
                          '__attribute__((section(".bss.nx_addsram"))) int bss;\n'
                          '__attribute__((section(".bss.nx_addsram_noinit"))) int untouched;\n')
        elf = self.directory / "placed.elf"
        command = [compiler, "-fno-pie", "-no-pie", "-fno-unwind-tables",
                   "-fno-asynchronous-unwind-tables", "-nostdlib",
                   "-Wl,--build-id=none", "-T", str(linker), str(source), "-o", str(elf)]
        linked = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(0, linked.returncode, linked.stdout + linked.stderr)
        table = subprocess.run(["nm", "-P", str(elf)], capture_output=True,
                               text=True, timeout=30, check=True).stdout
        values = {row[0]: int(row[2], 16) for line in table.splitlines()
                  if len(row := line.split()) >= 3}
        self.assertEqual(0x20020000, values["_estack"])
        self.assertEqual(0x20020000 - 2048, values["__nx_msp_start"])
        for name in ("data", "bss", "untouched"):
            self.assertTrue(0x68000000 <= values[name] < 0x68100000)
        for name in ("primary_data", "primary_bss", "__nx_heap_start"):
            self.assertTrue(0x20000000 <= values[name] < 0x20020000)
        self.assertTrue(0x08000000 <= values["Reset_Handler"] < 0x08100000)
        self.assertEqual(0x20000000, values["__nx_primary_region_start"])
        self.assertEqual(0x20020000, values["__nx_primary_region_end"])


if __name__ == "__main__":
    unittest.main()
