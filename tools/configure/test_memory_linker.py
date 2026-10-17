"""Actual GNU links prove word-copy alignment and separate RAM ownership."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from tools.configure import configure as gate


PROFILES = (
    ("stm32f407_qiming_v31", "hse8-pll168", ()),
    ("stm32f407ve_sky_qingchun", "hse8-pll168", ()),
    ("gd32f470_liangshan", "hxtal25-pll200", ()),
    ("stm32g474re_hse24_reference", "hse24-pll170", ()),
    ("stm32h743_nucleo_zi2", "hsi64-fixed-cache-off", ()),
    ("stm32u575_nucleo_zi_q", "msis4", ()),
    ("stm32f407_qiming_v31", "hse8-pll168", ("ccm",)),
    ("stm32f407ve_sky_qingchun", "hse8-pll168", ("ccm",)),
    ("gd32f470_liangshan", "hxtal25-pll200", ("addsram",)),
    ("gd32f470_liangshan", "hxtal25-pll200", ("tcm",)),
    ("gd32f470_liangshan", "hxtal25-pll200", ("addsram", "tcm")),
)


class MemoryLinkerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.storage = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.storage.cleanup)
        cls.root = Path(cls.storage.name)
        host = shutil.which("gcc")
        if not host:
            raise AssertionError("Actual GNU host compiler is required")
        compilers = [(host, [])]
        arm = (os.environ.get("NEXUS_ARM_GCC") or
               shutil.which("arm-none-eabi-gcc"))
        if arm:
            compilers.append((arm, ["-mthumb"]))
        cls.compilers = compilers
        cls.images = []
        for compiler, flags in compilers:
            profiles = [(*profile, 1) for profile in PROFILES]
            profiles += [(*profile, alignment) for alignment in (8, 32)
                         for profile in (PROFILES[6], PROFILES[10])]
            for board, clock, extras, alignment in profiles:
                directory = cls.root / str(len(cls.images))
                directory.mkdir()
                cls.images.append(cls.link(directory, compiler, flags,
                                           board, clock, extras, alignment))

    @staticmethod
    def command(argv, directory):
        result = subprocess.run(argv, cwd=directory, capture_output=True,
                                text=True, check=False, timeout=30)
        if result.returncode:
            raise AssertionError(f"{argv!r}\n{result.stdout}\n{result.stderr}")
        return result.stdout

    @classmethod
    def tool(cls, compiler, name, directory):
        return cls.command([compiler, "-print-prog-name=" + name],
                           directory).strip()

    @classmethod
    def link(cls, directory, compiler, flags, board, clock, extras, alignment,
             selected_flash=False, ram_pod=False):
        assembly = directory / "assembly.toml"
        assembly.write_text(
            f'schema=2\nboard="{board}"\nbackend="baremetal"\n'
            f'clock="{clock}"\n[memory]\nmain_stack_bytes=2048\n'
            'libc_heap_bytes=16\nstatic_ram_limit_bytes=4096\n' +
            "".join(f'[memory.regions.{name}]\nlimit_bytes=128\n'
                    for name in extras) +
            ('[flash.storage]\nbinding="flash0"\nmode="poll-word"\n'
             if selected_flash else ""))
        bundle = directory / "bundle"
        resolved = gate.configure(assembly, bundle)
        if flags:
            flags = [*flags, "-mcpu=" + resolved["cpu"]["arch"]]
        # Byte-only input sections exercise the linker contract without relying
        # on a C object's natural alignment to hide an odd Flash load cursor.
        source = directory / "image.c"
        source.write_text(
            'void Reset_Handler(void) { for (;;) {} }\n'
            f'__attribute__((aligned({alignment})))\n'
            'unsigned char primary_data = 0x73;\n'
            'unsigned char primary_bss[3];\n' +
            ('__attribute__((section(".nx_flash_ram"), noinline))\n'
             'void ram_leaf(void) { __asm__ volatile ("" ::: "memory"); }\n'
             '__attribute__((section(".bss.nx_h743_flash_workspace"), '
             'aligned(1024)))\n'
             'unsigned char g_nx_stm32h743_flash_ram_workspace[2048];\n'
             if ram_pod else "") +
            "".join(
                f'__attribute__((section(".nx_{name}_data"), '
                f'aligned({alignment})))\n'
                f'unsigned char {name}_data = 0x5a;\n'
                f'__attribute__((section(".bss.nx_{name}"), '
                f'aligned({alignment})))\n'
                f'unsigned char {name}_bss[3];\n'
                f'__attribute__((section(".bss.nx_{name}_noinit"), '
                f'aligned({alignment})))\n'
                f'unsigned char {name}_noinit[3];\n'
                for name in extras))
        tail = directory / "tail.s"
        tail.write_text('.section .fini_array,"a",%progbits\n'
                        '.balign 4\n.byte 0x39\n')
        elf = directory / "image.elf"
        cls.command([compiler, *flags, "-Os", "-fno-pie", "-no-pie",
                     "-ffunction-sections", "-fdata-sections", "-fno-common",
                     "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
                     "-nostdlib", "-Wl,--build-id=none", "-T",
                     str(bundle / "memory.ld"), str(source), str(tail),
                     "-o", str(elf)], directory)
        table = cls.command([cls.tool(compiler, "objdump", directory),
                             "-h", str(elf)], directory)
        section_pattern = re.compile(
            r"^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+"
            r"([0-9a-f]+)\s+([0-9a-f]+)\s+2\*\*(\d+)\n([^\n]*)",
            re.MULTILINE)
        sections = {
            match[1]: {"size": int(match[2], 16), "vma": int(match[3], 16),
                       "lma": int(match[4], 16), "offset": int(match[5], 16),
                       "alignment": 1 << int(match[6]), "flags": match[7]}
            for match in section_pattern.finditer(table)}
        table = cls.command([cls.tool(compiler, "nm", directory), "-P",
                             str(elf)], directory)
        symbols = {entry[0]: int(entry[2], 16) for line in table.splitlines()
                   if len(entry := line.split()) >= 3}
        table = cls.command([cls.tool(compiler, "readelf", directory), "-lW",
                             str(elf)], directory)
        loads = []
        for line in table.splitlines():
            fields = line.split()
            if fields and fields[0] == "LOAD":
                load = dict(zip(("offset", "vma", "lma", "files", "size"),
                                (int(item, 16) for item in fields[1:6])))
                load["flags"] = "".join(fields[6:-1])
                loads.append(load)
        return {"label": (compiler, board, extras, alignment),
                "sections": sections, "alignment": alignment,
                "symbols": symbols, "loads": loads, "bytes": elf.read_bytes(),
                "resolved": resolved, "extras": extras}

    def test_flash_image_bounds_cover_live_code_and_every_load_initializer(self):
        for image in self.images:
            with self.subTest(image=image["label"]):
                symbols = image["symbols"]
                names = ("__nx_flash_live_start", "__nx_flash_live_end",
                         "__nx_flash_image_load_start", "__nx_flash_image_load_end")
                self.assertTrue(all(name in symbols for name in names),
                                "linked image must expose physical protection bounds")
                live_start, live_end, load_start, load_end = (
                    symbols[name] for name in names)
                physical_start = image["resolved"]["layout"]["origin"]
                physical_end = physical_start + image["resolved"]["layout"]["size"]
                self.assertEqual(load_start, physical_start)
                self.assertEqual(live_start, load_start)
                self.assertLess(live_start, live_end)
                self.assertLessEqual(live_end, load_end)
                self.assertLessEqual(load_end, physical_end)
                for section in image["sections"].values():
                    if "CONTENTS" not in section["flags"] or not section["size"]:
                        continue
                    if physical_start <= section["lma"] < physical_end:
                        self.assertLessEqual(load_start, section["lma"])
                        self.assertLessEqual(section["lma"] + section["size"], load_end)
                    if physical_start <= section["vma"] < physical_end:
                        self.assertLessEqual(live_start, section["vma"])
                        self.assertLessEqual(section["vma"] + section["size"], live_end)
                for load in image["loads"]:
                    if load["files"] and physical_start <= load["lma"] < physical_end:
                        self.assertLessEqual(load["lma"] + load["files"], load_end)

    def test_selected_h743_flash_embeds_exact_readonly_image_descriptor(self):
        for image in self.images:
            self.assertNotIn("g_nx_stm32h743_flash_image", image["symbols"])
        for index, (compiler, flags) in enumerate(self.compilers):
            with self.subTest(compiler=compiler):
                directory = self.root / ("selected-flash-" + str(index))
                directory.mkdir()
                image = self.link(directory, compiler, flags,
                                  "stm32h743zi_hsi_reference",
                                  "hsi64-fixed-cache-off", (), 1,
                                  selected_flash=True)
                self.assertIn("g_nx_stm32h743_flash_image", image["symbols"])
                section = image["sections"][".nx_flash_image"]
                self.assertEqual(section["size"], 16)
                self.assertIn("READONLY", section["flags"])
                self.assertEqual(image["symbols"]["g_nx_stm32h743_flash_image"],
                                 section["vma"])
                data = image["bytes"][section["offset"]:section["offset"] + 16]
                names = ("__nx_flash_live_start", "__nx_flash_live_end",
                         "__nx_flash_image_load_start", "__nx_flash_image_load_end")
                self.assertEqual([int.from_bytes(data[n:n + 4], "little")
                                  for n in range(0, 16, 4)],
                                 [image["symbols"][name] for name in names])
                self.assertGreaterEqual(image["symbols"][names[1]],
                                        section["vma"] + section["size"])
                self.assertEqual(image["symbols"][names[3]],
                                 image["sections"][".data"]["lma"] +
                                 image["sections"][".data"]["size"])

    def test_primary_initializer_load_and_run_addresses_are_word_aligned(self):
        for image in self.images:
            with self.subTest(image=image["label"]):
                tail = image["sections"][".fini_array"]
                self.assertEqual((tail["lma"] + tail["size"]) % 4, 1)
                self.assert_data(image, ".data", "_sidata", "_sdata", "_edata",
                                 "primary_data", 0x73)

    def test_selected_ram_pod_has_exact_zeroed_vectors_stack_and_rx_copy(self):
        for index, (compiler, flags) in enumerate(self.compilers):
            with self.subTest(compiler=compiler):
                directory = self.root / ("selected-ram-pod-" + str(index))
                directory.mkdir()
                image = self.link(directory, compiler, flags,
                                  "stm32h743zi_hsi_reference",
                                  "hsi64-fixed-cache-off", (), 1,
                                  selected_flash=True, ram_pod=True)
                symbols = image["symbols"]
                required = ("__nx_flash_ram_vector_start",
                            "__nx_flash_ram_vector_end",
                            "__nx_flash_ram_stack_start",
                            "__nx_flash_ram_stack_end",
                            "__nx_flash_ram_workspace_end")
                self.assertTrue(all(name in symbols for name in required),
                                "selected physical workspace bounds are missing")
                start, vectors_end, stack_start, stack_end, end = (
                    symbols[name] for name in required)
                self.assertEqual(start % 1024, 0)
                self.assertEqual(start, symbols[
                    "g_nx_stm32h743_flash_ram_workspace"])
                self.assertEqual(vectors_end - start, 166 * 4)
                self.assertEqual(stack_start, vectors_end)
                self.assertEqual(stack_end - stack_start, 256 * 4)
                self.assertEqual(stack_end % 8, 0)
                self.assertEqual(end - start, 2048)
                self.assertLessEqual(symbols["_sbss"], start)
                self.assertLessEqual(end, symbols["_ebss"])
                self.assertLessEqual(symbols["__nx_flash_ram_end"], start)
                section = image["sections"][".nx_flash_ram"]
                self.assertGreater(section["size"], 0)
                self.assertEqual(section["lma"], symbols[
                    "__nx_flash_ram_load_start"])
                self.assertEqual(section["lma"] + section["size"], symbols[
                    "__nx_flash_ram_load_end"])
                self.assertLessEqual(symbols["__nx_flash_ram_load_end"], symbols[
                    "__nx_flash_image_load_end"])
                code_loads = [load for load in image["loads"]
                              if load["vma"] <= section["vma"] and
                              section["vma"] + section["size"] <=
                              load["vma"] + load["files"]]
                self.assertEqual(len(code_loads), 1)
                self.assertEqual(code_loads[0]["flags"], "RE")
                self.assertTrue(all(load["flags"] != "RWE"
                                    for load in image["loads"]))

    def test_optional_initializer_load_and_run_addresses_are_word_aligned(self):
        for image in self.images:
            for region in image["extras"]:
                with self.subTest(image=image["label"], region=region):
                    stem = f"__nx_{region}_data"
                    self.assert_data(image, f".nx_{region}_data",
                                     stem + "_load", stem + "_start",
                                     stem + "_end", region + "_data", 0x5a)

    def assert_data(self, image, name, load, start, end, variable, initial):
        section, symbols = image["sections"][name], image["symbols"]
        self.assertEqual(section["lma"] % 4, 0, "word-copy source is unaligned")
        self.assertEqual(section["vma"] % 4, 0)
        self.assertGreaterEqual(section["alignment"], 4)
        self.assertEqual(symbols[load], section["lma"])
        self.assertEqual(symbols[start], section["vma"])
        self.assertEqual(symbols[end], section["vma"] + section["size"])
        self.assertEqual(section["size"] % 4, 0)
        self.assertEqual(symbols[variable], symbols[start])
        self.assertEqual(image["bytes"][section["offset"]], initial)
        matches = [item for item in image["loads"]
                   if item["vma"] <= section["vma"] and
                   section["vma"] + section["size"] <=
                   item["vma"] + item["files"]]
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]["lma"] + section["vma"] - matches[0]["vma"],
                         section["lma"])

    def test_bss_and_noinit_remain_separate_unloaded_regions(self):
        for image in self.images:
            with self.subTest(image=image["label"]):
                symbols = image["symbols"]
                bss = image["sections"][".bss"]
                self.assertNotIn("CONTENTS", bss["flags"])
                self.assertEqual(symbols["_sbss"], bss["vma"])
                self.assertEqual(symbols["_ebss"], bss["vma"] + bss["size"])
                for region in image["extras"]:
                    intervals = []
                    for kind in ("data", "bss", "noinit"):
                        section = image["sections"][f".nx_{region}_{kind}"]
                        start = symbols[f"__nx_{region}_{kind}_start"]
                        end = symbols[f"__nx_{region}_{kind}_end"]
                        self.assertEqual(start % 4, 0)
                        self.assertEqual(end % 4, 0)
                        self.assertLess(start, end)
                        self.assertEqual(start, section["vma"])
                        self.assertEqual(end, start + section["size"])
                        if kind != "data":
                            self.assertNotIn("CONTENTS", section["flags"])
                        intervals.append((start, end))
                    self.assertLessEqual(intervals[0][1], intervals[1][0])
                    self.assertLessEqual(intervals[1][1], intervals[2][0])
                    self.assertFalse(any(start <= symbols["primary_bss"] < end
                                         for start, end in intervals))

    def test_heap_and_msp_preserve_alignment_size_and_noload(self):
        for image in self.images:
            with self.subTest(image=image["label"]):
                symbols = image["symbols"]
                self.assertEqual(symbols["__nx_heap_start"] % 8, 0)
                self.assertEqual(symbols["__nx_heap_end"] -
                                 symbols["__nx_heap_start"], 16)
                self.assertGreaterEqual(symbols["__nx_heap_start"],
                                        symbols["_ebss"])
                self.assertLessEqual(symbols["__nx_heap_end"],
                                     symbols["__nx_msp_start"])
                self.assertEqual(symbols["__nx_msp_end"] -
                                 symbols["__nx_msp_start"], 2048)
                self.assertEqual(symbols["_estack"], symbols["__nx_msp_end"])
                for name in (".reserved", ".msp"):
                    self.assertNotIn("CONTENTS",
                                     image["sections"][name]["flags"])

    def test_stronger_input_alignment_is_preserved(self):
        for image in self.images:
            with self.subTest(image=image["label"]):
                alignment = image["alignment"]
                variables = {"primary_data": ".data"}
                variables.update({f"{region}_{kind}": f".nx_{region}_{kind}"
                                  for region in image["extras"]
                                  for kind in ("data", "bss", "noinit")})
                for variable, section in variables.items():
                    self.assertEqual(image["symbols"][variable] % alignment, 0)
                    self.assertGreaterEqual(
                        image["sections"][section]["alignment"], alignment)


if __name__ == "__main__":
    unittest.main()
