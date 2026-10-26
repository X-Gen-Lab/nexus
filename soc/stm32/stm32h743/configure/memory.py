"""H743 Flash RAM execution ABI fragments owned by its cold provider."""


def linker_fragments(result):
    if not any(item["kind"] == "flash" for item in result["controllers"]):
        return ("", "", "", "")
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
    return (flash_image_record, flash_ram_phdr, flash_ram_section,
            flash_workspace_bss)
