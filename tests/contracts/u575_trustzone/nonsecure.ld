/* A single unprivileged NS Thread; no NS exception handler or peripheral map. */
ENTRY(NonSecure_Reset_Handler)
MEMORY {
    NS_FLASH (rx) : ORIGIN = 0x08100000, LENGTH = 64K
    NS_RAM (rw) : ORIGIN = 0x20040000, LENGTH = 14K
    NS_STACK (rw) : ORIGIN = 0x20043800, LENGTH = 2K
}
SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) } > NS_FLASH
    .text ALIGN(1024) : { *(.text*) *(.rodata*) } > NS_FLASH
    .ARM.extab : { *(.ARM.extab*) } > NS_FLASH
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > NS_FLASH
    .data : ALIGN(4) {
        __data_start = .; *(.data*) . = ALIGN(4); __data_end = .;
    } > NS_RAM AT> NS_FLASH
    __data_load = LOADADDR(.data);
    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(4); __bss_end = .;
    } > NS_RAM
    .stack (NOLOAD) : { . += LENGTH(NS_STACK); } > NS_STACK
    __stack_bottom = ORIGIN(NS_STACK);
    __stack_top = ORIGIN(NS_STACK) + LENGTH(NS_STACK);
    /DISCARD/ : { *(.note.GNU-stack) }
}
ASSERT(SIZEOF(.isr_vector) == 568, "Exact U575 vector extent")
ASSERT(ADDR(.text) >= ORIGIN(NS_FLASH) + 1024, "Reserved NS vector page")
ASSERT((__data_load & 3) == 0, "DATA source alignment")
