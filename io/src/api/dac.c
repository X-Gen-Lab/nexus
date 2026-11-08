/**
 * \file            dac.c
 * \brief           Checked dispatch of caller-owned raw DAC operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/dac.h"

/** \brief Check complete, aligned storage without wrapping its end. */
static bool span(const void* data, size_t size, size_t alignment) {
    return data != NULL && (uintptr_t)data % alignment == 0U &&
           size <= UINTPTR_MAX - (uintptr_t)data;
}
/** \brief Both extents have already passed their nonwrapping checks. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Querying identity neither initializes nor acquires hardware. */
static bool valid(const nx_dac_port_t* port) {
    return span(port, sizeof(*port), _Alignof(nx_dac_port_t)) &&
           span(port->ops, sizeof(*port->ops), _Alignof(nx_dac_ops_t)) &&
           port->context != NULL;
}
/** \brief An observation cannot overwrite its own face or shared methods. */
static bool output(const nx_dac_port_t* port, const void* data, size_t size,
                   size_t alignment) {
    return span(data, size, alignment) &&
           !overlap(data, size, port, sizeof(*port)) &&
           !overlap(data, size, port->ops, sizeof(*port->ops));
}
/** \brief Obtain selected facts without starting conversion. */
nx_result_t nx_dac_port_info(const nx_dac_port_t* port, nx_dac_info_t* info) {
    if (!valid(port) ||
        !output(port, info, sizeof(*info), _Alignof(nx_dac_info_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->info == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->info(port->context, info);
}
/** \brief Scalar input is consumed synchronously and is never borrowed. */
nx_result_t nx_dac_port_write(const nx_dac_port_t* port, uint8_t channel,
                              uint16_t code) {
    if (!valid(port) || channel < 1U || channel > 2U || code > 4095U) {
        return NX_ERROR_INVALID;
    }
    return port->ops->write == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->write(port->context, channel, code);
}
/** \brief One selected provider owns simultaneous pair staging. */
nx_result_t nx_dac_port_write_pair(const nx_dac_port_t* port, uint16_t first,
                                   uint16_t second) {
    if (!valid(port) || first > 4095U || second > 4095U) {
        return NX_ERROR_INVALID;
    }
    return port->ops->write_pair == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->write_pair(port->context, first, second);
}
/** \brief Preserve the caller's absolute monotonic deadline. */
nx_result_t nx_dac_port_start(const nx_dac_port_t* port, uint32_t trigger_hz,
                              nx_time_us_t deadline) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->start(port->context, trigger_hz, deadline);
}
/** \brief Public masks never substitute for encoded vendor channel literals. */
nx_result_t nx_dac_port_trigger(const nx_dac_port_t* port, uint8_t channels) {
    if (!valid(port) || channels == 0U || (channels & ~3U) != 0U) {
        return NX_ERROR_INVALID;
    }
    return port->ops->trigger == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->trigger(port->context, channels);
}
/** \brief A failed stop retains the same hardware responsibility. */
nx_result_t nx_dac_port_stop(const nx_dac_port_t* port, nx_time_us_t deadline) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context, deadline);
}
/** \brief Forward digital observation into independent caller storage. */
nx_result_t nx_dac_port_state(const nx_dac_port_t* port,
                              nx_dac_state_t* state) {
    if (!valid(port) ||
        !output(port, state, sizeof(*state), _Alignof(nx_dac_state_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}
