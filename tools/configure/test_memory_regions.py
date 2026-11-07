"""Optional exact RAM domains have explicit budgets and fail-closed sections."""
from tools.configure.tests.native_fixture import native_fixture
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

from tools.configure import configure as gate, memory


class MemoryRegionConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def authored(self, regions="", board="stm32f407_qiming_v31"):
        clock = "hse8-pll168"
        content = (f'schema=2\nboard="{board}"\nbackend="baremetal"\n'
                   f'clock="{clock}"\n[memory]\nmain_stack_bytes=2048\n'
                   'libc_heap_bytes=0\nstatic_ram_limit_bytes=4096\n'+regions)
        path = self.root / "assembly.toml"
        path.write_text(content)
        return path

    def test_copied_budget_projection_needs_no_provider_tree(self):
        copied = self.root / "memory.py"
        shutil.copyfile(Path(memory.__file__), copied)
        config = {
            "memory_budgets": {"main_stack_bytes": 1024,
                               "static_ram_bytes": 4096},
            "layout": {"size": 32768},
            "memory": [{"id": "sram", "size": 8192, "linker": True}],
        }
        script = (
            "import json, runpy, sys\n"
            "module = runpy.run_path(sys.argv[1])\n"
            "budget = module['resource_budget_projection'](json.loads(sys.argv[2]))\n"
            "print(json.dumps(budget))\n"
        )
        result = subprocess.run(
            [sys.executable, "-I", "-B", "-c", script, str(copied),
             json.dumps(config)], cwd=self.root, capture_output=True,
            text=True, timeout=10, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout), {
            "schema_version": 1, "flash_loaded_max": 32768,
            "flash_footprint_max": 32768, "ram_reserved_max": {"sram": 4096},
            "msp_max": 1024, "heap_max": 0,
        })

    def test_copied_dma_renderer_keeps_exact_reachability_errors(self):
        copied = self.root / "memory.py"
        shutil.copyfile(Path(memory.__file__), copied)
        script = textwrap.dedent("""\
            import json, runpy, sys
            emit = runpy.run_path(sys.argv[1])["dma_regions_from_facts"]
            facts = {"memory": [{"origin": 0x20000000, "size": 1024,
                                "dma_engines": {"DMA1": True, "BDMA": False}}]}
            rendered, count = emit("port", facts["memory"], engine="DMA1")
            unreachable = emit("port", facts["memory"], engine="BDMA")
            errors = []
            for options in ({}, {"engine": "unknown"},
                            {"engine": "DMA1", "flash": {}}):
                try:
                    emit("port", facts["memory"], **options)
                except ValueError as error:
                    errors.append(str(error))
                else:
                    raise AssertionError("Unreviewed DMA reachability accepted")
            print(json.dumps({"rendered": rendered, "count": count,
                              "unreachable": unreachable, "errors": errors}))
        """)
        result = subprocess.run(
            [sys.executable, "-I", "-B", "-c", script, str(copied)],
            cwd=self.root, capture_output=True, text=True, timeout=10,
            check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        observed = json.loads(result.stdout)
        self.assertEqual(observed["count"], 1)
        self.assertIn("0x20000000u, 1024u", "\n".join(observed["rendered"]))
        self.assertEqual(observed["unreachable"], [[], 0])
        self.assertEqual(observed["errors"], [
            "DMA memory projection requires an exact declared engine",
            "DMA memory projection requires an exact declared engine",
            "Engine-specific Flash reachability is not declared",
        ])


    def test_default_has_no_selected_extra_budget_and_rejects_live_sections(self):
        path = self.authored()
        output = self.root / "default"
        gate.configure(path, output, test_support=native_fixture(path))
        source = (output / "memory.ld").read_text()
        self.assertIn("memory region is not selected", source)
        self.assertIn(".bss.nx_ccm_noinit.*", source)
        header = (output / "nexus_config.h").read_text()
        self.assertIn("#define NEXUS_MEMORY_ADDSRAM 0", header)
        self.assertIn("#define NEXUS_MEMORY_TCM 0", header)
        cmake = (output / "selection.cmake").read_text()
        self.assertIn("set(NEXUS_MEMORY_ADDSRAM 0)", cmake)
        self.assertIn("set(NEXUS_MEMORY_TCM 0)", cmake)
        ir = gate.resolve_ir(path, test_support=native_fixture(path))
        self.assertEqual(ir.memory_budget.regions, ())
        self.assertEqual([item["id"] for item in ir.memory if item["linker"]],
                         ["sram"])

    def test_extra_selection_cannot_replace_the_authoritative_primary_ram(self):
        soc = json.loads((gate.ROOT / "soc/stm32/stm32f4/soc.json").read_text())
        physical = soc["variants"]["STM32F407ZGT6"]["memory"]
        original = json.dumps(physical, sort_keys=True)
        selected = memory.select(list(reversed(physical)),
            {"regions": {"ccm": {"limit_bytes": 64}}},
            frozenset({"ccm"}), obj=gate.obj,
            integer=gate.integer, fail=gate.fail)
        self.assertEqual(next(item["id"] for item in selected if item["linker"]),
                         "sram")
        self.assertEqual(json.dumps(physical, sort_keys=True), original)


    def test_unknown_main_or_unsupported_chip_region_is_not_silently_selected(self):
        for name in ("sram", "missing"):
            with self.subTest(region=name):
                with self.assertRaisesRegex(ValueError, "region"):
                    gate.resolve_ir(self.authored(f'[memory.regions.{name}]\n'
                                                  'limit_bytes=128\n'), test_support=native_fixture(self.authored(f'[memory.regions.{name}]\n'
                                                  'limit_bytes=128\n')))
        with self.assertRaisesRegex(ValueError, "region"):
            gate.resolve_ir(self.authored('[memory.regions.tcm]\nlimit_bytes=128\n'))

    def test_f407_ccm_has_explicit_budget_and_remains_cpu_only(self):
        for board in ("stm32f407_qiming_v31", "stm32f407ve_sky_qingchun"):
            with self.subTest(board=board):
                path = self.authored('[memory.regions.ccm]\nlimit_bytes=128\n',
                                     board=board)
                output = self.root / board
                gate.configure(path, output, test_support=native_fixture(path))
                ir = gate.resolve_ir(path, test_support=native_fixture(path))
                domains = {item["id"]: item for item in ir.memory}
                self.assertTrue(domains["ccm"]["linker"])
                self.assertFalse(domains["ccm"]["dma"])
                declarations, count = memory.dma_regions("selected", ir,
                                                         include_flash=True)
                self.assertEqual(count, 2)
                self.assertNotIn("0x10000000", "\n".join(declarations))
                budget = json.loads((output / "resource-budget.json").read_text())
                self.assertEqual(budget["ram_reserved_max"],
                                 {"sram": 4096, "ccm": 128})
                self.assertIn("#define NEXUS_MEMORY_CCM 1",
                              (output / "nexus_config.h").read_text())
                self.assertIn("set(NEXUS_MEMORY_CCM 1)",
                              (output / "selection.cmake").read_text())
                source = (output / "memory.ld").read_text()
                self.assertIn("CCM (rw)", source)
                self.assertIn("__nx_ccm_data_load", source)
                self.assertIn("__nx_ccm_noinit_end", source)
                self.assertLess(source.index(".nx_ccm_noinit"),
                                source.index("_sbss ="))

    def test_ccm_invalid_limits_are_rejected_without_selecting_other_ram(self):
        for value in ("0", "true", "3", "65540"):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "region"):
                    gate.resolve_ir(self.authored('[memory.regions.ccm]\n'
                        f'limit_bytes={value}\n', board="stm32f407_qiming_v31"), test_support=native_fixture(self.authored('[memory.regions.ccm]\n'
                        f'limit_bytes={value}\n', board="stm32f407_qiming_v31")))

    def test_region_boot_bounds_are_derived_from_physical_domains(self):
        cases = (('stm32f407_qiming_v31', 'ccm'),)
        for board, name in cases:
            with self.subTest(board=board, region=name):
                path = self.authored(f'[memory.regions.{name}]\nlimit_bytes=128\n',
                                     board=board)
                ir = gate.resolve_ir(path, test_support=native_fixture(path))
                source = memory.linker(ir)
                self.assertIn(f"__nx_{name}_region_start = ORIGIN({name.upper()});",
                              source)
                self.assertIn(f"__nx_{name}_region_end = ORIGIN({name.upper()}) + "
                              f"LENGTH({name.upper()});", source)

    def test_region_contract_rejects_unknown_fields_and_non_table_values(self):
        for content in ('regions=4\n',
                        '[memory.regions.addsram]\nlimit_bytes=128\nextra=1\n',
                        '[memory.regions.addsram]\nsize=128\n'):
            with self.subTest(content=content):
                with self.assertRaises(ValueError):
                    gate.resolve_ir(self.authored(content), test_support=native_fixture(self.authored(content)))


if __name__ == "__main__":
    unittest.main()
