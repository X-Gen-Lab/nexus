/**
 * \file            fal.c
 * \brief           Exact physical-region access with retained failure ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fal.h"
#include <limits.h>

/** \brief Product reservations own complete physical sectors, never rounding.
 */
static nx_result_t complete_region(const nx_flash_region_t* region) {
    nx_result_t result = nx_flash_region_validate(region);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!region->writable) {
        return NX_ERROR_PERMISSION;
    }
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(region->port);
    uint32_t ordinal;
    uint32_t end = region->offset + region->size;
    if (nx_flash_geometry_sector_index(geometry, region->offset, &ordinal) !=
            NX_SUCCESS ||
        (end != geometry->size && nx_flash_geometry_sector_index(
                                      geometry, end, &ordinal) != NX_SUCCESS)) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

/** \brief Validate regions without creating another partition authority. */
nx_result_t nx_fal_layout_validate(const nx_flash_region_t* regions,
                                   size_t count) {
    if ((regions == NULL && count != 0U) ||
        count > SIZE_MAX / sizeof(*regions)) {
        return NX_ERROR_INVALID;
    }
    for (size_t index = 0U; index < count; ++index) {
        const nx_flash_region_t* current = &regions[index];
        nx_result_t result = complete_region(current);
        if (result != NX_SUCCESS) {
            return result;
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            const nx_flash_region_t* other = &regions[previous];
            if (current->port->context == other->port->context &&
                current->offset < other->offset + other->size &&
                other->offset < current->offset + current->size) {
                return NX_ERROR_INVALID;
            }
        }
    }
    return NX_SUCCESS;
}

/** \brief Bind only the media profile actually supported by these consumers. */
nx_result_t nx_fal_bind(nx_fal_t* adapter, nx_flash_region_t region,
                        nx_clock_t clock) {
    if (adapter == NULL || clock.read == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = complete_region(&region);
    if (result != NX_SUCCESS) {
        return result;
    }
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(region.port);
    if (geometry->program_flags != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t ordinal;
    nx_flash_sector_t first;
    if (nx_flash_geometry_sector_index(geometry, region.offset, &ordinal) !=
            NX_SUCCESS ||
        nx_flash_geometry_sector_at(geometry, ordinal, &first) != NX_SUCCESS) {
        return NX_ERROR_INVALID;
    }
    uint32_t end = region.offset + region.size;
    for (size_t index = 0U; index < geometry->erase_span_count; ++index) {
        const nx_flash_erase_span_t* span = &geometry->erase_spans[index];
        uint64_t span_end =
            (uint64_t)span->offset + (uint64_t)span->erase_bytes * span->count;
        if (span->offset < end && region.offset < span_end &&
            span->erase_bytes != first.size) {
            return NX_ERROR_UNSUPPORTED;
        }
    }
    nx_fal_t bound = {0};
    bound.region = region;
    bound.clock = clock;
    bound.program_bytes = geometry->program_unit;
    bound.erase_bytes = first.size;
    *adapter = bound;
    return NX_SUCCESS;
}

/** \brief One monotonic finite budget gates every callback, including reads. */
static nx_result_t budget(nx_fal_t* adapter) {
    nx_time_us_t now = adapter->clock.read(adapter->clock.context);
    if (now == NX_DEADLINE_NEVER || now < adapter->last_time) {
        return NX_ERROR_STATE;
    }
    adapter->last_time = now;
    return nx_deadline_expired(adapter->deadline, now) ? NX_ERROR_TIMEOUT
                                                       : NX_SUCCESS;
}

/** \brief Start logical use only; physical serialization remains external. */
nx_result_t nx_fal_begin(nx_fal_t* adapter, nx_time_us_t deadline) {
    if (adapter == NULL || adapter->region.port == NULL ||
        adapter->clock.read == NULL || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (adapter->active) {
        return NX_ERROR_BUSY;
    }
    if (adapter->faulted) {
        return NX_ERROR_STATE;
    }
    nx_time_us_t now = adapter->clock.read(adapter->clock.context);
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    adapter->deadline = deadline;
    adapter->last_time = now;
    adapter->error = NX_SUCCESS;
    adapter->active = true;
    return NX_SUCCESS;
}

/** \brief Finish callback access without upgrading a failed operation. */
nx_result_t nx_fal_end(nx_fal_t* adapter) {
    if (adapter == NULL || !adapter->active) {
        return NX_ERROR_STATE;
    }
    if (adapter->error == NX_SUCCESS) {
        adapter->error = budget(adapter);
    }
    adapter->active = false;
    return adapter->error;
}

/** \brief Preserve the first error even if upstream ignores a callback result.
 */
static int fail(nx_fal_t* adapter, nx_result_t result, bool mutation) {
    if (adapter != NULL && adapter->active) {
        if (adapter->error == NX_SUCCESS) {
            adapter->error = result;
        }
        if (mutation) {
            adapter->faulted = true;
            adapter->recovery_required = true;
        }
    }
    return -1;
}

/** \brief Check integer bounds before dispatching or retaining any payload. */
static nx_result_t admit(nx_fal_t* adapter, uint32_t offset, size_t bytes) {
    if (adapter == NULL || !adapter->active) {
        return NX_ERROR_STATE;
    }
    if (adapter->error != NX_SUCCESS) {
        return adapter->error;
    }
    if (bytes == 0U || bytes > INT_MAX || offset > adapter->region.size ||
        bytes > adapter->region.size - offset) {
        return NX_ERROR_INVALID;
    }
    return budget(adapter);
}

/** \brief Reads share the transaction budget without a hidden timeout reset. */
int nx_fal_read(nx_fal_t* adapter, uint32_t offset, void* data, size_t bytes) {
    nx_result_t result = admit(adapter, offset, bytes);
    if (result != NX_SUCCESS || data == NULL) {
        return fail(adapter, result != NX_SUCCESS ? result : NX_ERROR_INVALID,
                    false);
    }
    result = nx_flash_region_read(&adapter->region, offset, data, bytes);
    if (result == NX_SUCCESS) {
        result = budget(adapter);
    }
    return result == NX_SUCCESS ? (int)bytes : fail(adapter, result, false);
}

/** \brief Exact aligned writes; failed pulses keep the original owner alive. */
int nx_fal_program(nx_fal_t* adapter, uint32_t offset, const void* data,
                   size_t bytes) {
    nx_result_t result = admit(adapter, offset, bytes);
    if (result != NX_SUCCESS || data == NULL ||
        (adapter != NULL && adapter->program_bytes != 0U &&
         (offset % adapter->program_bytes != 0U ||
          bytes % adapter->program_bytes != 0U))) {
        return fail(adapter, result != NX_SUCCESS ? result : NX_ERROR_INVALID,
                    false);
    }
    result = nx_flash_region_program(&adapter->region, offset, data, bytes,
                                     adapter->deadline);
    return result == NX_SUCCESS ? (int)bytes : fail(adapter, result, true);
}

/** \brief Never enlarge an erase beyond the caller-selected whole sectors. */
int nx_fal_erase(nx_fal_t* adapter, uint32_t offset, size_t bytes) {
    nx_result_t result = admit(adapter, offset, bytes);
    if (result != NX_SUCCESS ||
        (adapter != NULL && adapter->erase_bytes != 0U &&
         (offset % adapter->erase_bytes != 0U ||
          bytes % adapter->erase_bytes != 0U))) {
        return fail(adapter, result != NX_SUCCESS ? result : NX_ERROR_INVALID,
                    false);
    }
    result = nx_flash_region_erase(&adapter->region, offset, bytes,
                                   adapter->deadline);
    return result == NX_SUCCESS ? (int)bytes : fail(adapter, result, true);
}

/** \brief Cleanup proves only hardware withdrawal, never database contents. */
nx_result_t nx_fal_recover(nx_fal_t* adapter) {
    if (adapter == NULL || adapter->region.port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (adapter->active) {
        return NX_ERROR_BUSY;
    }
    if (adapter->recovery_required) {
        nx_result_t result = nx_flash_port_recover(adapter->region.port);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    adapter->faulted = false;
    adapter->recovery_required = false;
    adapter->error = NX_SUCCESS;
    return NX_SUCCESS;
}
