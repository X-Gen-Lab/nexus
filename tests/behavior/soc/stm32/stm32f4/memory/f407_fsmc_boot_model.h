/**
 * \file            f407_fsmc_boot_model.h
 * \brief           Shared host ports for real reset and runtime FSMC owners
 * \author          Nexus Team
 */
#ifndef NEXUS_F407_FSMC_BOOT_MODEL_H
#define NEXUS_F407_FSMC_BOOT_MODEL_H
#include "f407_fsmc_model.h"
#include "f407_sram_boot_model.h"
/* Model only the selected identity read ports; the actual plan proof and
 * reset profile remain real C code under test. */
#undef NX_SRAM_BOOT_DEVICE_ID
#undef NX_SRAM_BOOT_FLASH_KIB
#define NX_SRAM_BOOT_DEVICE_ID() UINT32_C(0x10010413)
#define NX_SRAM_BOOT_FLASH_KIB() UINT16_C(1024)
#endif
