"""Resolve optional exact RAM domains and render one allocation-free linker ABI.

Internal physical memory facts remain authoritative in the SoC package. A
reviewed selected external bank retains its own immutable backing reference.
Placement and per-domain budgets never add bus-master reachability.
"""
from collections.abc import Mapping

_FLASH_UINT32_MAX = (1 << 32) - 1


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


def _flash_uint32(value, minimum=0, *, context):
    if type(value) is not int or not minimum <= value <= _FLASH_UINT32_MAX:
        raise ValueError(
            f"{context}: integer required in [{minimum}, {_FLASH_UINT32_MAX}]")
    return value


def flash_erase_spans(flash, *, program_unit=1):
    """Validate one complete physical span sequence without expanding sectors."""
    size = _flash_uint32(flash.get("size"), 1, context="Flash size")
    _flash_uint32(program_unit, 1, context="Flash program unit")
    spans = flash.get("erase_spans")
    if not isinstance(spans, (list, tuple)):
        raise ValueError("Flash erase geometry: expected array")
    if not spans or "erase_blocks" in flash:
        raise ValueError("Flash erase geometry requires only complete erase spans")
    cursor = count = 0
    for span in spans:
        if (not isinstance(span, Mapping) or
                set(span) != {"offset", "erase_bytes", "count"}):
            raise ValueError(
                "Flash erase geometry requires exact offset/erase_bytes/count")
        offset = _flash_uint32(span["offset"], context="erase span offset")
        erase_bytes = _flash_uint32(span["erase_bytes"], 1,
                                   context="erase span bytes")
        sectors = _flash_uint32(span["count"], 1, context="erase span count")
        if offset != cursor or erase_bytes % program_unit:
            raise ValueError(
                "Flash erase geometry is unordered or not a whole program unit")
        cursor += erase_bytes * sectors
        count += sectors
        if cursor > size or count > _FLASH_UINT32_MAX:
            raise ValueError(
                "Flash erase geometry overflows physical Flash or sector count")
    if cursor != size:
        raise ValueError("Flash erase geometry does not cover exact physical Flash")
    return spans


def flash_erase_boundary(flash, address):
    """Check an absolute physical boundary in O(spans), including device end."""
    spans = flash_erase_spans(flash)
    offset = address - flash["origin"]
    if offset == flash["size"]:
        return True
    return any(span["offset"] <= offset < span["offset"] +
               span["erase_bytes"] * span["count"] and
               (offset - span["offset"]) % span["erase_bytes"] == 0
               for span in spans)


def flash_geometry(result, *, symbol, span_symbol, program_unit,
                   program_flags="0u", static=True):
    """Emit one readonly physical geometry from the selected immutable facts."""
    flash = result.physical_part.flash
    spans = flash_erase_spans(flash, program_unit=program_unit)
    lines = [f"static const nx_flash_erase_span_t {span_symbol}[] = {{"]
    for span in spans:
        lines.append(f"    {{ {span['offset']}u, {span['erase_bytes']}u, "
                     f"{span['count']}u }},")
    storage = "static " if static else ""
    return lines + ["};", f"{storage}const nx_flash_geometry_t {symbol} = {{",
                   f"    .base_address = 0x{flash['origin']:08x}u,",
                   f"    .size = {flash['size']}u,",
                   f"    .program_unit = {program_unit}u,",
                   f"    .erase_spans = {span_symbol},",
                   f"    .erase_span_count = {len(spans)}u,",
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
    """Audit selected references and quantities without admitting hardware.

    The producer owns part, bus, startup and electrical permission. This reader
    checks the same projection's endpoint/owner/memory/budget relationship; it
    cannot turn an internally consistent record into a new support claim.
    """
    error = "Invalid external memory backing projection"

    def named(value):
        return type(value) is str and bool(value)

    def interval(record, origin="origin", size="size"):
        if not isinstance(record, Mapping):
            raise ValueError(error)
        start = _flash_uint32(record.get(origin), context="External memory origin")
        count = _flash_uint32(record.get(size), 1, context="External memory bytes")
        if start + count > 1 << 32:
            raise ValueError(error + ": address interval overflows")
        return start, count

    external = resolved.get("external_memory", [])
    devices = resolved.get("devices", [])
    if (not isinstance(external, list) or not isinstance(devices, list) or
            any(not isinstance(item, Mapping) for item in devices)):
        raise ValueError(error)
    startup = [item for item in devices if
               item.get("driver") == "memory_window-endpoint" and
               item.get("mode") == "startup-memory"]
    if not external:
        if startup:
            raise ValueError(error + ": selected startup endpoint lacks backing")
        return
    for name in ("soc_family", "part", "backend", "optimization"):
        if name in resolved and not named(resolved[name]):
            raise ValueError(error + ": malformed selection identity")
    memory = resolved.get("memory")
    controllers = resolved.get("controllers")
    budget = resolved.get("memory_budgets")
    if (not isinstance(memory, list) or not memory or
            any(not isinstance(item, Mapping) for item in memory) or
            not isinstance(controllers, list) or
            any(not isinstance(item, Mapping) for item in controllers) or
            not isinstance(budget, Mapping) or
            not isinstance(budget.get("regions"), Mapping)):
        raise ValueError(error)
    domains = {}
    for region in memory:
        identity = region.get("id")
        if not named(identity) or identity in domains:
            raise ValueError(error + ": memory identities must be distinct")
        domains[identity] = region
    selected_regions, selected_endpoints = set(), set()
    for reference in external:
        if (not isinstance(reference, Mapping) or
                set(reference) != {"id", "endpoint"}):
            raise ValueError(error)
        identity, endpoint = reference["id"], reference["endpoint"]
        if (not named(identity) or identity in selected_regions or
                not isinstance(endpoint, Mapping) or
                endpoint.get("driver") != "memory_window-endpoint" or
                endpoint.get("mode") != "startup-memory" or
                not named(endpoint.get("id")) or
                endpoint["id"] in selected_endpoints or
                not named(endpoint.get("controller"))):
            raise ValueError(error + ": invalid or duplicate backing reference")
        matches = [item for item in devices if item.get("id") == endpoint["id"]]
        parents = [item for item in controllers
                   if item.get("id") == endpoint["controller"]]
        if (matches != [endpoint] or len(parents) != 1 or
                parents[0].get("kind") != "external_bus" or
                not named(parents[0].get("controller")) or
                not named(parents[0].get("mode"))):
            raise ValueError(error + ": endpoint or controller owner is not unique")
        origin, size = interval(endpoint, "address", "bytes")
        region = domains.get(identity)
        if (origin % 4 or size % 4 or region != {
                "id": identity, "origin": origin, "size": size,
                "dma": False, "linker": True} or
                region.get("dma") is not False or region.get("linker") is not True):
            raise ValueError(error + ": selected memory differs from its backing")
        selected_regions.add(identity)
        selected_endpoints.add(endpoint["id"])
    if (len(startup) != len(selected_endpoints) or
            {item["id"] for item in startup} != selected_endpoints):
        raise ValueError(error + ": startup endpoints and backing must be one-to-one")
    for identity, choice in budget["regions"].items():
        if (identity not in domains or not isinstance(choice, Mapping) or
                set(choice) != {"limit_bytes"} or
                domains[identity].get("linker") is not True):
            raise ValueError(error + ": budget lacks a selected memory domain")
        limit = _flash_uint32(choice["limit_bytes"], 4, context="External memory budget")
        if limit % 4 or limit > domains[identity]["size"]:
            raise ValueError(error + ": memory budget exceeds its selected domain")
    if not selected_regions.issubset(budget["regions"]):
        raise ValueError(error + ": external placement requires an explicit budget")
    primary = next((region for region in memory if region.get("linker") is True), None)
    if primary is None or primary["id"] in selected_regions:
        raise ValueError(error + ": primary stack/heap domain must remain independent")
    _, primary_size = interval(primary)
    stack = _flash_uint32(budget.get("main_stack_bytes"), 4, context="External memory MSP budget")
    heap = _flash_uint32(budget.get("libc_heap_bytes", 0), context="External memory heap budget")
    static = _flash_uint32(budget.get("static_ram_bytes", primary_size), 4,
                           context="External memory primary budget")
    if stack % 4 or heap % 4 or static % 4 or stack + heap > static or static > primary_size:
        raise ValueError(error + ": stack/heap exceed the primary memory budget")
    flash_origin, flash_size = interval(resolved.get("flash"))
    image_origin, image_size = interval(resolved.get("layout"))
    if (image_origin < flash_origin or
            image_origin + image_size > flash_origin + flash_size):
        raise ValueError(error + ": image layout leaves physical Flash")
    prefix = _flash_uint32(budget.get("image_prefix_bytes", 0), context="External memory image prefix")
    suffix = _flash_uint32(budget.get("image_suffix_bytes", 0), context="External memory image suffix")
    if prefix % 4 or suffix % 4 or prefix + suffix >= image_size:
        raise ValueError(error + ": image reservations exceed the selected layout")
    load = _flash_uint32(budget.get("flash_load_bytes", image_size - prefix - suffix),
                         1, context="External memory Flash load budget")
    if load > image_size - prefix - suffix:
        raise ValueError(error + ": Flash load budget exceeds the selected image")


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


def _dma_region_facts(memory, *, flash=None, engine=None):
    """Select reviewed DMA intervals once for legacy and typed projections."""
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
            regions.append((region, True))
    if flash is not None:
        regions.append((flash, False))
    return tuple(regions)


def dma_region_declarations(name, result, include_flash=False, *, engine=None):
    """Describe exact static memory-domain records without C expressions."""
    from tools.configure.static_ir import Array, Declaration, Record
    regions = dma_region_bindings(result, include_flash=include_flash, engine=engine)
    if not regions:
        return (), 0
    records = []
    for region in regions:
        records.append(Record((("begin", region.begin), ("size", region.size),
                               ("permissions", region.permissions))))
    declaration = Declaration("nx_dma_memory_region_t", "s_nx_dma_regions_" + name,
                              Array(tuple(records)), count=len(records), const=True)
    return (declaration,), len(records)


def dma_region_bindings(result, include_flash=False, *, engine=None):
    """Freeze the same reviewed regions for static recipes and declarations."""
    from tools.configure.calls import Symbol
    from tools.configure.ir import DmaMemoryBindingIR
    from tools.configure.static_ir import Flags
    regions = _dma_region_facts(result.memory,
        flash=result.flash if include_flash else None, engine=engine)
    return tuple(DmaMemoryBindingIR(region["origin"], region["size"],
        Flags((Symbol("NX_DMA_MEMORY_READ"), Symbol("NX_DMA_MEMORY_WRITE")))
        if writable else Symbol("NX_DMA_MEMORY_READ"))
        for region, writable in regions)


def dma_regions_from_facts(name, memory, *, flash=None, engine=None):
    """Render the same reviewed intervals for remaining legacy constructors."""
    regions = _dma_region_facts(memory, flash=flash, engine=engine)
    if not regions:
        return [], 0
    lines = [f"static const nx_dma_memory_region_t s_nx_dma_regions_{name}[] = {{"]
    for region, writable in regions:
        permissions = "NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE" if writable else "NX_DMA_MEMORY_READ"
        lines.append(f"    {{ (uintptr_t)0x{region['origin']:08x}u, {region['size']}u, {permissions} }},")
    lines.append("};")
    return lines, len(regions)
