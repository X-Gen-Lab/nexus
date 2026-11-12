/**
 * \file            flash.c
 * \brief           Checked Flash dispatch through shared provider methods
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/flash.h"

/** \brief Dispatch using this face's exact instance state. */
const nx_flash_geometry_t* nx_flash_port_geometry(const nx_flash_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NULL;
    }
    if (port->ops->geometry == NULL) {
        return NULL;
    }
    return port->ops->geometry(port->context);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_flash_port_read(const nx_flash_port_t* port, uint32_t offset,
                               void* data, size_t length) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->read == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->read(port->context, offset, data, length);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_flash_port_program(const nx_flash_port_t* port, uint32_t offset,
                                  const void* data, size_t length,
                                  nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->program == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->program(port->context, offset, data, length, deadline);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_flash_port_erase(const nx_flash_port_t* port, uint32_t offset,
                                size_t length, nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->erase == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->erase(port->context, offset, length, deadline);
}

/** \brief Dispatch using this face's exact retained physical owner. */
nx_result_t nx_flash_port_recover(const nx_flash_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return NX_ERROR_INVALID;
    if (port->ops->recover == NULL)
        return NX_ERROR_UNSUPPORTED;
    return port->ops->recover(port->context);
}

/** \brief Reject aliased outputs before any query can alter input metadata. */
static bool geometry_output(const nx_flash_geometry_t* geometry,
                            const void* output, size_t bytes,
                            size_t alignment) {
    uintptr_t descriptor = (uintptr_t)geometry;
    uintptr_t target = (uintptr_t)output;
    if (geometry == NULL || output == NULL ||
        descriptor % _Alignof(nx_flash_geometry_t) != 0U ||
        target % alignment != 0U ||
        sizeof(*geometry) > UINTPTR_MAX - descriptor ||
        bytes > UINTPTR_MAX - target || geometry->erase_spans == NULL ||
        geometry->erase_span_count == 0U ||
        geometry->erase_span_count > SIZE_MAX / sizeof(*geometry->erase_spans))
        return false;
    uintptr_t spans = (uintptr_t)geometry->erase_spans;
    size_t extent = geometry->erase_span_count * sizeof(*geometry->erase_spans);
    if (spans % _Alignof(nx_flash_erase_span_t) != 0U ||
        extent > UINTPTR_MAX - spans)
        return false;
    return !(target < descriptor + sizeof(*geometry) &&
             descriptor < target + bytes) &&
           !(target < spans + extent && spans < target + bytes);
}

/** \brief One checked interpretation serves every complete physical layout. */
static bool geometry_count(const nx_flash_geometry_t* geometry,
                           uint32_t* result) {
    if (geometry->size == 0U || geometry->program_unit == 0U)
        return false;
    uint64_t cursor = 0U;
    uint64_t count = 0U;
    for (size_t index = 0U; index < geometry->erase_span_count; ++index) {
        const nx_flash_erase_span_t* span = &geometry->erase_spans[index];
        if (span->offset != cursor || span->erase_bytes == 0U ||
            span->count == 0U ||
            span->erase_bytes % geometry->program_unit != 0U)
            return false;
        uint64_t bytes = (uint64_t)span->erase_bytes * span->count;
        if (bytes > (uint64_t)geometry->size - cursor)
            return false;
        cursor += bytes;
        count += span->count;
        if (count > UINT32_MAX)
            return false;
    }
    if (cursor != geometry->size || count == 0U)
        return false;
    *result = (uint32_t)count;
    return true;
}

/** \brief Derive a complete count without publishing partially valid facts. */
nx_result_t nx_flash_geometry_sector_count(const nx_flash_geometry_t* geometry,
                                           uint32_t* count) {
    if (!geometry_output(geometry, count, sizeof(*count), _Alignof(uint32_t)))
        return NX_ERROR_INVALID;
    uint32_t result;
    if (!geometry_count(geometry, &result))
        return NX_ERROR_INVALID;
    *count = result;
    return NX_SUCCESS;
}

/** \brief Return a value from one span, never a retained metadata pointer. */
nx_result_t nx_flash_geometry_sector_at(const nx_flash_geometry_t* geometry,
                                        uint32_t ordinal,
                                        nx_flash_sector_t* sector) {
    if (!geometry_output(geometry, sector, sizeof(*sector),
                         _Alignof(nx_flash_sector_t)))
        return NX_ERROR_INVALID;
    uint32_t count;
    if (!geometry_count(geometry, &count) || ordinal >= count)
        return NX_ERROR_INVALID;
    for (size_t index = 0U; index < geometry->erase_span_count; ++index) {
        const nx_flash_erase_span_t* span = &geometry->erase_spans[index];
        if (ordinal < span->count) {
            uint64_t offset =
                (uint64_t)span->offset + (uint64_t)ordinal * span->erase_bytes;
            *sector = (nx_flash_sector_t){(uint32_t)offset, span->erase_bytes};
            return NX_SUCCESS;
        }
        ordinal -= span->count;
    }
    return NX_ERROR_INVALID;
}

/** \brief Only an exact sector start is an erase boundary inside the chip. */
nx_result_t nx_flash_geometry_sector_index(const nx_flash_geometry_t* geometry,
                                           uint32_t offset, uint32_t* ordinal) {
    if (!geometry_output(geometry, ordinal, sizeof(*ordinal),
                         _Alignof(uint32_t)))
        return NX_ERROR_INVALID;
    uint32_t count;
    if (!geometry_count(geometry, &count) || offset >= geometry->size)
        return NX_ERROR_INVALID;
    uint64_t preceding = 0U;
    for (size_t index = 0U; index < geometry->erase_span_count; ++index) {
        const nx_flash_erase_span_t* span = &geometry->erase_spans[index];
        uint64_t end =
            (uint64_t)span->offset + (uint64_t)span->erase_bytes * span->count;
        if (offset >= span->offset && offset < end) {
            uint32_t displacement = offset - span->offset;
            if (displacement % span->erase_bytes != 0U)
                return NX_ERROR_INVALID;
            *ordinal = (uint32_t)(preceding + displacement / span->erase_bytes);
            return NX_SUCCESS;
        }
        preceding += span->count;
    }
    return NX_ERROR_INVALID;
}
