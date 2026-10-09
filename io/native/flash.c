/**
 * \file            flash.c
 *
 * \brief           Nonpersistent NOR geometry/settlement model, not MCU Flash.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/native/model.h"
#include <string.h>

struct nx_flash_port {
    uint8_t* memory;
    const nx_flash_geometry_t* geometry;
    size_t pulses_left;
    bool active;
};
nx_flash_port_t g_nx_native_flash;
nx_flash_port_t* const nx_native_flash = &g_nx_native_flash;

/** \brief Validate geometry before exposing RAM as a deliberately named model.
 */
nx_result_t nx_native_flash_configure(uint8_t* memory,
                                      const nx_flash_geometry_t* geometry) {
    if (memory == NULL || geometry == NULL || geometry->size == 0 ||
        geometry->program_unit == 0 || geometry->sectors == NULL ||
        geometry->sector_count == 0 ||
        geometry->size % geometry->program_unit != 0) {
        return NX_ERROR_INVALID;
    }
    uint32_t end = 0;
    for (size_t i = 0; i < geometry->sector_count; i++) {
        const nx_flash_sector_t* sector = &geometry->sectors[i];
        if (sector->offset != end || sector->size == 0 ||
            sector->size % geometry->program_unit != 0 ||
            sector->size > geometry->size - end) {
            return NX_ERROR_INVALID;
        }
        end += sector->size;
    }
    if (end != geometry->size) {
        return NX_ERROR_INVALID;
    }
    g_nx_native_flash = (nx_flash_port_t){
        .memory = memory, .geometry = geometry, .pulses_left = SIZE_MAX};
    memset(memory, 0xff, geometry->size);
    return NX_SUCCESS;
}

/** \brief Return maintained full geometry only for configured model storage. */
const nx_flash_geometry_t* nx_flash_port_geometry(const nx_flash_port_t* port) {
    return port == NULL ? NULL : port->geometry;
}

/** \brief Reject every physical overflow before accessing model memory. */
static bool valid_range(const nx_flash_port_t* port, uint32_t offset,
                        size_t length) {
    return port != NULL && port->geometry != NULL &&
           offset <= port->geometry->size &&
           length <= port->geometry->size - offset;
}

/** \brief Read bounded bytes without any persistent-memory support claim. */
nx_result_t nx_flash_port_read(const nx_flash_port_t* port, uint32_t offset,
                               void* data, size_t length) {
    if (!valid_range(port, offset, length) || (length != 0 && data == NULL)) {
        return NX_ERROR_INVALID;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (length != 0) {
        memcpy(data, &port->memory[offset], length);
    }
    return NX_SUCCESS;
}

/** \brief Consume a complete modeled pulse or reject before its first access.
 */
static nx_result_t begin_pulse(nx_flash_port_t* port, nx_time_us_t deadline) {
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (port->pulses_left == 0) {
        return NX_ERROR_IO;
    }
    if (port->pulses_left != SIZE_MAX) {
        port->pulses_left--;
    }
    return NX_SUCCESS;
}

/** \brief Model whole program pulses, preserving NOR zero-to-one constraints.
 */
nx_result_t nx_flash_port_program(nx_flash_port_t* port, uint32_t offset,
                                  const void* data, size_t length,
                                  nx_time_us_t deadline) {
    if (!valid_range(port, offset, length) || data == NULL || length == 0 ||
        offset % port->geometry->program_unit != 0 ||
        length % port->geometry->program_unit != 0) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    const uint8_t* source = data;
    for (size_t i = 0; i < length; i++) {
        if ((port->memory[offset + i] & source[i]) != source[i]) {
            return NX_ERROR_IO;
        }
    }
    port->active = true;
    nx_result_t result = NX_SUCCESS;
    for (size_t i = 0; i < length; i += port->geometry->program_unit) {
        result = begin_pulse(port, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        memcpy(&port->memory[offset + i], &source[i],
               port->geometry->program_unit);
        (void)nx_native_clock_advance(10);
    }
    port->active = false;
    return result;
}

/** \brief Validate full-sector boundaries before beginning any irreversible
 * pulse. */
nx_result_t nx_flash_port_erase(nx_flash_port_t* port, uint32_t offset,
                                size_t length, nx_time_us_t deadline) {
    if (!valid_range(port, offset, length) || length == 0) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    size_t first = SIZE_MAX;
    size_t last = SIZE_MAX;
    uint32_t end = offset + (uint32_t)length;
    for (size_t i = 0; i < port->geometry->sector_count; i++) {
        const nx_flash_sector_t* sector = &port->geometry->sectors[i];
        if (sector->offset == offset) {
            first = i;
        }
        if (sector->offset + sector->size == end) {
            last = i;
        }
    }
    if (first == SIZE_MAX || last == SIZE_MAX || first > last) {
        return NX_ERROR_INVALID;
    }
    port->active = true;
    nx_result_t result = NX_SUCCESS;
    for (size_t i = first; i <= last; i++) {
        result = begin_pulse(port, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        const nx_flash_sector_t* sector = &port->geometry->sectors[i];
        memset(&port->memory[sector->offset], 0xff, sector->size);
        (void)nx_native_clock_advance(100);
    }
    port->active = false;
    return result;
}

/** \brief Inject partial physical-operation model failure at pulse boundaries.
 */
void nx_native_flash_fault(size_t pulse_count) {
    g_nx_native_flash.pulses_left = pulse_count;
}
