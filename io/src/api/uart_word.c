/**
 * \file            uart_word.c
 * \brief           Nine-bit preparation and exact face dispatch.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/uart_word.h"

/** \brief Unknown encodings never silently select a different format. */
nx_result_t nx_uart_format_validate(nx_uart_format_t format) {
    if ((format.parity != NX_UART_PARITY_NONE &&
         format.parity != NX_UART_PARITY_EVEN &&
         format.parity != NX_UART_PARITY_ODD) ||
        (format.stop != NX_UART_STOP_1 && format.stop != NX_UART_STOP_2) ||
        (format.flow != NX_UART_FLOW_NONE && format.flow != NX_UART_FLOW_RTS &&
         format.flow != NX_UART_FLOW_CTS &&
         format.flow != NX_UART_FLOW_RTS_CTS) ||
        format.data_bits < 7U || format.data_bits > 9U) {
        return NX_ERROR_INVALID;
    }
    if ((format.data_bits == 7U && format.parity == NX_UART_PARITY_NONE) ||
        (format.data_bits == 9U && format.parity != NX_UART_PARITY_NONE)) {
        return NX_ERROR_UNSUPPORTED;
    }
    return NX_SUCCESS;
}

/** \brief Bounds precede arithmetic and mutation of unborrowed metadata. */
nx_result_t nx_uart_word_tx_prepare(nx_uart_word_tx_request_t* request,
                                    const uint16_t* data, size_t words,
                                    nx_time_us_t deadline) {
    if (request == NULL || data == NULL || words == 0U ||
        words > SIZE_MAX / sizeof(*data) ||
        (uintptr_t)data % _Alignof(uint16_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    size_t bytes = words * sizeof(*data);
    uintptr_t payload = (uintptr_t)data;
    uintptr_t descriptor = (uintptr_t)request;
    if (bytes > UINTPTR_MAX - payload ||
        sizeof(*request) > UINTPTR_MAX - descriptor ||
        (payload < descriptor + sizeof(*request) &&
         descriptor < payload + bytes)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->data = data;
        request->words = words;
    }
    return result;
}

/** \brief Invalid faces never enter provider code. */
static bool valid(const nx_uart_word_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}
/** \brief Dispatch once using the exact selected context. */
nx_result_t nx_uart_word_port_submit(const nx_uart_word_port_t* port,
                                     nx_uart_word_tx_request_t* request) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit != NULL ? port->ops->submit(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}
/** \brief Preserve an existing adapter admission. */
nx_result_t
nx_uart_word_port_start_admitted(const nx_uart_word_port_t* port,
                                 nx_uart_word_tx_request_t* request) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start_admitted != NULL
               ? port->ops->start_admitted(port->context, request)
               : NX_ERROR_UNSUPPORTED;
}
/** \brief Cancellation never changes lifetime by itself. */
nx_result_t nx_uart_word_port_cancel(const nx_uart_word_port_t* port,
                                     nx_uart_word_tx_request_t* request) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel != NULL ? port->ops->cancel(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}
/** \brief Dispatch bounded work only for a valid selected provider. */
void nx_uart_word_port_service(const nx_uart_word_port_t* port) {
    if (valid(port) && port->ops->service != NULL) {
        port->ops->service(port->context);
    }
}
/** \brief Read words; the provider checks context and aliasing. */
nx_result_t nx_uart_word_port_read(const nx_uart_word_port_t* port,
                                   uint16_t* words, size_t capacity,
                                   size_t* count) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->read != NULL
               ? port->ops->read(port->context, words, capacity, count)
               : NX_ERROR_UNSUPPORTED;
}
/** \brief Stop cannot revoke a provider or IRQ reference. */
nx_result_t nx_uart_word_port_stop(const nx_uart_word_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop != NULL ? port->ops->stop(port->context)
                                   : NX_ERROR_UNSUPPORTED;
}
