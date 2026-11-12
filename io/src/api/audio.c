/**
 * \file            audio.c
 * \brief           Checked read-only audio storage dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/audio.h"

/** \brief Validate complete objects before reading pointers or overlap ends. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)pointer;
}
/** \brief Both complete ranges have already passed arithmetic admission. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}

nx_result_t nx_audio_port_layout(const nx_audio_port_t* port,
                                 nx_audio_layout_t* result) {
    if (!span(port, sizeof(*port), _Alignof(nx_audio_port_t)) ||
        !span(port->ops, sizeof(*port->ops), _Alignof(nx_audio_ops_t)) ||
        port->context == NULL ||
        !span(result, sizeof(*result), _Alignof(nx_audio_layout_t)) ||
        overlap(result, sizeof(*result), port, sizeof(*port)) ||
        overlap(result, sizeof(*result), port->ops, sizeof(*port->ops))) {
        return NX_ERROR_INVALID;
    }
    /* Preserve the caller's identity for private state and active-loan guards.
     */
    return port->ops->layout == NULL ? NX_ERROR_UNSUPPORTED
                                     : port->ops->layout(port->context, result);
}
