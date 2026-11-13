/**
 * \file            audio.c
 * \brief           Checked typed audio operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/audio.h"
#include "dispatch_checks.h"

/** \brief Lookup validates identity and never initializes private storage. */
static bool valid(const nx_audio_port_t* port) {
    return nx_io_dispatch_span(port, sizeof(*port),
                               _Alignof(nx_audio_port_t)) &&
           nx_io_dispatch_span(port->ops, sizeof(*port->ops),
                               _Alignof(nx_audio_ops_t)) &&
           port->context != NULL;
}
/** \brief Preserve original argument identity for the provider's private
 * checks. */
static bool independent(const nx_audio_port_t* port, const void* pointer,
                        size_t bytes, size_t alignment) {
    return nx_io_dispatch_span(pointer, bytes, alignment) &&
           !nx_io_dispatch_overlap(pointer, bytes, port, sizeof(*port)) &&
           !nx_io_dispatch_overlap(pointer, bytes, port->ops,
                                   sizeof(*port->ops));
}

/** \brief Admit one finite audio block without converting its stored samples.
 */
nx_result_t nx_audio_port_submit(const nx_audio_port_t* port,
                                 nx_audio_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_audio_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}

/** \brief Advance bounded completion and withdrawal; final results belong to
 * requests. */
nx_result_t nx_audio_port_service(const nx_audio_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

/** \brief Request finite-block withdrawal without revoking any outstanding
 * loan. */
nx_result_t nx_audio_port_cancel(const nx_audio_port_t* port,
                                 nx_audio_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_audio_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}

/** \brief Join finite or continuous producers; consumer RX loans remain
 * independent. */
nx_result_t nx_audio_port_stop(const nx_audio_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}

/** \brief Copy the exact nominal rate; an external unknown rate remains
 * unknown. */
nx_result_t nx_audio_port_rate(const nx_audio_port_t* port,
                               nx_audio_rate_t* rate) {
    if (!valid(port) ||
        !independent(port, rate, sizeof(*rate), _Alignof(nx_audio_rate_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->rate == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->rate(port->context, rate);
}

/** \brief Admit a continuous caller-owned session without hidden replenishment.
 */
nx_result_t nx_audio_port_start_blocks(const nx_audio_port_t* port,
                                       nx_audio_blocks_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_audio_blocks_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start_blocks == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->start_blocks(port->context, request);
}

/** \brief Copy exact storage layout without converting or borrowing payloads.
 */
nx_result_t nx_audio_port_layout(const nx_audio_port_t* port,
                                 nx_audio_layout_t* result) {
    if (!valid(port) || !independent(port, result, sizeof(*result),
                                     _Alignof(nx_audio_layout_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->layout == NULL ? NX_ERROR_UNSUPPORTED
                                     : port->ops->layout(port->context, result);
}
