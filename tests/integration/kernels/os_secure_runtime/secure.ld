/* Explicit synthetic software bounds; no silicon/PCB attribution claim. */
ENTRY(_start)
MEMORY {
    S_ROM (rx) : ORIGIN = 0x10000000, LENGTH = 64K
    S_NSC (rx) : ORIGIN = 0x10010000, LENGTH = 4K
    S_RAM (rwx) : ORIGIN = 0x30000000, LENGTH = 32K
}
SECTIONS {
    .text : { *(.text*) *(.rodata*) } > S_ROM
    .gnu.sgstubs ORIGIN(S_NSC) : {
        . = ALIGN(32);
        *(.gnu.sgstubs*)
    } > S_NSC
    .ARM.extab : { *(.ARM.extab*) } > S_ROM
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > S_ROM
    .data : { *(.data*) } > S_RAM AT> S_ROM
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > S_RAM
    /DISCARD/ : { *(.note.GNU-stack) }
}
ASSERT(SIZEOF(.gnu.sgstubs) > 0, "Real Secure veneers are required")
