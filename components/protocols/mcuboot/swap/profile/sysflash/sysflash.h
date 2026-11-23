/**
 * \file            sysflash.h
 * \brief           Role identities for the independent scratch profile
 * \author          Nexus Team
 */
#ifndef NEXUS_MCUBOOT_SWAP_SYSFLASH_H
#define NEXUS_MCUBOOT_SWAP_SYSFLASH_H
#include "nexus/mcuboot_swap_port.h"
#define FLASH_AREA_IMAGE_PRIMARY(image)                                        \
    flash_area_id_from_multi_image_slot((image), 0)
#define FLASH_AREA_IMAGE_SECONDARY(image)                                      \
    flash_area_id_from_multi_image_slot((image), 1)
#define FLASH_AREA_IMAGE_SCRATCH NX_MCUBOOT_SCRATCH_AREA_ID
#endif
