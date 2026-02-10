/**
 ******************************************************************************
 * \file            stm32f4xx_template.ld
 * \brief           Generic linker script template for STM32F4 series
 * \author          Nexus Team
 ******************************************************************************
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This is a generic linker script template for STM32F4 series.
 *                  Memory sizes should be configured based on specific chip variant.
 *                  Stack and heap sizes can be configured through Kconfig.
 *
 * \note            To create a linker script for a specific chip:
 *                  1. Copy this template
 *                  2. Update FLASH and RAM sizes in MEMORY section
 *                  3. Update CCMRAM size if available
 *                  4. Rename file to match chip variant
 ******************************************************************************
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Stack and heap sizes (defined by CMake via --defsym) */
/* Default values if not provided */
PROVIDE(_Min_Stack_Size = 0x400);  /* 1KB default */
PROVIDE(_Min_Heap_Size = 0x200);   /* 512B default */

/* Memory Regions */
/* NOTE: Update these values based on specific chip variant */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K  /* Update based on chip */
    RAM (rw)    : ORIGIN = 0x20000000, LENGTH = 128K   /* Update based on chip */
    CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K    /* Not all F4 have CCM */
}

/* Sections */
SECTIONS
{
    /* Vector table must be at the beginning of FLASH */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* Program code and constants */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP (*(.init))
        KEEP (*(.fini))

        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* Constant data */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    .ARM.extab :
    {
        . = ALIGN(4);
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } >FLASH

    .ARM :
    {
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
        . = ALIGN(4);
    } >FLASH

    .preinit_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .init_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .fini_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
        . = ALIGN(4);
    } >FLASH

    /* Nexus HAL device registration section */
    .nx_device :
    {
        . = ALIGN(4);
        __nx_device_start = .;
        KEEP(*(SORT(.nx_device*)))
        __nx_device_end = .;
        . = ALIGN(4);
    } >FLASH

    /* Used by startup to initialize data */
    _sidata = LOADADDR(.data);

    /* Initialized data section */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    /* CCM RAM section (optional, not all F4 variants have CCM) */
    .ccmram :
    {
        . = ALIGN(4);
        _sccmram = .;
        *(.ccmram)
        *(.ccmram*)
        . = ALIGN(4);
        _eccmram = .;
    } >CCMRAM AT> FLASH

    /* Uninitialized data section */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    /* User heap and stack */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /* Remove information from the compiler libraries */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
