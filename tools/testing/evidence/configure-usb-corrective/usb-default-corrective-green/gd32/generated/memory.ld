/* Exact resolved physical layout; no product reservation. */
ENTRY(Reset_Handler)
PHDRS {
    flash PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    ram PT_LOAD FLAGS(6);
    stack PT_LOAD FLAGS(6);

}
MEMORY {
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1048576
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 196608

}
__nx_flash_start = ORIGIN(FLASH);
__nx_flash_end = ORIGIN(FLASH) + LENGTH(FLASH);
_estack = ORIGIN(RAM) + LENGTH(RAM);
_sp = _estack;
_Min_Stack_Size = 2048;
_Min_Heap_Size = 0;
__nx_msp_start = _estack - _Min_Stack_Size;
__nx_msp_end = _estack;
__nx_resolved_sha256_0 = 0x7a8cb925;
__nx_resolved_sha256_1 = 0x985e4ad8;
__nx_resolved_sha256_2 = 0x14571eb8;
__nx_resolved_sha256_3 = 0x8a13ad92;
__nx_resolved_sha256_4 = 0x7cb2e317;
__nx_resolved_sha256_5 = 0xf8df3294;
__nx_resolved_sha256_6 = 0x1eeb4fe4;
__nx_resolved_sha256_7 = 0x489013b9;
SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) KEEP(*(.vectors)) } > FLASH :flash
    .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) } > FLASH :flash
    .ARM.extab : { *(.ARM.extab*) } > FLASH :flash
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH :flash
    .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH :flash
    .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } > FLASH :flash
    .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH :flash
    .nx_addsram_unselected : { *(.nx_addsram_data .nx_addsram_data.*) *(.bss.nx_addsram_noinit .bss.nx_addsram_noinit.*) *(.bss.nx_addsram .bss.nx_addsram.*) } > RAM :ram
    ASSERT(SIZEOF(.nx_addsram_unselected) == 0, "memory region is not selected")
    .nx_tcm_unselected : { *(.nx_tcm_data .nx_tcm_data.*) *(.bss.nx_tcm_noinit .bss.nx_tcm_noinit.*) *(.bss.nx_tcm .bss.nx_tcm.*) } > RAM :ram
    ASSERT(SIZEOF(.nx_tcm_unselected) == 0, "memory region is not selected")
    _sidata = LOADADDR(.data);
    .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT> FLASH :data
    .bss (NOLOAD) : AT(ADDR(.bss)) { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM :ram
    .reserved (NOLOAD) : AT(ADDR(.reserved)) { . = ALIGN(8); __heap_start = .; __nx_heap_start = .; . += _Min_Heap_Size; __heap_end = .; __nx_heap_end = .; } > RAM :ram
    .msp __nx_msp_start (NOLOAD) : AT(__nx_msp_start) { . += _Min_Stack_Size; } > RAM :stack
    PROVIDE(end = _ebss);
    PROVIDE(_end = _ebss);
    ASSERT(_ebss + _Min_Heap_Size <= _estack - _Min_Stack_Size, "RAM stack/heap overlap")
}
