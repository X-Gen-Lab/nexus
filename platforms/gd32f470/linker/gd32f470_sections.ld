SECTIONS {
    .isr_vector (READONLY) : {
        . = ALIGN(512); KEEP(*(.vectors)) KEEP(*(.isr_vector))
    } > FLASH
    .text : {
        . = ALIGN(4); *(.text*) *(.glue_7*) *(.eh_frame)
        KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .;
    } > FLASH
    .rodata : { . = ALIGN(4); *(.rodata*) . = ALIGN(4); } > FLASH
    .ARM.extab (READONLY) : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM.exidx (READONLY) : {
        __exidx_start = .; *(.ARM.exidx*) __exidx_end = .;
    } > FLASH
    .preinit_array (READONLY) : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH
    .init_array (READONLY) : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH
    .fini_array (READONLY) : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH
    .nx_init_fn (READONLY) : {
        . = ALIGN(4); __nx_init_fn_start = .;
        KEEP(*(SORT_BY_NAME(.nx_init_fn.*))) __nx_init_fn_end = .;
    } > FLASH
    .nx_abi (READONLY) : {
        . = ALIGN(4); KEEP(*(.nx_abi))
    } > FLASH
    .nx_device (READONLY) : {
        . = ALIGN(4); __nx_device_start = .;
        KEEP(*(SORT(.nx_device*))) __nx_device_end = .;
    } > FLASH
    _sidata = LOADADDR(.data);
    .data : {
        . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .;
    } > RAM AT > FLASH
    .bss (NOLOAD) : {
        . = ALIGN(4); _sbss = .; __bss_start__ = .;
        *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; __bss_end__ = .;
    } > RAM
    ._user_heap_stack (NOLOAD) : {
        . = ALIGN(8); PROVIDE(end = .); PROVIDE(_end = .);
        . += _Min_Heap_Size; . += _Min_Stack_Size; . = ALIGN(8);
    } > RAM
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
__nexus_firmware_flash_end = LOADADDR(.data) + SIZEOF(.data);
ASSERT(__nexus_firmware_flash_end <= __nexus_image_end,
       "GD32F470ZG firmware exceeds selected image layout")
ASSERT(_ebss + _Min_Heap_Size + _Min_Stack_Size <= _estack,
       "GD32F470ZG static data, heap and stack exceed 192 KiB main SRAM")
ASSERT(ADDR(.isr_vector) == ORIGIN(FLASH) && SIZEOF(.isr_vector) == 0x1AC,
       "GD32F470 official startup vector is missing or misplaced")
