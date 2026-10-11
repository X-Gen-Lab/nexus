/**
 * \file            flash_region.c
 *
 * \brief           Checked region-relative access to complete physical Flash.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/flash.h"

/** \brief Validate every region-relative access before translating its address.
 */
static nx_result_t region_range(const nx_flash_region_t* region,
                                uint32_t offset, size_t length, bool write) {
    nx_result_t result = nx_flash_region_validate(region);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (write && !region->writable) {
        return NX_ERROR_PERMISSION;
    }
    if (offset > region->size || length > region->size - offset) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

/** \brief Reject overflow, empty regions and bounds outside physical geometry.
 */
nx_result_t nx_flash_region_validate(const nx_flash_region_t* region) {
    if (region == NULL || region->port == NULL || region->size == 0) {
        return NX_ERROR_INVALID;
    }
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(region->port);
    if (geometry == NULL || region->offset > geometry->size ||
        region->size > geometry->size - region->offset) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

/** \brief Translate only a successfully validated region-relative read. */
nx_result_t nx_flash_region_read(const nx_flash_region_t* region,
                                 uint32_t offset, void* data, size_t length) {
    nx_result_t result = region_range(region, offset, length, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    return nx_flash_port_read(region->port, region->offset + offset, data,
                              length);
}

/** \brief Enforce writable region bounds before the physical program contract.
 */
nx_result_t nx_flash_region_program(const nx_flash_region_t* region,
                                    uint32_t offset, const void* data,
                                    size_t length, nx_time_us_t deadline) {
    nx_result_t result = region_range(region, offset, length, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    return nx_flash_port_program(region->port, region->offset + offset, data,
                                 length, deadline);
}

/** \brief Preserve physical erase alignment while enforcing selected bounds. */
nx_result_t nx_flash_region_erase(const nx_flash_region_t* region,
                                  uint32_t offset, size_t length,
                                  nx_time_us_t deadline) {
    nx_result_t result = region_range(region, offset, length, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    return nx_flash_port_erase(region->port, region->offset + offset, length,
                               deadline);
}
