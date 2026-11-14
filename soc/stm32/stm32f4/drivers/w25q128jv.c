/**
 * \file            w25q128jv.c
 * \brief           Exact NOR identity, adoption and physical pulse boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "w25q128jv.h"

static const nx_flash_erase_span_t erase_span = {0U, NX_W25Q128JV_ERASE_BYTES,
                                                 NX_W25Q128JV_BYTES /
                                                     NX_W25Q128JV_ERASE_BYTES};

const nx_flash_geometry_t nx_w25q128jv_geometry = {
    0U, NX_W25Q128JV_BYTES, 1U, &erase_span, 1U, 0U};

bool nx_w25q128jv_id_matches(const uint8_t id[3]) {
    return id != NULL && id[0] == 0xefU && id[1] == 0x40U && id[2] == 0x18U;
}

nx_result_t nx_w25q128jv_adopt(uint8_t status2, bool quad) {
    return (status2 & 0x80U) != 0U || (quad && (status2 & 2U) == 0U)
               ? NX_ERROR_UNSUPPORTED
               : NX_SUCCESS;
}

size_t nx_w25q128jv_chunk(bool erase, uint32_t offset, size_t remaining) {
    size_t bytes =
        erase ? NX_W25Q128JV_ERASE_BYTES
              : NX_W25Q128JV_PAGE_BYTES - offset % NX_W25Q128JV_PAGE_BYTES;
    return bytes < remaining ? bytes : remaining;
}
