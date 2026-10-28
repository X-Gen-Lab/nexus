"""Protected U575 memory emits exact domains for the real reset startup."""

from dataclasses import replace
import re
import unittest

from tools.configure import configure, memory
from tools.configure.ir import MemoryBudgetIR, MpuMemoryReservationsIR, freeze


def resolved(protected=True):
    result = configure.resolve_ir(configure.ROOT /
        "tools/configure/assemblies/nucleo-u575-baremetal-empty.toml")
    reservations = MpuMemoryReservationsIR(
        privileged_flash_bytes=131072, syscall_flash_bytes=256,
        user_flash_bytes=2097152 - 131072 - 256,
        privileged_ram_bytes=163840, user_ram_bytes=32768) if protected else None
    return replace(result,
        layout=freeze({"origin": 0x08000000, "size": 2097152}),
        physical_part=replace(result.physical_part, memory=freeze([
            {"id": "sram1", "origin": 0x20000000,
             "size": 196608, "linker": True},
            {"id": "sram4", "origin": 0x28000000,
             "size": 16384, "linker": False}])),
        memory_budget=MemoryBudgetIR(2048, 0, None, None, mpu=reservations),
        configuration_sha256="12" * 32)


class MpuMemoryLinkerTests(unittest.TestCase):
    def test_five_exact_disjoint_domains_preserve_physical_part_extent(self):
        source = memory.linker(resolved())
        pattern = (r"^    (\w+) \([^)]*\) : ORIGIN = 0x([0-9a-f]+), "
                   r"LENGTH = (\d+)$")
        domains = {name: (int(start, 16), int(size))
                   for name, start, size in re.findall(pattern, source,
                                                       re.MULTILINE)}
        self.assertEqual(domains, {
            "PRIV_FLASH": (0x08000000, 131072),
            "SVC_FLASH": (0x08020000, 256),
            "USER_FLASH": (0x08020100, 2097152 - 131072 - 256),
            "PRIV_RAM": (0x20000000, 163840),
            "USER_RAM": (0x20028000, 32768),
        })
        self.assertIn("__nx_flash_end = 0x08200000;", source)

    def test_primary_startup_and_msp_stay_inside_privileged_ram(self):
        source = memory.linker(resolved())
        self.assertIn("_estack = ORIGIN(PRIV_RAM) + LENGTH(PRIV_RAM);", source)
        self.assertIn("_sidata = LOADADDR(.data);", source)
        self.assertRegex(source, r"\.data : ALIGN\(4\).*?_sdata = .*?"
                         r"privileged_data.*?_edata = .*?> PRIV_RAM "
                         r"AT> PRIV_FLASH :data")
        self.assertRegex(source, r"\.bss \(NOLOAD\).*?_sbss = .*?"
                         r"_ebss = .*?> PRIV_RAM :ram")
        self.assertIn(".msp __nx_msp_start (NOLOAD)", source)
        self.assertIn("RAM stack/heap overlap", source)

    def test_user_initializers_and_bss_have_separate_boot_bounds(self):
        source = memory.linker(resolved())
        self.assertIn("__nx_mpu_user_data_load = LOADADDR(.nx_mpu_user_data);",
                      source)
        for kind in ("data", "bss"):
            self.assertIn(f"__nx_mpu_user_{kind}_start = .;", source)
            self.assertIn(f"__nx_mpu_user_{kind}_end = .;", source)
        self.assertIn("*(.nexus_user_data .nexus_user_data.*)", source)
        self.assertIn("*(.bss.nexus_user_data .bss.nexus_user_data.*)", source)
        self.assertIn("*(.nexus_user_bss .nexus_user_bss.*)", source)
        self.assertLess(source.index(".nx_mpu_user_data"),
                        source.index("_sidata ="))
        self.assertLess(source.index(".nx_mpu_user_bss"),
                        source.index("_sbss ="))
        self.assertRegex(source, r"\.nx_mpu_user_data : ALIGN\(32\).*?"
                         r"> USER_RAM AT> PRIV_FLASH :user_data")
        self.assertIn("__nx_mpu_user_data_start & 31", source)

    def test_half_open_nexus_spans_match_inclusive_v8_port_aliases(self):
        source = memory.linker(resolved())
        for name, domain in (("privileged_flash", "PRIV_FLASH"),
                             ("privileged_ram", "PRIV_RAM"),
                             ("user_flash", "USER_FLASH"),
                             ("user_ram", "USER_RAM"),
                             ("syscall_flash", "SVC_FLASH")):
            self.assertIn(f"__nexus_{name}_start__ = ORIGIN({domain});", source)
            self.assertIn(f"__nexus_{name}_end__ = ORIGIN({domain}) + "
                          f"LENGTH({domain});", source)
        for port, nexus in (("privileged_functions", "privileged_flash"),
                            ("privileged_sram", "privileged_ram"),
                            ("unprivileged_flash", "user_flash"),
                            ("syscalls_flash", "syscall_flash")):
            self.assertIn(f"__{port}_end__ = __nexus_{nexus}_end__ - 1;", source)
        self.assertIn("*(freertos_system_calls freertos_system_calls.*)", source)
        self.assertIn("*(.nexus_user_text .nexus_user_text.*)", source)
        self.assertLess(source.index(".nexus_user_text"),
                        source.index("*(.text*)"))
        self.assertIn("*(privileged_functions privileged_functions.*)", source)

    def test_normal_linker_does_not_gain_user_permissions_or_initializers(self):
        source = memory.linker(resolved(False))
        self.assertNotIn("__nexus_user_flash", source)
        self.assertNotIn("__nx_mpu_user_data", source)
        self.assertIn("_estack = ORIGIN(RAM) + LENGTH(RAM);", source)
        self.assertIn("ENTRY(Reset_Handler)", source)


if __name__ == "__main__":
    unittest.main()
