/* Reference application partition; not a platform-selected product policy. */
ENTRY(Secure_Reset_Handler)
MEMORY {
    S_FLASH (rx) : ORIGIN = 0x0c000000, LENGTH = 1016K
    NSC (rx) : ORIGIN = 0x0c0fe000, LENGTH = 8K
    S_RAM (rw) : ORIGIN = 0x30000000, LENGTH = 188K
    S_STACK (rw) : ORIGIN = 0x3002f000, LENGTH = 4K
}
SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) } > S_FLASH
    .text : { *(.text*) *(.rodata*) } > S_FLASH
    .gnu.sgstubs ORIGIN(NSC) : { . = ALIGN(32); *(.gnu.sgstubs*) } > NSC
    .ARM.extab : { *(.ARM.extab*) } > S_FLASH
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > S_FLASH
    .data : ALIGN(4) {
        __data_start = .; *(.data*) . = ALIGN(4); __data_end = .;
    } > S_RAM AT> S_FLASH
    __data_load = LOADADDR(.data);
    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(4); __bss_end = .;
    } > S_RAM
    .stack (NOLOAD) : { . += LENGTH(S_STACK); } > S_STACK
    __stack_bottom = ORIGIN(S_STACK);
    __stack_top = ORIGIN(S_STACK) + LENGTH(S_STACK);
    /DISCARD/ : { *(.note.GNU-stack) }
}
ASSERT(SIZEOF(.isr_vector) == 568, "Exact U575 vector extent")
ASSERT(SIZEOF(.gnu.sgstubs) > 0, "Actual CMSE gateway required")
ASSERT(SIZEOF(.gnu.sgstubs) <= LENGTH(NSC), "NSC partition overflow")
ASSERT((__data_load & 3) == 0, "DATA source alignment")
