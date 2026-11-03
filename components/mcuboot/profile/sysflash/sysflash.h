/**
 * \file            sysflash.h
 * \brief           Upstream role IDs for the caller's explicit image pair.
 * \author          Nexus Team
 */
#ifndef NEXUS_MCUBOOT_REFERENCE_SYSFLASH_H
#define NEXUS_MCUBOOT_REFERENCE_SYSFLASH_H
#include "flash_map_backend/flash_map_backend.h"
#define FLASH_AREA_IMAGE_PRIMARY(image)                                        \
    flash_area_id_from_multi_image_slot((image), 0)
#define FLASH_AREA_IMAGE_SECONDARY(image)                                      \
    flash_area_id_from_multi_image_slot((image), 1)
#endif
