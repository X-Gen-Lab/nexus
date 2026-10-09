/**
 ******************************************************************************
 * \file            stm32f407ve.ld
 * \brief           Linker script for STM32F407VE device with 512KB Flash
 *                  and 128KB RAM for ARM GCC toolchain
 * \author          Nexus Team
 ******************************************************************************
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This linker script defines memory layout and section
 *                  placement for STM32F407VE microcontroller. Stack and heap
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
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
    RAM (rw)    : ORIGIN = 0x20000000, LENGTH = 128K
    CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K
}

/* Sectors 6 and 7 are reserved for the two-bank parameter journal.
 * The image cannot link into the storage partition. Physical Flash is 512 KiB. */
__nexus_storage_start = 0x08040000;
__nexus_storage_end = 0x08080000;

/* Common STM32F407 section placement; memory identity stays above. */
INCLUDE stm32f407_sections.ld
