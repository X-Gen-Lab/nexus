"""Resolve optional exact RAM domains and render one allocation-free linker ABI.

Physical memory facts remain authoritative in the SoC package. Selection adds
only placement and per-domain budgets; it never changes bus-master reachability.
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
                   else result.layout["size"])
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
        budget.get("flash_load_bytes", resolved["layout"]["size"]), limits,
        budget["main_stack_bytes"], budget.get("libc_heap_bytes", 0))


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


def _mpu_linker(result):
    """Emit the selected five-domain v8 layout with real reset copy bounds.

    The exact policy and reservation sizes are validated by configuration.
    User initializers have a distinct cold-start copy/zero ABI; ordinary data,
    kernel metadata and the boot stack remain in the primary privileged RAM.
    """
    budgets = result.memory_budget
    reservations = budgets.mpu
    flash = result.layout
    ram = next(region for region in result.memory if region["linker"])
    flash_origin = flash["origin"]
    svc_origin = flash_origin + reservations.privileged_flash_bytes
    user_flash_origin = svc_origin + reservations.syscall_flash_bytes
    ram_origin = ram["origin"]
    user_ram_origin = ram_origin + reservations.privileged_ram_bytes
    domains = (
        ("PRIV_FLASH", "rx", flash_origin,
         reservations.privileged_flash_bytes),
        ("SVC_FLASH", "rx", svc_origin, reservations.syscall_flash_bytes),
        ("USER_FLASH", "rx", user_flash_origin,
         reservations.user_flash_bytes),
        ("PRIV_RAM", "rw", ram_origin, reservations.privileged_ram_bytes),
        ("USER_RAM", "rw", user_ram_origin, reservations.user_ram_bytes),
    )
    domain_lines = "\n".join(
        f"    {name} ({access}) : ORIGIN = 0x{origin:08x}, LENGTH = {size}"
        for name, access, origin, size in domains)
    nexus_domains = (
        ("privileged_flash", "PRIV_FLASH"),
        ("syscall_flash", "SVC_FLASH"),
        ("user_flash", "USER_FLASH"),
        ("privileged_ram", "PRIV_RAM"),
        ("user_ram", "USER_RAM"),
    )
    bounds = "\n".join(
        f"__nexus_{name}_start__ = ORIGIN({domain});\n"
        f"__nexus_{name}_end__ = ORIGIN({domain}) + LENGTH({domain});"
        for name, domain in nexus_domains)
    aliases = "\n".join(
        f"__{port}_start__ = __nexus_{name}_start__;\n"
        f"__{port}_end__ = __nexus_{name}_end__ - 1;"
        for port, name in (("privileged_functions", "privileged_flash"),
                           ("privileged_sram", "privileged_ram"),
                           ("unprivileged_flash", "user_flash"),
                           ("syscalls_flash", "syscall_flash")))
    identities = "\n".join(
        f"__nx_resolved_sha256_{index} = 0x"
        f"{result.configuration_sha256[index * 8:(index + 1) * 8]};"
        for index in range(8))
    # Optional physical domains are outside this exact policy. Preserve their
    # existing fail-closed input-section checks in the privileged primary RAM.
    extras = [region for region in result.memory if region["id"] != ram["id"]]
    extra_sections = _extra_sections(extras).replace(
        "> RAM :ram", "> PRIV_RAM :ram")
    return f'''/* Explicit selected MPU reservations; physical facts unchanged. */
ENTRY(Reset_Handler)
PHDRS {{
    flash PT_LOAD FLAGS(5);
    syscalls PT_LOAD FLAGS(5);
    user_flash PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    user_data PT_LOAD FLAGS(6);
    ram PT_LOAD FLAGS(6);
    user_ram PT_LOAD FLAGS(6);
    stack PT_LOAD FLAGS(6);
}}
MEMORY {{
{domain_lines}
}}
{bounds}
{aliases}
__nx_flash_start = 0x{flash_origin:08x};
__nx_flash_end = 0x{flash_origin + flash["size"]:08x};
__nx_flash_live_start = __nx_flash_start;
__nx_flash_image_load_start = __nx_flash_start;
_estack = ORIGIN(PRIV_RAM) + LENGTH(PRIV_RAM);
_sp = _estack;
_Min_Stack_Size = {budgets.main_stack_bytes};
_Min_Heap_Size = {budgets.libc_heap_bytes};
__nx_msp_start = _estack - _Min_Stack_Size;
__nx_msp_end = _estack;
{identities}
SECTIONS {{
    .syscalls : {{ *(freertos_system_calls freertos_system_calls.*) }} > SVC_FLASH :syscalls
    .user_text : {{ *(.nexus_user_text .nexus_user_text.*) *(.nexus_user_rodata .nexus_user_rodata.*) }} > USER_FLASH :user_flash
    .isr_vector : {{ KEEP(*(.isr_vector)) KEEP(*(.vectors)) }} > PRIV_FLASH :flash
    .text : {{ *(privileged_functions privileged_functions.*) *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) }} > PRIV_FLASH :flash
    .ARM.extab : {{ *(.ARM.extab*) }} > PRIV_FLASH :flash
    .ARM.exidx : {{ __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; }} > PRIV_FLASH :flash
    .preinit_array : {{ PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); }} > PRIV_FLASH :flash
    .init_array : {{ PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); }} > PRIV_FLASH :flash
    .fini_array : {{ PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); }} > PRIV_FLASH :flash
    __nx_mpu_priv_code_end = .;
    __nx_flash_live_end = MAX(__nx_mpu_priv_code_end, MAX(ADDR(.syscalls) + SIZEOF(.syscalls), ADDR(.user_text) + SIZEOF(.user_text)));
{extra_sections}
    .nx_mpu_user_data : ALIGN(32) {{ . = ALIGN(32); __nx_mpu_user_data_start = .; *(.nexus_user_data .nexus_user_data.*) . = ALIGN(32); __nx_mpu_user_data_end = .; }} > USER_RAM AT> PRIV_FLASH :user_data
    __nx_mpu_user_data_load = LOADADDR(.nx_mpu_user_data);
    .nx_mpu_user_bss (NOLOAD) : AT(ADDR(.nx_mpu_user_bss)) {{ . = ALIGN(32); __nx_mpu_user_bss_start = .; *(.bss.nexus_user_data .bss.nexus_user_data.*) *(.nexus_user_bss .nexus_user_bss.*) . = ALIGN(32); __nx_mpu_user_bss_end = .; }} > USER_RAM :user_ram
    _sidata = LOADADDR(.data);
    .data : ALIGN(4) {{ . = ALIGN(4); _sdata = .; *(privileged_data privileged_data.*) *(.data*) . = ALIGN(4); _edata = .; }} > PRIV_RAM AT> PRIV_FLASH :data
    __nx_flash_image_load_end = MAX(__nx_flash_live_end, MAX(LOADADDR(.data) + SIZEOF(.data), LOADADDR(.nx_mpu_user_data) + SIZEOF(.nx_mpu_user_data)));
    .bss (NOLOAD) : AT(ADDR(.bss)) {{ . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; }} > PRIV_RAM :ram
    .reserved (NOLOAD) : AT(ADDR(.reserved)) {{ . = ALIGN(8); __heap_start = .; __nx_heap_start = .; . += _Min_Heap_Size; __heap_end = .; __nx_heap_end = .; }} > PRIV_RAM :ram
    .msp __nx_msp_start (NOLOAD) : AT(__nx_msp_start) {{ . += _Min_Stack_Size; }} > PRIV_RAM :stack
    PROVIDE(end = _ebss);
    PROVIDE(_end = _ebss);
    ASSERT(_ebss + _Min_Heap_Size <= _estack - _Min_Stack_Size, "RAM stack/heap overlap")
    ASSERT((__nx_mpu_user_data_start & 31) == 0 && (__nx_mpu_user_data_end & 31) == 0, "user data requires exact MPU granules")
    ASSERT((__nx_mpu_user_bss_start & 31) == 0 && (__nx_mpu_user_bss_end & 31) == 0, "user BSS requires exact MPU granules")
}}
'''


def linker(result):
    if result.memory_budget.mpu is not None:
        return _mpu_linker(result)
    ram = next(region for region in result.memory if region["linker"])
    flash = result.layout
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
    from tools.configure.providers import maintained
    fragments = getattr(maintained(result.soc_family),
                        "linker_fragments", None)
    (flash_image_record, flash_ram_phdr, flash_ram_section,
     flash_workspace_bss) = (fragments(result) if fragments else ("", "", "", ""))
    vector_assertions = ""
    if flash["origin"] != result.flash["origin"]:
        profile = maintained(result.soc_family)
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
