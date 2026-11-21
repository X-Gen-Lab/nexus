/**
 * \file            log_uart.c
 * \brief           Copied one-record UART logging with retained drain ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/log_uart.h"
#include "nexus/arch/arch.h"
#include <string.h>

/** \brief Caller staging cannot alias its state or a submitted source span. */
static bool overlaps(const void* first, size_t bytes, const void* second,
                     size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Diagnostics saturate rather than create a reused count identity. */
static void dropped(nx_log_uart_t* sink) {
    if (sink->dropped != UINT32_MAX) {
        ++sink->dropped;
    }
}

/** \brief Copy before admission so no logger input survives this invocation. */
static nx_result_t write_record(void* context, nx_log_level_t level,
                                const void* bytes, size_t length,
                                nx_time_us_t deadline) {
    nx_log_uart_t* sink = context;
    if (sink == NULL || (bytes == NULL && length != 0U) ||
        (unsigned)level > (unsigned)NX_LOG_ERROR) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr()) {
        return NX_ERROR_CONTEXT;
    }
    if (sink->stopped) {
        return NX_ERROR_STATE;
    }
    if (sink->pending) {
        dropped(sink);
        return NX_ERROR_BUSY;
    }
    if (length > sink->capacity) {
        dropped(sink);
        return NX_ERROR_OVERFLOW;
    }
    if (length == 0U) {
        return NX_SUCCESS;
    }
    if (overlaps(bytes, length, sink, sizeof(*sink)) ||
        overlaps(bytes, length, sink->buffer, sink->capacity)) {
        return NX_ERROR_INVALID;
    }
    memcpy(sink->buffer, bytes, length);
    nx_result_t result =
        nx_uart_tx_prepare(&sink->request, sink->buffer, length, deadline);
    if (result == NX_SUCCESS) {
        result = nx_uart_port_submit(sink->uart, &sink->request);
    }
    if (result != NX_SUCCESS) {
        dropped(sink);
        return result;
    }
    sink->pending = true;
    sink->cancel_requested = false;
    sink->last_result = NX_ERROR_BUSY;
    sink->last_transferred = 0U;
    if (sink->accepted != UINT32_MAX) {
        ++sink->accepted;
    }
    return NX_SUCCESS;
}

/** \brief One progress step never converts deadline expiry into buffer release.
 */
static nx_result_t flush_record(void* context, nx_time_us_t deadline) {
    (void)deadline;
    return nx_log_uart_service(context);
}

/** \brief Bind only live typed ownership; construction causes no UART effects.
 */
nx_result_t nx_log_uart_init(nx_log_uart_t* sink, const nx_uart_port_t* uart,
                             void* buffer, size_t capacity) {
    if (sink == NULL || uart == NULL || uart->ops == NULL ||
        uart->ops->submit == NULL || uart->ops->service == NULL ||
        buffer == NULL || capacity == 0U ||
        overlaps(sink, sizeof(*sink), buffer, capacity) ||
        overlaps(sink, sizeof(*sink), uart, sizeof(*uart)) ||
        overlaps(buffer, capacity, uart, sizeof(*uart))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr()) {
        return NX_ERROR_CONTEXT;
    }
    *sink = (nx_log_uart_t){0};
    sink->uart = uart;
    sink->buffer = buffer;
    sink->capacity = capacity;
    sink->last_result = NX_SUCCESS;
    nx_request_initialize(&sink->request.base);
    return NX_SUCCESS;
}

/** \brief Reuse Log's existing port instead of introducing another forwarding
 * API. */
nx_log_sink_port_t nx_log_uart_sink(nx_log_uart_t* sink) {
    return sink != NULL ? (nx_log_sink_port_t){sink, write_record, flush_record}
                        : (nx_log_sink_port_t){0};
}

/** \brief Acquire SETTLED before inspecting results or releasing private
 * staging. */
nx_result_t nx_log_uart_service(nx_log_uart_t* sink) {
    if (sink == NULL || sink->uart == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr()) {
        return NX_ERROR_CONTEXT;
    }
    if (!sink->pending) {
        return sink->last_result;
    }
    nx_uart_port_service(sink->uart);
    nx_result_t result;
    size_t transferred;
    if (nx_request_result(&sink->request.base, &result, &transferred) !=
        NX_SUCCESS) {
        return NX_ERROR_BUSY;
    }
    if (result == NX_SUCCESS && transferred != sink->request.length) {
        result = NX_ERROR_IO;
    }
    sink->last_result = result;
    sink->last_transferred = transferred;
    sink->pending = false;
    if (sink->completed != UINT32_MAX) {
        ++sink->completed;
    }
    return result;
}

/** \brief Close this adapter while leaving shared UART hardware ownership
 * intact. */
nx_result_t nx_log_uart_stop(nx_log_uart_t* sink) {
    if (sink == NULL || sink->uart == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr()) {
        return NX_ERROR_CONTEXT;
    }
    sink->stopped = true;
    if (!sink->pending) {
        return NX_SUCCESS;
    }
    if (!sink->cancel_requested) {
        nx_result_t result = nx_uart_port_cancel(sink->uart, &sink->request);
        if (result != NX_SUCCESS) {
            return result;
        }
        sink->cancel_requested = true;
    }
    (void)nx_log_uart_service(sink);
    return sink->pending ? NX_ERROR_BUSY : NX_SUCCESS;
}
