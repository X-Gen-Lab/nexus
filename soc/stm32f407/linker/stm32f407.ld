/**
 ******************************************************************************
 * \file            stm32f407vg.ld
 * \brief           Linker script for STM32F407VG device with 1024KB Flash
 *                  and 128KB RAM for ARM GCC toolchain
 * \author          Nexus Team
 ******************************************************************************
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This linker script defines memory layout and section
 *                  placement for STM32F407VG microcontroller. Stack and heap
 *                  sizes can be configured through Kconfig.
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
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 768K
    RAM (rw)    : ORIGIN = 0x20000000, LENGTH = 128K
    CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K
}

/* Sectors 10 and 11 are reserved for the two-bank parameter journal.
 * The image cannot link into the storage partition. Physical Flash is 1 MiB. */
__nexus_storage_start = 0x080C0000;
__nexus_storage_end = 0x08100000;

/* Common STM32F407 section placement; memory identity stays above. */
INCLUDE stm32f407_sections.ld
