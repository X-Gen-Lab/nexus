/**
 * \file            entropy.c
 * \brief           Checked entropy dispatch and finite all-or-erased filling
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/entropy.h"

/** \brief Validate complete spans before typed access or address arithmetic. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Compare already admitted nonwrapping spans. */
static bool overlaps(const void* left, size_t left_size, const void* right,
                     size_t right_size) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    return left_size != 0U && right_size != 0U &&
           (a <= b ? b - a < left_size : a - b < right_size);
}

/** \brief Establish readable immutable dispatch metadata. */
static bool face_valid(const nx_entropy_port_t* port) {
    return span(port, sizeof(*port), _Alignof(nx_entropy_port_t)) &&
           port->context &&
           span(port->ops, sizeof(*port->ops), _Alignof(nx_entropy_ops_t));
}

/** \brief Reject known aliases without guessing the opaque context's extent. */
static bool output_valid(const nx_entropy_port_t* port, const void* output,
                         size_t bytes, size_t alignment) {
    return span(output, bytes, alignment) &&
           !overlaps(output, bytes, port, sizeof(*port)) &&
           !overlaps(output, bytes, port->ops, sizeof(*port->ops)) &&
           output != port->context;
}

/** \brief Volatile stores prevent dead-store removal of failure erasure. */
static void erase(void* output, size_t bytes) {
    volatile uint8_t* cursor = output;
    while (bytes-- != 0U)
        *cursor++ = 0U;
}

/** \brief Dispatch one bounded word attempt through the selected static face.
 */
nx_result_t nx_entropy_port_try_read(const nx_entropy_port_t* port,
                                     uint32_t* word) {
    if (!face_valid(port) ||
        !output_valid(port, word, sizeof(*word), _Alignof(uint32_t)))
        return NX_ERROR_INVALID;
    return port->ops->try_read ? port->ops->try_read(port->context, word)
                               : NX_ERROR_UNSUPPORTED;
}

/** \brief Poll only inside the explicit live monotonic clock budget. */
nx_result_t nx_entropy_fill(const nx_entropy_port_t* port, void* output,
                            size_t bytes, const nx_clock_t* clock,
                            nx_time_us_t deadline) {
    if (!face_valid(port) ||
        !span(clock, sizeof(*clock), _Alignof(nx_clock_t)) || !clock->read ||
        deadline == NX_DEADLINE_NEVER ||
        (bytes != 0U && (!output_valid(port, output, bytes, 1U) ||
                         overlaps(output, bytes, clock, sizeof(*clock)) ||
                         output == clock->context)))
        return NX_ERROR_INVALID;
    if (!port->ops->try_read)
        return NX_ERROR_UNSUPPORTED;
    if (bytes == 0U)
        return NX_SUCCESS;
    uint8_t* cursor = output;
    size_t completed = 0U;
    nx_time_us_t previous = 0U;
    nx_result_t result = NX_SUCCESS;
    while (completed < bytes) {
        nx_time_us_t now = clock->read(clock->context);
        if (now == NX_DEADLINE_NEVER || now < previous) {
            result = NX_ERROR_STATE;
            break;
        }
        previous = now;
        if (now >= deadline) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t word = 0U;
        result = nx_entropy_port_try_read(port, &word);
        if (result == NX_ERROR_BUSY) {
            erase(&word, sizeof(word));
            continue;
        }
        if (result != NX_SUCCESS) {
            erase(&word, sizeof(word));
            break;
        }
        now = clock->read(clock->context);
        if (now == NX_DEADLINE_NEVER || now < previous) {
            result = NX_ERROR_STATE;
        } else if (now >= deadline) {
            result = NX_ERROR_TIMEOUT;
        } else {
            previous = now;
            for (unsigned byte = 0U; byte < 4U && completed < bytes; ++byte)
                cursor[completed++] = (uint8_t)(word >> (8U * byte));
        }
        erase(&word, sizeof(word));
        if (result != NX_SUCCESS)
            break;
    }
    if (result != NX_SUCCESS)
        erase(output, bytes);
    return result;
}
