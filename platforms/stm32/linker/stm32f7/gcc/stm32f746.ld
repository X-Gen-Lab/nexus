/**
 ******************************************************************************
 * \file            stm32f746.ld
 * \brief           Linker script for STM32F746 device with 1MB Flash
 *                  and 320KB RAM for ARM GCC toolchain
 * \author          Nexus Team
 ******************************************************************************
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This linker script defines memory layout and section
 *                  placement for STM32F746 microcontroller. Stack and heap
 *                  sizes can be configured through Kconfig.
 *
 * \note            STM32F7 has multiple RAM regions:
 *                  - DTCM RAM: 64KB (0x20000000)
 *                  - SRAM1: 240KB (0x20010000)
 *                  - SRAM2: 16KB (0x2004C000)
 ******************************************************************************
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);

/* Stack and heap sizes (defined by CMake via --defsym) */
/* Default values if not provided */
PROVIDE(_Min_Stack_Size = 0x1000);  /* 4KB default */
PROVIDE(_Min_Heap_Size = 0x1000);   /* 4KB default */

/* Memory Regions */
MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
    DTCMRAM (rw)    : ORIGIN = 0x20000000, LENGTH = 64K
    SRAM1 (rw)      : ORIGIN = 0x20010000, LENGTH = 240K
    SRAM2 (rw)      : ORIGIN = 0x2004C000, LENGTH = 16K
    ITCMRAM (rwx)   : ORIGIN = 0x00000000, LENGTH = 16K
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
    } >DTCMRAM AT> FLASH

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
    } >DTCMRAM

    /* User heap and stack */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >DTCMRAM

    /* SRAM1 section (optional, for large buffers) */
    .sram1 (NOLOAD) :
    {
        . = ALIGN(4);
        _ssram1 = .;
        *(.sram1)
        *(.sram1*)
        . = ALIGN(4);
        _esram1 = .;
    } >SRAM1

    /* SRAM2 section (optional) */
    .sram2 (NOLOAD) :
    {
        . = ALIGN(4);
        _ssram2 = .;
        *(.sram2)
        *(.sram2*)
        . = ALIGN(4);
        _esram2 = .;
    } >SRAM2

    /* Remove information from the compiler libraries */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
