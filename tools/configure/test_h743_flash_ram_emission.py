"""Normal H743 assembly owns optional physical Flash RAM execution storage.

These tests establish generated ownership and linker contracts. Host execution
and inspection of the actual provider ELF establish their separate boundaries.
"""

from pathlib import Path
import re
import tempfile
import tomllib
import unittest

from tools.configure import bindings, configure, factory, memory


ASSEMBLIES = configure.ROOT / "cmake/package/verification/io_evolution"
FLASH_ASSEMBLY = (configure.ROOT /
                  "tools/configure/assemblies/h743-high-peripherals.toml")
WORKSPACE = "g_nx_stm32h743_flash_ram_workspace"


class H743FlashRamEmissionTests(unittest.TestCase):
    def resolve(self, name="h743-high-peripherals"):
        path = (FLASH_ASSEMBLY if name == "h743-high-peripherals" else
                ASSEMBLIES / (name + ".toml"))
        return configure.resolve_ir(path)

    def test_selected_flash_emits_one_aligned_private_physical_workspace(self):
        source = bindings.emit(self.resolve())[1]
        self.assertIn('#include "stm32h743_flash_ram.h"', source)
        declarations = re.findall(
            r"_Alignas\(1024\)\s+nx_stm32h743_flash_ram_workspace_t\s+"
            + WORKSPACE + r"\s*;", source)
        self.assertEqual(1, len(declarations))
        self.assertIn('__attribute__((section(".bss.nx_h743_flash_workspace")))',
                      source)
        self.assertNotIn("malloc(", source)
        self.assertNotIn("calloc(", source)

    def test_normal_prepare_and_stop_route_through_the_owned_workspace(self):
        source = bindings.emit(self.resolve())[1]
        self.assertRegex(
            source, r"nx_stm32h743_flash_prepare\("
            r"&s_nx_port_storage, &" + WORKSPACE + r", 3300u\)")
        self.assertIn("nx_stm32h743_flash_ram_stop(&" + WORKSPACE + ")",
                      source)
        self.assertNotIn("return nx_stm32h743_flash_stop(&s_nx_port_storage);",
                         source)

    def test_failed_cold_owner_is_included_in_platform_cleanup(self):
        source = bindings.emit(self.resolve())[1]
        self.assertRegex(
            source, r"s_nx_platform_initialized = [0-9]+u;\s*"
            r"result.primary = nx_stm32h743_flash_prepare\(&s_nx_port_storage,")

    def test_physical_workspace_is_independent_of_the_public_instance_name(self):
        original = FLASH_ASSEMBLY.read_text()
        board = tomllib.loads(original)["board_package"]
        original = original.replace(
            'board_package = "' + board + '"',
            'board_package = "' +
            str((FLASH_ASSEMBLY.parent / board).resolve()) + '"')
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "renamed.toml"
            path.write_text(original.replace("[flash.storage]",
                                             "[flash.maintenance]"))
            source = bindings.emit(configure.resolve_ir(path))[1]
        self.assertIn(WORKSPACE, source)
        self.assertIn("nx_stm32h743_flash_prepare(&s_nx_port_maintenance, &"
                      + WORKSPACE + ", 3300u)", source)
        self.assertNotIn("&s_nx_port_storage", source)

    def test_selected_ram_code_has_rx_load_and_exact_copy_symbols(self):
        script = memory.linker(self.resolve())
        self.assertRegex(script, r"flash_ram\s+PT_LOAD FLAGS\(5\);")
        for symbol in ("__nx_flash_ram_start", "__nx_flash_ram_end",
                       "__nx_flash_ram_load_start", "__nx_flash_ram_load_end"):
            self.assertIn(symbol, script)
        self.assertIn(".nx_flash_ram", script)
        self.assertRegex(script, r"> RAM AT> FLASH :flash_ram")
        self.assertNotRegex(script, r"PT_LOAD FLAGS\(7\)")

    def test_code_copy_precedes_data_and_remains_in_protected_load_image(self):
        script = memory.linker(self.resolve())
        self.assertIn(".nx_flash_ram :", script)
        self.assertLess(script.index(".nx_flash_ram :"),
                        script.index("_sidata = LOADADDR(.data)"))
        self.assertLess(script.index("__nx_flash_ram_load_end"),
                        script.index("__nx_flash_image_load_end ="))
        self.assertIn("__nx_flash_image_load_end = LOADADDR(.data) + "
                      "SIZEOF(.data);", script)

    def test_unselected_high_image_has_no_new_workspace_or_copy_contract(self):
        result = self.resolve("h743-high-empty")
        source = bindings.emit(result)[1]
        script = memory.linker(result)
        for token in (WORKSPACE, "stm32h743_flash_ram.h",
                      "nx_stm32h743_flash_ram_initialize"):
            self.assertNotIn(token, source)
        self.assertNotIn(".nx_flash_ram", script)
        self.assertNotIn("__nx_flash_ram_", script)

    def test_factory_getters_remain_cold_and_public_types_remain_unchanged(self):
        result = self.resolve()
        source = factory.emit(result)[1]
        self.assertNotIn("flash_ram_initialize", source)
        self.assertNotIn("flash_ram_workspace", source)
        self.assertIn("nx_flash_port_t", source)


if __name__ == "__main__":
    unittest.main()
