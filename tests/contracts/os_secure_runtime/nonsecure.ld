/* Paired synthetic NS TCB identity, unrelated to a real SoC memory map. */
ENTRY(_start)
MEMORY {
    NS_ROM (rx) : ORIGIN = 0x00020000, LENGTH = 128K
    NS_RAM (rwx) : ORIGIN = 0x20010000, LENGTH = 64K
}
SECTIONS {
    .text : {
        KEEP(*(.fixture.handlers))
        *(.text*) *(.rodata*)
    } > NS_ROM
    .ARM.extab : { *(.ARM.extab*) } > NS_ROM
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > NS_ROM
    .fixture.tcb (NOLOAD) : { KEEP(*(.fixture.tcb)) } > NS_RAM
    .fixture.idle_tcb 0x20010400 (NOLOAD) : {
        KEEP(*(.fixture.idle_tcb))
    } > NS_RAM
    .data : { *(.data*) } > NS_RAM AT> NS_ROM
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > NS_RAM
    /DISCARD/ : { *(.note.GNU-stack) }
}
ASSERT(ADDR(.fixture.tcb) == 0x20010000, "Secure task identity must agree")
ASSERT(SIZEOF(.fixture.tcb) <= 1024, "Task identity must not overlap Idle")
ASSERT(ADDR(.fixture.idle_tcb) == 0x20010400, "Secure Idle identity must agree")
