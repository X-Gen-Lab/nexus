/**
 * \file            nx_sections.ld
 * \brief           Nexus static registry linker section definitions for GCC
 * \author          Nexus Team
 *
 * This file defines the linker sections required for the Nexus static
 * registry pattern. Include this file in your main linker script or
 * copy the section definitions to the appropriate location.
 *
 * Sections defined:
 *   - .nx_init_fn.*  : Initialization function pointers (sorted by level)
 *   - .nx_device     : Static device registry
 *   - .nx_fw_info    : Firmware information metadata
 *
 * Usage:
 *   1. Include this file in your linker script using INCLUDE directive:
 *      INCLUDE "nx_sections.ld"
 *
 *   2. Or copy the section definitions to your linker script's SECTIONS block
 *
 * Requirements: 4.1, 4.3, 4.4, 4.5
 */

/*---------------------------------------------------------------------------*/
/* Nexus Initialization Function Section                                     */
/*---------------------------------------------------------------------------*/
/*
 * The .nx_init_fn section contains function pointers for automatic
 * initialization. Functions are sorted by level (0-7) using section
 * name suffixes (.nx_init_fn.0, .nx_init_fn.1, etc.).
 *
 * Level 0: Boundary marker (start) - internal use only
 * Level 1: Board initialization (clock, power)
 * Level 2: Pre-initialization (memory, debug)
 * Level 3: BSP initialization (peripheral config)
 * Level 4: Driver initialization
 * Level 5: Component initialization (middleware)
 * Level 6: Application initialization
 * Level 7: Boundary marker (end) - internal use only
 *
 * Place this section in FLASH/ROM region.
 */
.nx_init_fn :
{
    . = ALIGN(4);
    PROVIDE(__nx_init_fn_start = .);
    KEEP(*(.nx_init_fn.0))      /* Level 0: Boundary start */
    KEEP(*(.nx_init_fn.1))      /* Level 1: Board */
    KEEP(*(.nx_init_fn.2))      /* Level 2: Prev */
    KEEP(*(.nx_init_fn.3))      /* Level 3: BSP */
    KEEP(*(.nx_init_fn.4))      /* Level 4: Driver */
    KEEP(*(.nx_init_fn.5))      /* Level 5: Component */
    KEEP(*(.nx_init_fn.6))      /* Level 6: App */
    KEEP(*(.nx_init_fn.7))      /* Level 7: Boundary end */
    . = ALIGN(4);
    PROVIDE(__nx_init_fn_end = .);
} >FLASH

/*---------------------------------------------------------------------------*/
/* Nexus Device Registry Section                                             */
/*---------------------------------------------------------------------------*/
/*
 * The .nx_device section contains static device descriptors registered
 * at compile time using NX_DEVICE_REGISTER macro.
 *
 * Each device descriptor is aligned to pointer size for efficient access.
 *
 * Place this section in FLASH/ROM region.
 */
.nx_device :
{
    . = ALIGN(4);
    PROVIDE(__nx_device_start = .);
    KEEP(*(.nx_device))
    KEEP(*(.nx_device.*))
    . = ALIGN(4);
    PROVIDE(__nx_device_end = .);
} >FLASH

/*---------------------------------------------------------------------------*/
/* Nexus Firmware Information Section                                        */
/*---------------------------------------------------------------------------*/
/*
 * The .nx_fw_info section contains firmware metadata that can be
 * extracted by external tools without executing the firmware.
 *
 * Contains: product name, vendor, version, build date/time, key
 *
 * Place this section in FLASH/ROM region at a known offset for
 * easy extraction by programming tools.
 */
.nx_fw_info :
{
    . = ALIGN(4);
    PROVIDE(__nx_fw_info_start = .);
    KEEP(*(.nx_fw_info))
    KEEP(*(.nx_fw_info.*))
    . = ALIGN(4);
    PROVIDE(__nx_fw_info_end = .);
} >FLASH
