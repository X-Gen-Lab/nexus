/**
 * \file            cycle_probe.c
 * \brief           Optional raw cycle collection and typed service dispatch.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/cycle_probe.h"
#include "nexus/arch/arch.h"

/** \brief Bounds precede every pointer addition or metadata access. */
static bool span(const void* data, size_t bytes, size_t alignment) {
    return data != NULL && (uintptr_t)data % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)data;
}
/** \brief Only validated extents participate in overlap checks. */
static bool overlap(const void* first, size_t first_bytes, const void* last,
                    size_t last_bytes) {
    return (uintptr_t)first < (uintptr_t)last + last_bytes &&
           (uintptr_t)last < (uintptr_t)first + first_bytes;
}
/** \brief Diagnostic counters never acquire a new meaning by wrapping. */
static void increment(uint32_t* value) {
    if (*value != UINT32_MAX) {
        ++*value;
    }
}
/** \brief Tokens never share storage with their producer metadata. */
static bool pair_valid(const nx_io_cycle_probe_t* probe,
                       const nx_io_cycle_token_t* token) {
    return span(probe, sizeof(*probe), _Alignof(nx_io_cycle_probe_t)) &&
           span(token, sizeof(*token), _Alignof(nx_io_cycle_token_t)) &&
           !overlap(probe, sizeof(*probe), token, sizeof(*token));
}

nx_result_t nx_io_cycle_probe_begin(nx_io_cycle_probe_t* probe,
                                    nx_io_cycle_token_t* token) {
    if (!pair_valid(probe, token)) {
        return NX_ERROR_INVALID;
    }
    if (token->owner != NULL) {
        return NX_ERROR_STATE;
    }
    if (probe->observation.active) {
        increment(&probe->observation.reentries);
        return NX_ERROR_BUSY;
    }
    probe->observation.active = true;
    increment(&probe->observation.attempts);
    token->owner = probe;
    token->available = nx_arch_cycle_snapshot(&token->before);
    return NX_SUCCESS;
}

nx_result_t nx_io_cycle_probe_end(nx_io_cycle_probe_t* probe,
                                  nx_io_cycle_token_t* token) {
    if (!pair_valid(probe, token)) {
        return NX_ERROR_INVALID;
    }
    if (token->owner != probe || !probe->observation.active) {
        return NX_ERROR_STATE;
    }
    uint32_t after = 0U;
    bool available = nx_arch_cycle_snapshot(&after) && token->available;
    nx_io_cycle_observation_t* observation = &probe->observation;
    observation->last_available = available;
    observation->elapsed_known = false;
    if (available) {
        increment(&observation->raw_pairs);
        observation->last_begin = token->before;
        observation->last_end = after;
        observation->last_modulo_cycles = after - token->before;
        if (observation->last_modulo_cycles >
            observation->maximum_modulo_cycles) {
            observation->maximum_modulo_cycles =
                observation->last_modulo_cycles;
        }
    } else {
        increment(&observation->unavailable_pairs);
    }
    token->owner = NULL;
    token->available = false;
    observation->active = false;
    return NX_SUCCESS;
}

nx_result_t nx_io_cycle_probe_snapshot(const nx_io_cycle_probe_t* probe,
                                       nx_io_cycle_observation_t* out) {
    if (!span(probe, sizeof(*probe), _Alignof(nx_io_cycle_probe_t)) ||
        !span(out, sizeof(*out), _Alignof(nx_io_cycle_observation_t)) ||
        overlap(probe, sizeof(*probe), out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    *out = probe->observation;
    out->elapsed_known = false;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Do not let optional collection overwrite a known immutable face. */
static bool usable_probe(const nx_io_cycle_probe_t* probe, const void* face,
                         size_t face_bytes, const void* methods,
                         size_t methods_bytes, const void* context) {
    return span(probe, sizeof(*probe), _Alignof(nx_io_cycle_probe_t)) &&
           span(methods, methods_bytes, 1U) && span(context, 1U, 1U) &&
           !overlap(probe, sizeof(*probe), face, face_bytes) &&
           !overlap(probe, sizeof(*probe), methods, methods_bytes) &&
           !overlap(probe, sizeof(*probe), context, 1U);
}

nx_result_t nx_uart_word_port_service_observed(const nx_uart_word_port_t* port,
                                               nx_io_cycle_probe_t* probe) {
    if (!span(port, sizeof(*port), _Alignof(nx_uart_word_port_t)) ||
        port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->service == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_io_cycle_token_t token = {NULL, 0U, false};
    if (usable_probe(probe, port, sizeof(*port), port->ops, sizeof(*port->ops),
                     port->context)) {
        (void)nx_io_cycle_probe_begin(probe, &token);
    }
    nx_uart_word_port_service(port);
    if (token.owner != NULL) {
        (void)nx_io_cycle_probe_end(probe, &token);
    }
    return NX_SUCCESS;
}

nx_result_t nx_spi_dma_transaction_service_observed(
    const nx_spi_dma_transaction_port_t* port, nx_io_cycle_probe_t* probe) {
    if (!span(port, sizeof(*port), _Alignof(nx_spi_dma_transaction_port_t)) ||
        port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->service == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_io_cycle_token_t token = {NULL, 0U, false};
    if (usable_probe(probe, port, sizeof(*port), port->ops, sizeof(*port->ops),
                     port->context)) {
        (void)nx_io_cycle_probe_begin(probe, &token);
    }
    nx_spi_dma_transaction_service(port);
    if (token.owner != NULL) {
        (void)nx_io_cycle_probe_end(probe, &token);
    }
    return NX_SUCCESS;
}
