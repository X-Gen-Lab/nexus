/**
 * \file            crc.c
 * \brief           Checked dispatch of explicit CRC-32 word sequences
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/crc.h"
/** \brief Reject malformed storage before either metadata or data access. */
static bool span(const void* pointer, size_t size, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           size <= UINTPTR_MAX - (uintptr_t)pointer;
}
/** \brief All extents are validated before comparing their boundaries. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return an != 0U && bn != 0U && (uintptr_t)a < (uintptr_t)b + bn &&
           (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Preserve explicit word units and the caller's absolute deadline. */
nx_result_t nx_crc_port_compute(const nx_crc_port_t* port,
                                const uint32_t* words, size_t count,
                                nx_time_us_t deadline, uint32_t* result) {
    if (!span(port, sizeof(*port), _Alignof(nx_crc_port_t)) ||
        !span(port->ops, sizeof(*port->ops), _Alignof(nx_crc_ops_t)) ||
        port->context == NULL || count > SIZE_MAX / sizeof(*words) ||
        (count != 0U &&
         !span(words, count * sizeof(*words), _Alignof(uint32_t))) ||
        !span(result, sizeof(*result), _Alignof(uint32_t)) ||
        overlap(result, sizeof(*result), port, sizeof(*port)) ||
        overlap(result, sizeof(*result), port->ops, sizeof(*port->ops)) ||
        overlap(result, sizeof(*result), words, count * sizeof(*words))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->compute == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->compute(port->context, words, count, deadline,
                                    result);
}
