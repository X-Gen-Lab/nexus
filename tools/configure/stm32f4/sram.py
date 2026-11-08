"""One explicit reset-backed SRAM placement, separate from physical chip RAM."""
from collections.abc import Mapping

from tools.configure.ir import ExternalMemoryRegionIR
from tools.configure.providers.common import fail, integer


def defer_budget(assembly, budgets, part) -> tuple[str, ...]:
    """Defer only the reviewed NE3 budget until its bank graph is immutable."""
    choices = budgets.get("regions", {})
    if not isinstance(choices, Mapping) or "addsram" not in choices:
        return ()
    if (part != "STM32F407ZGT6" or assembly.backend != "baremetal" or
            assembly.layout is not None or budgets.get("mpu") is not None or
            budgets.get("image_prefix_bytes", 0) or
            budgets.get("image_suffix_bytes", 0)):
        fail("F407 addsram startup requires baremetal full reset image without layout or MPU")
    if budgets["main_stack_bytes"] < 512:
        fail("F407 addsram startup requires at least 512 bytes of internal MSP")
    if assembly.optimization.endswith("-lto"):
        fail("F407 addsram startup LTO requires an actual post-LTO stack proof")
    choice = choices["addsram"]
    if not isinstance(choice, Mapping) or set(choice) != {"limit_bytes"}:
        fail("F407 addsram budget requires only explicit limit_bytes")
    limit = integer(choice["limit_bytes"], 1, 0x100000,
                    "F407 addsram budget limit_bytes")
    if limit % 4:
        fail("F407 addsram budget requires four-byte alignment")
    return ("addsram",)


def resolve(budget, devices, *, assembly, variant) -> tuple[ExternalMemoryRegionIR, ...]:
    """Bind one selected exact startup child, without cloning its geometry."""
    children = [item for item in devices
                if item.driver == "memory_window-endpoint" and
                item.options.mode == "startup-memory"]
    limits = [item for item in budget.regions if item.id == "addsram"]
    if not limits and not children:
        return ()
    if not limits:
        fail("Selected SRAM startup-memory requires an explicit addsram budget")
    if len(children) != 1:
        fail("F407 addsram placement requires exactly one startup-memory child")
    child = children[0]
    if (child.options.address != 0x68000000 or
            child.options.bytes != 0x100000 or
            limits[0].limit_bytes > child.options.bytes or
            limits[0].limit_bytes % 4 or
            variant["flash"]["origin"] != 0x08000000 or
            variant["flash"]["size"] != 0x100000 or
            assembly.backend != "baremetal"):
        fail("F407 addsram startup differs from exact NE3 1MiB/full Flash geometry")
    return (ExternalMemoryRegionIR("addsram", child),)


def declarations(result):
    """Emit immutable Flash plan and actual linker symbols, never MMIO code."""
    if not result.external_memory:
        return []
    from .fsmc import boot_bank_reference, boot_declarations
    primary = ("__nx_primary_region_start", "__nx_primary_region_end",
               "_sidata", "_sdata", "_edata", "_sbss", "_ebss", "_ebss", "_ebss")
    selected = ["ccm"] if any(region["id"] == "ccm" and region["linker"]
                              for region in result.physical_part.memory) else []
    selected += ["addsram"]
    regions = [primary]
    for name in selected:
        regions.append(tuple("__nx_" + name + "_" + suffix for suffix in
            ("region_start", "region_end", "data_load", "data_start", "data_end",
             "bss_start", "bss_end", "noinit_start", "noinit_end")))
    symbols = sorted({symbol for region in regions for symbol in region} |
                     {"__nx_flash_start", "__nx_flash_end", "__nx_msp_start", "_estack"})
    lines = [*boot_declarations(result),
             *[f"extern uint8_t {symbol};" for symbol in symbols],
             "extern const uint32_t g_pfnVectors[];",
             "const nx_stm32_sram_boot_plan_t g_nx_stm32_sram_boot_plan = {",
             "    .version = NX_STM32_SRAM_BOOT_VERSION,",
             "    .part = NX_STM32_SRAM_IS62WV51216,",
             "    .maximum_hclk_hz = 168000000u,",
             f"    .region_count = {len(regions)}u,",
             "    .flash_start = (uintptr_t)&__nx_flash_start,",
             "    .flash_end = (uintptr_t)&__nx_flash_end,",
             "    .vector_address = (uintptr_t)g_pfnVectors,",
             "    .msp_start = (uintptr_t)&__nx_msp_start,",
             "    .msp_end = (uintptr_t)&_estack,",
             f"    .bank = {boot_bank_reference(result)},",
             "    .regions = {"]
    for region in regions:
        lines.append("        { " + ", ".join("(uintptr_t)&" + symbol
                                                for symbol in region) + " },")
    return lines + ["    }", "};"]
