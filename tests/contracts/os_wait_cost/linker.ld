/* Link-only allocation model; no physical platform or startup claim. */
ENTRY(_start)
MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1M
    RAM (rw) : ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS
{
    .text : { *(.text*) } > FLASH
    .rodata : { *(.rodata*) } > FLASH
    .data : { *(.data*) } > RAM AT> FLASH
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
