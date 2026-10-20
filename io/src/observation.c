/**
 * \file            observation.c
 * \brief           Pure timestamp checks and explicit service dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/observation.h"

/** \brief Checked spans avoid ordering unrelated pointers and address wrap. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief Facts are supplied, never guessed from another observation kind. */
static bool valid_timestamp(const nx_io_timestamp_t* stamp) {
    return stamp->ticks_per_second != 0U && stamp->domain != 0U &&
           stamp->epoch != 0U &&
           (unsigned)stamp->source <=
               (unsigned)NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION;
}

/** \brief Preserve provenance and reject clock changes before subtraction. */
nx_result_t nx_io_timestamp_elapsed(const nx_io_timestamp_t* first,
                                    const nx_io_timestamp_t* last,
                                    nx_io_timestamp_delta_t* out) {
    if (first == NULL || last == NULL || out == NULL ||
        overlaps(first, sizeof(*first), out, sizeof(*out)) ||
        overlaps(last, sizeof(*last), out, sizeof(*out)) ||
        !valid_timestamp(first) || !valid_timestamp(last)) {
        return NX_ERROR_INVALID;
    }
    if (first->source == NX_IO_TIMESTAMP_UNKNOWN ||
        last->source == NX_IO_TIMESTAMP_UNKNOWN) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (first->domain != last->domain || first->epoch != last->epoch ||
        first->ticks_per_second != last->ticks_per_second ||
        last->ticks < first->ticks) {
        return NX_ERROR_STATE;
    }
    bool known = first->uncertainty_known && last->uncertainty_known;
    uint64_t uncertainty = 0U;
    if (known) {
        if (first->uncertainty_ticks > UINT64_MAX - last->uncertainty_ticks) {
            return NX_ERROR_EXHAUSTED;
        }
        uncertainty = first->uncertainty_ticks + last->uncertainty_ticks;
    }
    *out =
        (nx_io_timestamp_delta_t){.ticks = last->ticks - first->ticks,
                                  .ticks_per_second = first->ticks_per_second,
                                  .uncertainty_ticks = uncertainty,
                                  .uncertainty_known = known};
    return NX_SUCCESS;
}

/** \brief Unknown error never becomes a passed budget or a false overrun. */
nx_result_t nx_io_timestamp_budget(const nx_io_timestamp_t* first,
                                   const nx_io_timestamp_t* last,
                                   uint64_t maximum_ticks,
                                   nx_io_timestamp_budget_result_t* out) {
    if (first == NULL || last == NULL || out == NULL ||
        (uintptr_t)first % _Alignof(nx_io_timestamp_t) != 0U ||
        (uintptr_t)last % _Alignof(nx_io_timestamp_t) != 0U ||
        (uintptr_t)out % _Alignof(nx_io_timestamp_budget_result_t) != 0U ||
        overlaps(first, sizeof(*first), out, sizeof(*out)) ||
        overlaps(last, sizeof(*last), out, sizeof(*out)))
        return NX_ERROR_INVALID;
    nx_io_timestamp_budget_result_t result = {0};
    nx_result_t status = nx_io_timestamp_elapsed(first, last, &result.delta);
    if (status != NX_SUCCESS)
        return status;
    if (result.delta.uncertainty_known) {
        const uint64_t ticks = result.delta.ticks;
        const uint64_t uncertainty = result.delta.uncertainty_ticks;
        if (uncertainty > UINT64_MAX - ticks)
            return NX_ERROR_EXHAUSTED;
        result.lower_ticks = ticks > uncertainty ? ticks - uncertainty : 0U;
        result.upper_ticks = ticks + uncertainty;
        if (result.upper_ticks <= maximum_ticks)
            result.verdict = NX_IO_TIMESTAMP_BUDGET_WITHIN;
        else if (result.lower_ticks > maximum_ticks)
            result.verdict = NX_IO_TIMESTAMP_BUDGET_EXCEEDED;
    }
    *out = result;
    return NX_SUCCESS;
}

/** \brief One dispatch does not invent provider progress or final state. */
nx_result_t nx_uart_port_service_checked(const nx_uart_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->service == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    port->ops->service(port->context);
    return NX_SUCCESS;
}

/** \brief One dispatch preserves the selected provider and its exact owner. */
nx_result_t nx_spi_port_service_checked(const nx_spi_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->service == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    port->ops->service(port->context);
    return NX_SUCCESS;
}

/** \brief No retry, allocation, request access or recovery is implicit. */
nx_result_t nx_i2c_port_service_checked(const nx_i2c_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->service == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    port->ops->service(port->context);
    return NX_SUCCESS;
}
