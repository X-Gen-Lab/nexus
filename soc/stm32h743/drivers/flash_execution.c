/**
 * \file            flash_execution.c
 * \brief           H743 physical running-image admission before Flash pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_flash_execution.h"

#define NX_H743_FLASH_START 0x08000000U
#define NX_H743_FLASH_END   0x08200000U
#define NX_H743_FLASH_BYTES (NX_H743_FLASH_END - NX_H743_FLASH_START)

/** \brief Image provenance is supplied by the linker, not a product range. */
static bool image_valid(const nx_stm32h743_flash_image_t* image) {
    return image != NULL && image->live_start >= NX_H743_FLASH_START &&
           image->live_start < image->live_end &&
           image->live_end <= NX_H743_FLASH_END &&
           image->load_start >= NX_H743_FLASH_START &&
           image->load_start < image->load_end &&
           image->load_end <= NX_H743_FLASH_END &&
           image->load_start <= image->live_start &&
           image->live_end <= image->load_end;
}

nx_result_t
nx_stm32h743_flash_image_check(const nx_stm32h743_flash_image_t* image,
                               uint32_t offset, size_t length) {
    if (!image_valid(image) || length == 0U || offset >= NX_H743_FLASH_BYTES ||
        length > (size_t)(NX_H743_FLASH_BYTES - offset)) {
        return NX_ERROR_INVALID;
    }
    /* Validation bounds both additions and the host size_t conversion. Load
     * bytes remain protected even after their RAM copy has completed. */
    const uint32_t first = NX_H743_FLASH_START + offset;
    const uint32_t last = first + (uint32_t)length;
    return first < image->load_end && image->load_start < last ? NX_ERROR_STATE
                                                               : NX_SUCCESS;
}
