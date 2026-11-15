/**
 * \file            at24c02.c
 * \brief           Settled EEPROM page operations and bounded ready observation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/at24c02.h"
#include <string.h>

/** \brief Validate known numeric spans before dereferencing control storage. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Known spans stay disjoint; opaque context extents are external. */
static bool overlap(const void* first, size_t first_bytes, const void* second,
                    size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief Reject aliasing with every known control span before any I/O. */
static bool separate_buffer(const nx_at24c02_t* device, const void* pointer,
                            size_t bytes, size_t alignment) {
    return span(pointer, bytes, alignment) &&
           !overlap(pointer, bytes, device, sizeof(*device)) &&
           !overlap(pointer, bytes, device->endpoint,
                    sizeof(*device->endpoint)) &&
           !overlap(pointer, bytes, device->endpoint->ops,
                    sizeof(*device->endpoint->ops)) &&
           !overlap(pointer, bytes, device->endpoint->context, 1U) &&
           (device->clock.context == NULL ||
            !overlap(pointer, bytes, device->clock.context, 1U));
}

/** \brief A rejected call cannot enter a live owner or select infinite wait. */
static nx_result_t admission(const nx_at24c02_t* device,
                             nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_at24c02_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call) {
        return NX_ERROR_BUSY;
    }
    if (!device->initialized) {
        return NX_ERROR_STATE;
    }
    return NX_SUCCESS;
}

/** \brief Clock discontinuity is terminal; numerical recovery is not proof. */
static nx_result_t observe_clock(nx_at24c02_t* device, nx_time_us_t deadline) {
    if (device->clock_failed) {
        return NX_ERROR_IO;
    }
    nx_time_us_t now = device->clock.read(device->clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        device->clock_failed = true;
        return NX_ERROR_IO;
    }
    device->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief A finite read probe never programs EEPROM or retains local spans. */
static nx_result_t observe_ready(nx_at24c02_t* device, nx_time_us_t deadline) {
    for (uint32_t attempt = 0U; attempt < device->max_ready_probes; ++attempt) {
        nx_result_t result = observe_clock(device, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        uint8_t ignored = 0U;
        nx_i2c_message_t probe = nx_i2c_message_read(&ignored, 1U);
        size_t transferred = 0U;
        result = nx_i2c_endpoint_transaction(device->endpoint, &probe, 1U,
                                             deadline, &transferred);
        if (result != NX_SUCCESS && result != NX_ERROR_NACK) {
            return result;
        }
        if (result == NX_SUCCESS && transferred != 1U) {
            return NX_ERROR_IO;
        }
        nx_result_t clock_result = observe_clock(device, deadline);
        if (clock_result != NX_SUCCESS) {
            return clock_result;
        }
        if (result == NX_SUCCESS) {
            /* ACK plus settled final STOP proves ready, never data contents. */
            device->write_pending = false;
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_BUSY;
}

nx_result_t nx_at24c02_init(nx_at24c02_t* device,
                            const nx_i2c_endpoint_t* endpoint, nx_clock_t clock,
                            uint16_t max_ready_probes) {
    if (!span(device, sizeof(*device), _Alignof(nx_at24c02_t))) {
        return NX_ERROR_INVALID;
    }
    if (device->initialized || device->in_call || device->write_pending) {
        return NX_ERROR_BUSY;
    }
    if (!span(endpoint, sizeof(*endpoint), _Alignof(nx_i2c_endpoint_t)) ||
        overlap(device, sizeof(*device), endpoint, sizeof(*endpoint)) ||
        clock.read == NULL || max_ready_probes == 0U ||
        !span(endpoint->ops, sizeof(*endpoint->ops),
              _Alignof(nx_i2c_endpoint_ops_t)) ||
        overlap(device, sizeof(*device), endpoint->ops,
                sizeof(*endpoint->ops)) ||
        endpoint->context == NULL ||
        overlap(device, sizeof(*device), endpoint->context, 1U) ||
        (clock.context != NULL &&
         overlap(device, sizeof(*device), clock.context, 1U))) {
        return NX_ERROR_INVALID;
    }
    if (endpoint->ops->transaction == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    *device = (nx_at24c02_t){.endpoint = endpoint,
                             .clock = clock,
                             .max_ready_probes = max_ready_probes,
                             .initialized = true};
    return NX_SUCCESS;
}

nx_result_t nx_at24c02_read(nx_at24c02_t* device, size_t offset, uint8_t* data,
                            size_t bytes, nx_time_us_t deadline) {
    nx_result_t result = admission(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (offset >= NX_AT24C02_BYTES || bytes == 0U ||
        bytes > NX_AT24C02_BYTES - offset ||
        !separate_buffer(device, data, bytes, 1U)) {
        return NX_ERROR_INVALID;
    }
    if (device->write_pending) {
        return NX_ERROR_BUSY;
    }
    device->in_call = true;
    result = observe_clock(device, deadline);
    if (result == NX_SUCCESS) {
        uint8_t address = (uint8_t)offset;
        nx_i2c_message_t messages[2] = {nx_i2c_message_write(&address, 1U),
                                        nx_i2c_message_read(data, bytes)};
        size_t transferred = 0U;
        result = nx_i2c_endpoint_transaction(device->endpoint, messages, 2U,
                                             deadline, &transferred);
        if (result == NX_SUCCESS) {
            result = transferred == bytes + 1U ? observe_clock(device, deadline)
                                               : NX_ERROR_IO;
        }
    }
    device->in_call = false;
    return result;
}

nx_result_t nx_at24c02_write(nx_at24c02_t* device, size_t offset,
                             const uint8_t* data, size_t bytes,
                             nx_time_us_t deadline,
                             nx_at24c02_progress_t* progress) {
    nx_result_t result = admission(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (offset >= NX_AT24C02_BYTES || bytes == 0U ||
        bytes > NX_AT24C02_BYTES - offset ||
        !separate_buffer(device, data, bytes, 1U) ||
        !separate_buffer(device, progress, sizeof(*progress),
                         _Alignof(nx_at24c02_progress_t)) ||
        overlap(data, bytes, progress, sizeof(*progress))) {
        return NX_ERROR_INVALID;
    }
    if (device->write_pending) {
        return NX_ERROR_BUSY;
    }
    nx_at24c02_progress_t completed = {0U, false};
    device->in_call = true;
    while (completed.confirmed_bytes < bytes) {
        result = observe_clock(device, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        size_t current = offset + completed.confirmed_bytes;
        size_t remaining = bytes - completed.confirmed_bytes;
        size_t count = NX_AT24C02_PAGE_BYTES - current % NX_AT24C02_PAGE_BYTES;
        if (count > remaining) {
            count = remaining;
        }
        uint8_t packet[NX_AT24C02_PAGE_BYTES + 1U];
        packet[0] = (uint8_t)current;
        memcpy(&packet[1], data + completed.confirmed_bytes, count);
        nx_i2c_message_t message = nx_i2c_message_write(packet, count + 1U);
        size_t transferred = 0U;
        /* An issued transfer may have accepted data before reporting failure.
         */
        device->write_pending = true;
        result = nx_i2c_endpoint_transaction(device->endpoint, &message, 1U,
                                             deadline, &transferred);
        if (result == NX_SUCCESS && transferred != count + 1U) {
            result = NX_ERROR_IO;
        }
        if (result == NX_SUCCESS) {
            result = observe_clock(device, deadline);
        }
        if (result == NX_SUCCESS) {
            result = observe_ready(device, deadline);
        }
        if (result != NX_SUCCESS) {
            break;
        }
        completed.confirmed_bytes += count;
    }
    completed.uncertain = device->write_pending;
    *progress = completed;
    device->in_call = false;
    return result;
}

nx_result_t nx_at24c02_sync(nx_at24c02_t* device, nx_time_us_t deadline) {
    nx_result_t result = admission(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    device->in_call = true;
    result = observe_clock(device, deadline);
    if (result == NX_SUCCESS && device->write_pending) {
        result = observe_ready(device, deadline);
    }
    device->in_call = false;
    return result;
}

nx_result_t nx_at24c02_stop(nx_at24c02_t* device) {
    if (!span(device, sizeof(*device), _Alignof(nx_at24c02_t))) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call || device->write_pending) {
        return NX_ERROR_BUSY;
    }
    *device = (nx_at24c02_t){0};
    return NX_SUCCESS;
}
