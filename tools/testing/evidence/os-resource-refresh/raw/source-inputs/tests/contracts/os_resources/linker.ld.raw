/* Synthetic CPU software reference, not a SoC memory or Board contract. */
ENTRY(_start)
MEMORY {
    SOFTWARE_ROM (rx) : ORIGIN = 0x08000000, LENGTH = 4M
    SOFTWARE_RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 1M
}
SECTIONS {
    .text : {
        KEEP(*(.runtime.references))
        *(.text*)
        *(.rodata*)
    } > SOFTWARE_ROM
    .ARM.extab : { *(.ARM.extab*) } > SOFTWARE_ROM
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > SOFTWARE_ROM
    .data : {
        __data_start__ = .;
        *(.data*)
        __data_end__ = .;
    } > SOFTWARE_RAM AT> SOFTWARE_ROM
    .bss (NOLOAD) : {
        __bss_start__ = .;
        KEEP(*(.bss.os.probes))
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    } > SOFTWARE_RAM
    /DISCARD/ : { *(.note.GNU-stack) }
}
