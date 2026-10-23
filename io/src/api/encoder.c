/**
 * \file            encoder.c
 * \brief           Checked dispatch for optional raw quadrature observations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/encoder.h"

/** \brief Check aligned exclusive-end arithmetic before descriptor access. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)pointer;
}
/** \brief Both extents have already passed their wrapping checks. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Lookup yields identity; ownership remains the caller's contract. */
static bool valid(const nx_encoder_t* port) {
    return span(port, sizeof(*port), _Alignof(nx_encoder_t)) &&
           span(port->ops, sizeof(*port->ops), _Alignof(nx_encoder_ops_t)) &&
           port->context != NULL;
}
/** \brief A snapshot cannot overwrite its face or immutable shared methods. */
static bool output(const nx_encoder_t* port, const void* pointer, size_t bytes,
                   size_t alignment) {
    return span(pointer, bytes, alignment) &&
           !overlap(pointer, bytes, port, sizeof(*port)) &&
           !overlap(pointer, bytes, port->ops, sizeof(*port->ops));
}
/** \brief Dispatch the exact selected context without acquiring resources. */
nx_result_t nx_encoder_start(const nx_encoder_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->start(port->context);
}
/** \brief Only an observed provider stop can end its active source. */
nx_result_t nx_encoder_stop(const nx_encoder_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
/** \brief No policy, wrap extension or calibration is hidden in dispatch. */
nx_result_t nx_encoder_read(const nx_encoder_t* port,
                            nx_encoder_sample_t* sample) {
    if (!valid(port) ||
        !output(port, sample, sizeof(*sample), _Alignof(nx_encoder_sample_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->read == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->read(port->context, sample);
}
/** \brief Provider state remains private and immutable facts stay exact. */
nx_result_t nx_encoder_state(const nx_encoder_t* port,
                             nx_encoder_state_t* state) {
    if (!valid(port) ||
        !output(port, state, sizeof(*state), _Alignof(nx_encoder_state_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}
