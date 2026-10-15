/* Exact resolved physical layout; no product reservation. */
ENTRY(Reset_Handler)
PHDRS {
    flash PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    ram PT_LOAD FLAGS(6);
    stack PT_LOAD FLAGS(6);

}
MEMORY {
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 524288
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 131072

}
__nx_flash_start = ORIGIN(FLASH);
__nx_flash_end = ORIGIN(FLASH) + LENGTH(FLASH);
_estack = ORIGIN(RAM) + LENGTH(RAM);
_sp = _estack;
_Min_Stack_Size = 2048;
_Min_Heap_Size = 0;
__nx_msp_start = _estack - _Min_Stack_Size;
__nx_msp_end = _estack;
__nx_resolved_sha256_0 = 0x1723da3b;
__nx_resolved_sha256_1 = 0x19d57b67;
__nx_resolved_sha256_2 = 0xf4aea1be;
__nx_resolved_sha256_3 = 0xa06db69d;
__nx_resolved_sha256_4 = 0x255235cc;
__nx_resolved_sha256_5 = 0x116aecb9;
__nx_resolved_sha256_6 = 0xa996f6a4;
__nx_resolved_sha256_7 = 0xab6ac557;
SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) KEEP(*(.vectors)) } > FLASH :flash
    .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) } > FLASH :flash
    .ARM.extab : { *(.ARM.extab*) } > FLASH :flash
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH :flash
    .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH :flash
    .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } > FLASH :flash
    .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH :flash
    .nx_ccm_unselected : { *(.nx_ccm_data .nx_ccm_data.*) *(.bss.nx_ccm_noinit .bss.nx_ccm_noinit.*) *(.bss.nx_ccm .bss.nx_ccm.*) } > RAM :ram
    ASSERT(SIZEOF(.nx_ccm_unselected) == 0, "memory region is not selected")
    _sidata = LOADADDR(.data);
    .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT> FLASH :data
    .bss (NOLOAD) : AT(ADDR(.bss)) { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM :ram
    .reserved (NOLOAD) : AT(ADDR(.reserved)) { . = ALIGN(8); __heap_start = .; __nx_heap_start = .; . += _Min_Heap_Size; __heap_end = .; __nx_heap_end = .; } > RAM :ram
    .msp __nx_msp_start (NOLOAD) : AT(__nx_msp_start) { . += _Min_Stack_Size; } > RAM :stack
    PROVIDE(end = _ebss);
    PROVIDE(_end = _ebss);
    ASSERT(_ebss + _Min_Heap_Size <= _estack - _Min_Stack_Size, "RAM stack/heap overlap")
}
