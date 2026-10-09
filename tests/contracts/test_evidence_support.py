"""Shared real ARM link fixture for evidence behavior tests."""

import os
from pathlib import Path
import shutil
import subprocess
import unittest

from tools.evidence.common import atomic_json, digest
from tools.evidence.identity import canonical_digest


def link_fixture(root: Path):
    compiler = os.environ.get("NEXUS_ARM_GCC") or shutil.which("arm-none-eabi-gcc")
    if not compiler:
        raise unittest.SkipTest("real ARM compiler unavailable; ELF qualification not performed")
    resolved = {"schema_version": 1, "board": "test-board", "soc_family": "test-soc",
        "part": "STM32F407ZGT6", "backend": "baremetal",
        "controllers": [{"kind": "uart", "controller": "USART1", "baud": 115200,
                         "pins": [{"function": "tx", "pin": "PA9"},
                                  {"function": "rx", "pin": "PA10"}]}],
        "flash": {"origin": 0x08000000, "size": 0x100000},
        "memory": [{"id": "sram", "origin": 0x20000000, "size": 0x10000,
                    "linker": True}, {"id": "ccm", "origin": 0x10000000,
                    "size": 0x10000, "linker": False}]}
    config_path = root / "resolved.json"
    root.joinpath("image.c").write_text('''
typedef unsigned int u32;
extern u32 __nx_msp_end;
void Reset_Handler(void);
__attribute__((section(".isr_vector"), used))
const void* const vectors[] = {&__nx_msp_end, Reset_Handler};
volatile u32 initialized = 0x12345678;
volatile u32 zeroed[4];
void Reset_Handler(void) {
    zeroed[0] = initialized;
    for (;;) {
    }
}
''')
    resolved["inputs"] = [{"path": "image.c", "sha256": digest(root / "image.c")}]
    resolved["configuration_sha256"] = canonical_digest(resolved)
    atomic_json(config_path, resolved)
    atomic_json(root / "input_paths.json", {"image.c": str(root / "image.c")})
    digest_symbols = "\n".join(f"__nx_resolved_sha256_{i} = 0x{resolved['configuration_sha256'][i*8:i*8+8]};"
                               for i in range(8))
    root.joinpath("memory.ld").write_text('''
ENTRY(Reset_Handler)
MEMORY { FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
RAM (rw) : ORIGIN = 0x20000000, LENGTH = 64K }
__nx_msp_start = 0x2000fc00; __nx_msp_end = 0x20010000;
SECTIONS {
.isr_vector : { KEEP(*(.isr_vector)) } > FLASH
.text : { *(.text*) *(.rodata*) } > FLASH
.data : { *(.data*) } > RAM AT> FLASH
.bss (NOLOAD) : { *(.bss*) } > RAM
.heap (NOLOAD) : { __nx_heap_start = .; . += 64; __nx_heap_end = .; } > RAM
.msp 0x2000fc00 (NOLOAD) : { . += 1024; } > RAM
}
''' + digest_symbols)
    elf = root / "image.elf"
    subprocess.run([compiler, "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16",
        "-mfloat-abi=hard", "-nostdlib", "-Os", "-Wl,-Map=" + str(root / "image.map"),
        "-T", str(root / "memory.ld"), str(root / "image.c"), "-o", str(elf)],
        capture_output=True, check=True)
    budget = {"schema_version": 1, "flash_loaded_max": 4096, "flash_footprint_max": 4096,
        "ram_reserved_max": {"sram": 4096, "ccm": 0}, "msp_max": 1024, "heap_max": 64}
    budget_path = root / "budget.json"
    atomic_json(budget_path, budget)
    return elf, config_path, budget_path
