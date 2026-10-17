/* Actual ARM mutation fixture; not a production memory map. */
ENTRY(Reset_Handler)
MEMORY {
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2048K
    RAM (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
}
PHDRS {
    flash PT_LOAD FLAGS(5);
    pod PT_LOAD FLAGS(5);
    ram PT_LOAD FLAGS(6);
}
SECTIONS {
    .text : { *(.text*) *(.rodata*) } >FLASH :flash
    .nx_flash_ram : ALIGN(4) {
        __nx_flash_ram_start = .;
        KEEP(*(.nx_flash_ram .nx_flash_ram.*))
        . = ALIGN(4);
        __nx_flash_ram_end = .;
    } >RAM AT>FLASH :pod
    .outside : { KEEP(*(.outside)) } >RAM AT>FLASH :pod
    .data : { *(.data*) } >RAM AT>FLASH :ram
    .bss (NOLOAD) : ALIGN(1024) {
        KEEP(*(.bss.workspace)) *(.bss*) *(COMMON)
    } >RAM :ram
    __nx_flash_ram_stack_start = g_nx_stm32h743_flash_ram_workspace + 664;
    __nx_flash_ram_stack_end = __nx_flash_ram_stack_start + 1024;
    /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.exidx*) *(.ARM.extab*) }
}
