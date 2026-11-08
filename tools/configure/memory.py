"""Resolve optional exact RAM domains and render one allocation-free linker ABI.

Internal physical memory facts remain authoritative in the SoC package. A
reviewed selected external bank retains its own immutable backing reference.
Placement and per-domain budgets never add bus-master reachability.
"""


def select(regions, budgets, maintained, *, obj, integer, fail):
    choices = obj(budgets.get("regions", {}), set(),
                  budgets.get("regions", {}).keys() if isinstance(
                      budgets.get("regions", {}), dict) else (),
                  "memory regions")
    available = {item["id"]: item for item in regions}
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


def flash_geometry(result, *, symbol, sector_symbol, program_unit,
                   program_flags="0u", static=True):
    """Emit one readonly physical geometry from the selected immutable facts."""
    flash = result.physical_part.flash
    lines = [f"static const nx_flash_sector_t {sector_symbol}[] = {{"]
    offset = 0
    for block in flash["erase_blocks"]:
        lines.append(f"    {{ {offset}u, {block}u }},")
        offset += block
    storage = "static " if static else ""
    return lines + ["};", f"{storage}const nx_flash_geometry_t {symbol} = {{",
                   f"    .base_address = 0x{flash['origin']:08x}u,",
                   f"    .size = {flash['size']}u,",
                   f"    .program_unit = {program_unit}u,",
                   f"    .sectors = {sector_symbol},",
                   f"    .sector_count = {len(flash['erase_blocks'])}u,",
                   f"    .program_flags = {program_flags}", "};"]


def _budget_limits(memory, static_limit, regions):
    primary = next(region["id"] for region in memory if region["linker"])
    return {region["id"]: (
        static_limit if static_limit is not None else region["size"])
        if region["id"] == primary else regions.get(region["id"], 0)
        for region in memory}


def budget_limits(result):
    budget = result.memory_budget
    return _budget_limits(result.memory, budget.static_ram_limit_bytes,
                          {item.id: item.limit_bytes for item in budget.regions})


def _resource_budget(flash_limit, ram_limits, stack, heap):
    return {"schema_version": 1, "flash_loaded_max": flash_limit,
            "flash_footprint_max": flash_limit, "ram_reserved_max": ram_limits,
            "msp_max": stack, "heap_max": heap}


def resource_budget(result):
    """Export the selected typed reservation and memory-limit contract."""
    budget = result.memory_budget
    flash_limit = (budget.flash_load_limit_bytes
                   if budget.flash_load_limit_bytes is not None
                   else result.image_size)
    return _resource_budget(flash_limit, budget_limits(result),
                            budget.main_stack_bytes, budget.libc_heap_bytes)


def resource_budget_projection(resolved):
    """Read a sealed JSON contract for independent ELF budget verification.

    This never reconstructs executable configuration or admits hardware modes.
    The caller validates the physical memory and complete expected budget.
    """
    budget = resolved["memory_budgets"]
    limits = _budget_limits(resolved["memory"], budget.get("static_ram_bytes"),
        {name: item["limit_bytes"] for name, item in budget.get("regions", {}).items()})
    return _resource_budget(
        budget.get("flash_load_bytes", resolved["layout"]["size"] -
                   budget.get("image_prefix_bytes", 0) -
                   budget.get("image_suffix_bytes", 0)), limits,
        budget["main_stack_bytes"], budget.get("libc_heap_bytes", 0))


def validate_external_projection(resolved):
    """Check sealed selected backing without extending physical chip facts.

    This reads the resolver's one-way audit projection. It does not re-resolve
    configuration, infer a PCB route or qualify external memory electrically.
    """
    external = resolved.get("external_memory", [])
    selected = [region for region in resolved.get("memory", [])
                if region.get("id") == "addsram"]
    if not external:
        if resolved.get("soc_family") == "stm32f4" and selected:
            raise ValueError("F407 addsram requires explicit external startup backing")
        return
    error = "Invalid external SRAM startup projection"
    if (resolved.get("soc_family") != "stm32f4" or
            resolved.get("part") != "STM32F407ZGT6" or
            resolved.get("backend") != "baremetal" or
            resolved.get("optimization", "Os") not in {"O0", "Og", "O2", "Os", "O3"} or
            not isinstance(external, list) or len(external) != 1 or
            set(external[0]) != {"id", "endpoint"} or
            external[0]["id"] != "addsram"):
        raise ValueError(error)
    endpoint = external[0]["endpoint"]
    if (not isinstance(endpoint, dict) or
            endpoint.get("driver") != "memory_window-endpoint" or
            endpoint.get("mode") != "startup-memory" or
            endpoint.get("address") != 0x68000000 or
            endpoint.get("bytes") != 0x100000 or
            [child for child in resolved.get("devices", [])
             if child.get("driver") == "memory_window-endpoint" and
             child.get("mode") == "startup-memory"] != [endpoint]):
        raise ValueError(error)
    parents = [item for item in resolved.get("controllers", [])
               if item.get("id") == endpoint.get("controller")]
    if (len(parents) != 1 or parents[0].get("kind") != "external_bus" or
            parents[0].get("controller") != "FSMC" or
            parents[0].get("mode") != "async16"):
        raise ValueError(error)
    if (selected != [{"id": "addsram", "origin": 0x68000000,
                      "size": 0x100000, "dma": False, "linker": True}] or
            selected[0].get("dma") is not False or
            selected[0].get("linker") is not True):
        raise ValueError(error)
    flash, layout = resolved.get("flash", {}), resolved.get("layout", {})
    budget = resolved.get("memory_budgets", {})
    choice = budget.get("regions", {}).get("addsram", {})
    limit = choice.get("limit_bytes")
    if (flash.get("origin") != 0x08000000 or flash.get("size") != 0x100000 or
            layout.get("origin") != flash["origin"] or
            layout.get("size") != flash["size"] or
            budget.get("image_prefix_bytes", 0) or
            budget.get("image_suffix_bytes", 0) or
            type(limit) is not int or not 4 <= limit <= 0x100000 or limit % 4 or
            budget.get("main_stack_bytes", 0) < 512):
        raise ValueError(error)


def selections(result):
    selected = {region["id"] for region in result.memory if region["linker"]}
    return [("NEXUS_MEMORY_" + name.upper(), int(name in selected))
            for name in ("addsram", "tcm", "ccm", "sram1", "sram4")]


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
    ram = next(region for region in result.memory if region["linker"])
    flash_origin = result.image_origin
    flash_size = result.image_size
    stack = result.memory_budget.main_stack_bytes
    heap = result.memory_budget.libc_heap_bytes
    extras = [region for region in result.memory if region["id"] != ram["id"]]
    selected = [region for region in extras if region["linker"]]
    extra_phdrs = "\n".join(f"    {region['id']}_{name} PT_LOAD FLAGS(6);"
                            for region in selected for name in ("data", "ram"))
    extra_memory = "\n".join(f"    {region['id'].upper()} (rw) : ORIGIN = "
                             f"0x{region['origin']:08x}, LENGTH = {region['size']}"
                             for region in selected)
    extra_sections = _extra_sections(extras)
    # Optional exact SoC sections remain independent from generic placement.
    from tools.configure.providers import for_selection
    fragments = getattr(for_selection(result),
                        "linker_fragments", None)
    (flash_image_record, flash_ram_phdr, flash_ram_section,
     flash_workspace_bss) = (fragments(result) if fragments else ("", "", "", ""))
    vector_assertions = ""
    if (flash_origin != result.flash["origin"] or
            result.memory_budget.image_prefix_bytes or
            result.memory_budget.image_suffix_bytes):
        profile = for_selection(result)
        alignment = profile.OFFSET_IMAGE_ALIGNMENT
        symbol = profile.OFFSET_IMAGE_VECTOR_SYMBOL
        vector_assertions = (
            '    ASSERT(ADDR(.isr_vector) == ORIGIN(FLASH), '
            '"image vectors must begin at the image origin")\n'
            f'    ASSERT((ADDR(.isr_vector) & {alignment - 1}) == 0, '
            '"image vectors require reviewed alignment")\n'
            f'    ASSERT({symbol} == ADDR(.isr_vector), '
            '"startup vectors must begin the image vector section")\n'
            '    ASSERT(SIZEOF(.isr_vector) > 0 && '
            f'SIZEOF(.isr_vector) <= {alignment}, '
            '"image vector table exceeds the reviewed reservation")\n')
    return f'''/* Resolved executable window inside its physical erase owner. */
ENTRY(Reset_Handler)
PHDRS {{
    flash PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    ram PT_LOAD FLAGS(6);
    stack PT_LOAD FLAGS(6);
{flash_ram_phdr}{extra_phdrs}
}}
MEMORY {{
    FLASH (rx) : ORIGIN = 0x{flash_origin:08x}, LENGTH = {flash_size}
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
__nx_primary_region_start = ORIGIN(RAM);
__nx_primary_region_end = ORIGIN(RAM) + LENGTH(RAM);
__nx_resolved_sha256_0 = 0x{result.configuration_sha256[0:8]};
__nx_resolved_sha256_1 = 0x{result.configuration_sha256[8:16]};
__nx_resolved_sha256_2 = 0x{result.configuration_sha256[16:24]};
__nx_resolved_sha256_3 = 0x{result.configuration_sha256[24:32]};
__nx_resolved_sha256_4 = 0x{result.configuration_sha256[32:40]};
__nx_resolved_sha256_5 = 0x{result.configuration_sha256[40:48]};
__nx_resolved_sha256_6 = 0x{result.configuration_sha256[48:56]};
__nx_resolved_sha256_7 = 0x{result.configuration_sha256[56:64]};
SECTIONS {{
    .isr_vector : {{ KEEP(*(.isr_vector)) KEEP(*(.vectors)) }} > FLASH :flash
{vector_assertions}    .text : {{ *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) }} > FLASH :flash
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


def dma_regions(name, result, include_flash=False, *, engine=None):
    """Render the selected physical memory without reconstructing a decision."""
    return dma_regions_from_facts(name, result.memory,
        flash=result.flash if include_flash else None, engine=engine)


def dma_regions_from_facts(name, memory, *, flash=None, engine=None):
    """Emit exact DMA domains; invalid physical reachability raises ValueError.

    Keep this module usable by independent ELF budget checkers without provider
    configuration imports. The configuration entry point owns error reporting.
    """
    if (flash is not None) and any("dma_engines" in region
                             for region in memory):
        # RAM bus reachability never implies access to the Flash bus. New
        # heterogeneous engines need reviewed Flash facts before enabling it.
        raise ValueError("Engine-specific Flash reachability is not declared")
    regions = []
    for region in memory:
        if "dma_engines" in region:
            if engine is None or engine not in region["dma_engines"]:
                raise ValueError(
                    "DMA memory projection requires an exact declared engine")
            reachable = region["dma_engines"][engine]
        else:
            reachable = region["dma"]
        if reachable:
            regions.append(region)
    if not regions and not (flash is not None):
        return [], 0
    lines = [f"static const nx_dma_memory_region_t s_nx_dma_regions_{name}[] = {{"]
    for region in regions:
        lines.append(f"    {{ (uintptr_t)0x{region['origin']:08x}u, {region['size']}u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }},")
    if (flash is not None):
        lines.append(f"    {{ (uintptr_t)0x{flash['origin']:08x}u, {flash['size']}u, NX_DMA_MEMORY_READ }},")
    lines.append("};")
    return lines, len(regions) + int((flash is not None))
