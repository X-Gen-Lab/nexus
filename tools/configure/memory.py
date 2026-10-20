"""Resolve optional exact RAM domains and render one allocation-free linker ABI.

Physical memory facts remain authoritative in the SoC package. Selection adds
only placement and per-domain budgets; it never changes bus-master reachability.
"""


def select(regions, budgets, family, part, *, obj, integer, fail):
    choices = obj(budgets.get("regions", {}), set(),
                  budgets.get("regions", {}).keys() if isinstance(
                      budgets.get("regions", {}), dict) else (),
                  "memory regions")
    available = {item["id"]: item for item in regions}
    maintained = ({"addsram", "tcm"} if (
        family == "gd32f470" and part == "GD32F470ZGT6") else
        {"ccm"} if family == "stm32f407" and part in {
            "STM32F407VET6", "STM32F407ZGT6"} else
        {"sram1"} if family == "stm32h743" and part == "STM32H743ZIT6"
        else set())
    for name, choice in choices.items():
        if name not in maintained or name not in available:
            fail(f"Unsupported optional memory region: {name}")
        obj(choice, {"limit_bytes"}, (), f"memory region {name}")
        limit = integer(choice["limit_bytes"], 1, available[name]["size"],
                        f"memory region {name} limit")
        if limit % 4:
            fail(f"Memory region {name} limit requires 4-byte alignment")
    # Keep the physical default domain first after optional domains become
    # selectable; MSP, libc storage and the primary budget stay in that domain.
    ordered = sorted(regions, key=lambda region: not region["linker"])
    return [{**region, "linker": region["linker"] or region["id"] in choices}
            for region in ordered]


def budget_limits(result):
    budgets = result["memory_budgets"]
    primary = next(region["id"] for region in result["memory"]
                   if region["linker"])
    choices = budgets.get("regions", {})
    return {region["id"]: (budgets.get("static_ram_bytes", region["size"])
            if region["id"] == primary else
            choices.get(region["id"], {}).get("limit_bytes", 0))
            for region in result["memory"]}


def resource_budget(result):
    """Project the sole resolved reservation and memory-limit contract.

    Generation and ELF verification share this pure projection. The emitted
    budget is a derived input; editing it cannot widen authored allowances.
    """
    budgets = result["memory_budgets"]
    flash_limit = budgets.get("flash_load_bytes", result["layout"]["size"])
    return {"schema_version": 1, "flash_loaded_max": flash_limit,
            "flash_footprint_max": flash_limit,
            "ram_reserved_max": budget_limits(result),
            "msp_max": budgets["main_stack_bytes"],
            "heap_max": budgets.get("libc_heap_bytes", 0)}


def selections(result):
    selected = {region["id"] for region in result["memory"] if region["linker"]}
    return [("NEXUS_MEMORY_" + name.upper(), int(name in selected))
            for name in ("addsram", "tcm", "ccm", "sram1")]


def header_lines(result):
    return [f"#define {name} {selected}" for name, selected in selections(result)]


def selection_lines(result):
    return [f"set({name} {selected})" for name, selected in selections(result)]


def _inputs(region, kind):
    stem = (f".nx_{region}_data" if kind == "data" else
            f".bss.nx_{region}" if kind == "bss" else
            f".bss.nx_{region}_noinit")
    return f"*({stem} {stem}.*)"


def _extra_sections(regions):
    sections = []
    for region in regions:
        name = region["id"]
        if not region["linker"]:
            section = f".nx_{name}_unselected"
            patterns = " ".join(_inputs(name, kind)
                                for kind in ("data", "noinit", "bss"))
            sections += [f"    {section} : {{ {patterns} }} > RAM :ram",
                         f'    ASSERT(SIZEOF({section}) == 0, '
                         '"memory region is not selected")']
            continue
        sections += [f"    __nx_{name}_region_start = ORIGIN({name.upper()});",
                     f"    __nx_{name}_region_end = ORIGIN({name.upper()}) + "
                     f"LENGTH({name.upper()});"]
        for kind in ("data", "bss", "noinit"):
            section = f".nx_{name}_{kind}"
            # NOINIT must precede .bss* wildcards, without being cleared by BSS.
            inputs = _inputs(name, kind)
            qualifier = "" if kind == "data" else " (NOLOAD)"
            load = "" if kind == "data" else f" AT(ADDR({section}))"
            # Output alignment also constrains AT> FLASH; aligning only the
            # location counter leaves byte-only initializers at an odd LMA.
            alignment = " ALIGN(4)" if kind == "data" else ""
            target = f" > {name.upper()}" + (" AT> FLASH" if kind == "data" else "")
            phdr = f" :{name}_" + ("data" if kind == "data" else "ram")
            sections += [f"    {section}{qualifier} :{load}{alignment} {{",
                         f"        . = ALIGN(4); __nx_{name}_{kind}_start = .;",
                         f"        {inputs}",
                         f"        . = ALIGN(4); __nx_{name}_{kind}_end = .;",
                         f"    }}{target}{phdr}"]
            if kind == "data":
                sections.append(f"    __nx_{name}_data_load = LOADADDR({section});")
    return "\n".join(sections)


def linker(result):
    ram = next(region for region in result["memory"] if region["linker"])
    flash = result["layout"]
    stack = result["memory_budgets"]["main_stack_bytes"]
    heap = result["memory_budgets"].get("libc_heap_bytes", 0)
    extras = [region for region in result["memory"] if region["id"] != ram["id"]]
    selected = [region for region in extras if region["linker"]]
    extra_phdrs = "\n".join(f"    {region['id']}_{name} PT_LOAD FLAGS(6);"
                            for region in selected for name in ("data", "ram"))
    extra_memory = "\n".join(f"    {region['id'].upper()} (rw) : ORIGIN = "
                             f"0x{region['origin']:08x}, LENGTH = {region['size']}"
                             for region in selected)
    extra_sections = _extra_sections(extras)
    # Only the selected H743 Flash provider consumes this immutable record.
    # Linker LONG expressions preserve physical addresses without C pointer-to-
    # integer initializers, static RAM, or product partition assumptions.
    flash_image_record = ""
    flash_ram_phdr = ""
    flash_ram_section = ""
    flash_workspace_bss = ""
    if result["soc_family"] == "stm32h743" and any(
            item["kind"] == "flash" for item in result["controllers"]):
        flash_image_record = """    .nx_flash_image (READONLY) : ALIGN(4) {
        g_nx_stm32h743_flash_image = .;
        LONG(__nx_flash_live_start)
        LONG(__nx_flash_live_end)
        LONG(__nx_flash_image_load_start)
        LONG(__nx_flash_image_load_end)
    } > FLASH :flash
    ASSERT(SIZEOF(.nx_flash_image) == 16, "Flash image record ABI differs")
"""
        # The cold Flash owner copies and verifies these exact bytes before
        # entering. RX and RW LOAD permissions remain separate; no section
        # name alone establishes the emitted instruction/literal closure.
        flash_ram_phdr = "    flash_ram PT_LOAD FLAGS(5);\n"
        flash_ram_section = """    .nx_flash_ram : ALIGN(4) {
        __nx_flash_ram_start = .;
        KEEP(*(.nx_flash_ram .nx_flash_ram.*))
        . = ALIGN(4);
        __nx_flash_ram_end = .;
    } > RAM AT> FLASH :flash_ram
    __nx_flash_ram_load_start = LOADADDR(.nx_flash_ram);
    __nx_flash_ram_load_end = LOADADDR(.nx_flash_ram) + SIZEOF(.nx_flash_ram);
"""
        # The prefix is an internal ABI shared with the checked C/ASM layout.
        # Collect it before .bss* so ordinary startup clears the entire owner.
        flash_workspace_bss = """. = ALIGN(1024);
        __nx_flash_ram_vector_start = .;
        KEEP(*(.bss.nx_h743_flash_workspace))
        __nx_flash_ram_workspace_end = .;
        __nx_flash_ram_vector_end = __nx_flash_ram_vector_start + 664;
        __nx_flash_ram_stack_start = __nx_flash_ram_vector_end;
        __nx_flash_ram_stack_end = __nx_flash_ram_stack_start + 1024;
        """
    return f'''/* Exact resolved physical layout; no product reservation. */
ENTRY(Reset_Handler)
PHDRS {{
    flash PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    ram PT_LOAD FLAGS(6);
    stack PT_LOAD FLAGS(6);
{flash_ram_phdr}{extra_phdrs}
}}
MEMORY {{
    FLASH (rx) : ORIGIN = 0x{flash["origin"]:08x}, LENGTH = {flash["size"]}
    RAM (rwx) : ORIGIN = 0x{ram["origin"]:08x}, LENGTH = {ram["size"]}
{extra_memory}
}}
__nx_flash_start = ORIGIN(FLASH);
__nx_flash_end = ORIGIN(FLASH) + LENGTH(FLASH);
__nx_flash_live_start = ORIGIN(FLASH);
__nx_flash_image_load_start = ORIGIN(FLASH);
_estack = ORIGIN(RAM) + LENGTH(RAM);
_sp = _estack;
_Min_Stack_Size = {stack};
_Min_Heap_Size = {heap};
__nx_msp_start = _estack - _Min_Stack_Size;
__nx_msp_end = _estack;
__nx_resolved_sha256_0 = 0x{result["configuration_sha256"][0:8]};
__nx_resolved_sha256_1 = 0x{result["configuration_sha256"][8:16]};
__nx_resolved_sha256_2 = 0x{result["configuration_sha256"][16:24]};
__nx_resolved_sha256_3 = 0x{result["configuration_sha256"][24:32]};
__nx_resolved_sha256_4 = 0x{result["configuration_sha256"][32:40]};
__nx_resolved_sha256_5 = 0x{result["configuration_sha256"][40:48]};
__nx_resolved_sha256_6 = 0x{result["configuration_sha256"][48:56]};
__nx_resolved_sha256_7 = 0x{result["configuration_sha256"][56:64]};
SECTIONS {{
    .isr_vector : {{ KEEP(*(.isr_vector)) KEEP(*(.vectors)) }} > FLASH :flash
    .text : {{ *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) }} > FLASH :flash
    .ARM.extab : {{ *(.ARM.extab*) }} > FLASH :flash
    .ARM.exidx : {{ __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; }} > FLASH :flash
    .preinit_array : {{ PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); }} > FLASH :flash
    .init_array : {{ PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); }} > FLASH :flash
    .fini_array : {{ PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); }} > FLASH :flash
{flash_image_record}    __nx_flash_live_end = .;
{extra_sections}
{flash_ram_section}    _sidata = LOADADDR(.data);
    .data : ALIGN(4) {{ . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; }} > RAM AT> FLASH :data
    __nx_flash_image_load_end = LOADADDR(.data) + SIZEOF(.data);
    .bss (NOLOAD) : AT(ADDR(.bss)) {{ . = ALIGN(4); _sbss = .; {flash_workspace_bss}*(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; }} > RAM :ram
    .reserved (NOLOAD) : AT(ADDR(.reserved)) {{ . = ALIGN(8); __heap_start = .; __nx_heap_start = .; . += _Min_Heap_Size; __heap_end = .; __nx_heap_end = .; }} > RAM :ram
    .msp __nx_msp_start (NOLOAD) : AT(__nx_msp_start) {{ . += _Min_Stack_Size; }} > RAM :stack
    PROVIDE(end = _ebss);
    PROVIDE(_end = _ebss);
    ASSERT(_ebss + _Min_Heap_Size <= _estack - _Min_Stack_Size, "RAM stack/heap overlap")
}}
'''
