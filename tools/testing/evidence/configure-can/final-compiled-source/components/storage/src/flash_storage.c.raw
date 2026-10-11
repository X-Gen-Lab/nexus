/**
 * \file            flash_storage.c
 * \brief           Narrow physical region adapter with direct static ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/flash_storage.h"

/** \brief Preserve coarse recovery decisions without a global last error. */
static nx_storage_status_t storage_result(nx_result_t result) {
    if (result == NX_SUCCESS) {
        return NX_STORAGE_OK;
    }
    if (result == NX_ERROR_INVALID || result == NX_ERROR_PERMISSION) {
        return NX_STORAGE_INVALID;
    }
    return result == NX_ERROR_UNSUPPORTED ? NX_STORAGE_UNSUPPORTED
                                          : NX_STORAGE_IO;
}

/** \brief Read only within the caller's physical region. */
static nx_storage_status_t read_bytes(void* context, size_t offset, void* data,
                                      size_t length) {
    nx_flash_storage_adapter_t* adapter = context;
    return offset > UINT32_MAX
               ? NX_STORAGE_INVALID
               : storage_result(nx_flash_region_read(
                     &adapter->region, (uint32_t)offset, data, length));
}

/** \brief Program and settle using the whole transaction's shared deadline. */
static nx_storage_status_t program_bytes(void* context, size_t offset,
                                         const void* data, size_t length) {
    nx_flash_storage_adapter_t* adapter = context;
    return offset > UINT32_MAX ? NX_STORAGE_INVALID
                               : storage_result(nx_flash_region_program(
                                     &adapter->region, (uint32_t)offset, data,
                                     length, adapter->deadline));
}

/** \brief Erase complete physical sectors inside the supplied region. */
static nx_storage_status_t erase_bytes(void* context, size_t offset,
                                       size_t length) {
    nx_flash_storage_adapter_t* adapter = context;
    return offset > UINT32_MAX
               ? NX_STORAGE_INVALID
               : storage_result(nx_flash_region_erase(&adapter->region,
                                                      (uint32_t)offset, length,
                                                      adapter->deadline));
}

/** \brief Physical program/erase already settles the final hardware pulse. */
static nx_storage_status_t sync_bytes(void* context) {
    nx_flash_storage_adapter_t* adapter = context;
    return nx_deadline_expired(adapter->deadline, nx_time_now_us())
               ? NX_STORAGE_IO
               : NX_STORAGE_OK;
}

/** \brief Reject mixed or incomplete erase geometries at binding time. */
nx_result_t nx_flash_storage_bind(nx_flash_storage_adapter_t* adapter,
                                  nx_flash_region_t region,
                                  nx_time_us_t deadline,
                                  nx_storage_port_t* port) {
    if (adapter == NULL || port == NULL || region.size == 0) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_flash_region_validate(&region);
    if (result != NX_SUCCESS || !region.writable) {
        return result != NX_SUCCESS ? result : NX_ERROR_PERMISSION;
    }
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(region.port);
    if (geometry == NULL || geometry->program_unit == 0) {
        return NX_ERROR_INVALID;
    }
    uint32_t erase_size = 0;
    uint32_t cursor = region.offset;
    uint32_t end = region.offset + region.size;
    for (size_t i = 0; i < geometry->sector_count && cursor < end; ++i) {
        nx_flash_sector_t sector = geometry->sectors[i];
        if (sector.offset < cursor) {
            continue;
        }
        if (sector.offset != cursor || sector.size > end - cursor ||
            (erase_size != 0 && sector.size != erase_size)) {
            return NX_ERROR_UNSUPPORTED;
        }
        erase_size = sector.size;
        cursor += sector.size;
    }
    if (cursor != end || erase_size == 0 ||
        erase_size % geometry->program_unit != 0) {
        return NX_ERROR_UNSUPPORTED;
    }
    adapter->region = region;
    adapter->deadline = deadline;
    nx_storage_port_t binding = {
        adapter,    region.size,   erase_size,  geometry->program_unit,
        read_bytes, program_bytes, erase_bytes, sync_bytes};
    *port = binding;
    return NX_SUCCESS;
}

/** \brief Change a deadline only between externally serialized transactions. */
void nx_flash_storage_set_deadline(nx_flash_storage_adapter_t* adapter,
                                   nx_time_us_t deadline) {
    if (adapter != NULL) {
        adapter->deadline = deadline;
    }
}
