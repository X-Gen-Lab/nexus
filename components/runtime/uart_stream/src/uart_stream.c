/**
 * \file            uart_stream.c
 * \brief           Stable copied TX survives cancellation and failed drains
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/uart_stream.h"
#include <string.h>

/** \brief Reject known aliases and wrapping spans before any public output. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (an != 0U && bn != 0U && x < y + bn && y < x + an);
}
/** \brief Keep the first observed framing/transport failure until joined stop.
 */
static nx_result_t failed(nx_uart_stream_t* stream, nx_result_t result) {
    if (stream->failure == NX_SUCCESS) {
        stream->failure = result;
    }
    return stream->failure;
}
/** \brief Read clock history without treating numeric continuity as an epoch.
 */
static nx_result_t clock_check(nx_uart_stream_t* stream) {
    nx_time_us_t now = stream->clock.read(stream->clock.context);
    if (now == NX_DEADLINE_NEVER || now < stream->last_time) {
        return failed(stream, NX_ERROR_STATE);
    }
    stream->last_time = now;
    return NX_SUCCESS;
}
/** \brief Only acquire-observed SETTLED permits private buffer reuse. */
static nx_result_t observe(nx_uart_stream_t* stream) {
    if (!stream->borrowed) {
        return stream->failure;
    }
    nx_request_state_t state = nx_request_state(&stream->request.base);
    if (state != NX_REQUEST_SETTLED) {
        if (state == NX_REQUEST_QUARANTINED) {
            return failed(stream, NX_ERROR_IO);
        }
        if (state != NX_REQUEST_ACTIVE && state != NX_REQUEST_QUEUED &&
            state != NX_REQUEST_DRAINING) {
            return failed(stream, NX_ERROR_STATE);
        }
        return stream->failure == NX_SUCCESS ? NX_ERROR_BUSY : stream->failure;
    }
    nx_result_t terminal;
    size_t count;
    nx_result_t result =
        nx_request_result(&stream->request.base, &terminal, &count);
    if (result != NX_SUCCESS) {
        return failed(stream, result);
    }
    stream->borrowed = false;
    if (terminal == NX_SUCCESS && count != stream->request.length) {
        return failed(stream, NX_ERROR_IO);
    }
    if (terminal != NX_SUCCESS &&
        !(stream->closing && terminal == NX_ERROR_CANCELLED)) {
        return failed(stream, terminal);
    }
    return stream->failure;
}
/** \brief Keep public buffers/count outside all adapter-owned memory. */
static bool transfer_valid(const nx_uart_stream_t* stream, const void* data,
                           size_t bytes, size_t* count, nx_time_us_t deadline) {
    return stream != NULL && stream->initialized && count != NULL &&
           deadline != NX_DEADLINE_NEVER &&
           !overlap(stream, sizeof(*stream), count, sizeof(*count)) &&
           !overlap(stream->transmit, stream->capacity, count,
                    sizeof(*count)) &&
           (bytes == 0U ||
            (data != NULL && !overlap(stream, sizeof(*stream), data, bytes) &&
             !overlap(stream->transmit, stream->capacity, data, bytes) &&
             !overlap(data, bytes, count, sizeof(*count))));
}
/** \brief A successful local copy creates exactly one stable inner loan. */
static nx_result_t write(void* context, const uint8_t* data, size_t bytes,
                         size_t* accepted, nx_time_us_t deadline) {
    nx_uart_stream_t* stream = context;
    if (!transfer_valid(stream, data, bytes, accepted, deadline)) {
        return NX_ERROR_INVALID;
    }
    *accepted = 0U;
    if (bytes == 0U) {
        return NX_SUCCESS;
    }
    if (stream->active) {
        return NX_ERROR_BUSY;
    }
    if (stream->closing || stream->closed) {
        return NX_ERROR_STATE;
    }
    stream->active = true;
    nx_result_t result = observe(stream);
    if (result == NX_SUCCESS) {
        result = clock_check(stream);
    }
    if (result == NX_SUCCESS && stream->last_time >= deadline) {
        result = NX_ERROR_TIMEOUT;
    }
    if (result == NX_SUCCESS) {
        size_t count = bytes < stream->capacity ? bytes : stream->capacity;
        memcpy(stream->transmit, data, count);
        result = nx_uart_tx_prepare(&stream->request, stream->transmit, count,
                                    deadline);
        if (result == NX_SUCCESS) {
            result = nx_uart_port_submit(stream->uart, &stream->request);
        }
        if (result == NX_SUCCESS) {
            stream->borrowed = true;
            *accepted = count;
        }
    }
    stream->active = false;
    return result;
}
/** \brief UART loss is a framing error; an empty live queue is not EOF. */
static nx_result_t read(void* context, uint8_t* data, size_t bytes,
                        size_t* received, nx_time_us_t deadline) {
    nx_uart_stream_t* stream = context;
    if (!transfer_valid(stream, data, bytes, received, deadline)) {
        return NX_ERROR_INVALID;
    }
    *received = 0U;
    if (bytes == 0U) {
        return NX_SUCCESS;
    }
    if (stream->active) {
        return NX_ERROR_BUSY;
    }
    if (stream->closing || stream->closed) {
        return NX_ERROR_STATE;
    }
    stream->active = true;
    nx_result_t result = observe(stream);
    if (result == NX_ERROR_BUSY) {
        result = NX_SUCCESS;
    }
    if (result == NX_SUCCESS) {
        result = clock_check(stream);
    }
    if (result == NX_SUCCESS && stream->last_time >= deadline) {
        result = NX_ERROR_TIMEOUT;
    }
    if (result == NX_SUCCESS) {
        size_t count = 0U;
        result = nx_uart_port_read_bytes(stream->uart, data, bytes, &count);
        if ((result == NX_SUCCESS || result == NX_ERROR_EMPTY) && count == 0U) {
            result = NX_ERROR_BUSY;
        } else if (result == NX_SUCCESS && count <= bytes) {
            *received = count;
        } else {
            result =
                failed(stream, result == NX_SUCCESS ? NX_ERROR_IO : result);
        }
    }
    stream->active = false;
    return result;
}
static const nx_byte_stream_ops_t operations = {write, read};

nx_result_t nx_uart_stream_init(nx_uart_stream_t* stream,
                                const nx_uart_port_t* uart, nx_clock_t clock,
                                uint8_t* transmit, size_t capacity) {
    if (stream == NULL || uart == NULL || uart->ops == NULL ||
        uart->context == NULL || clock.read == NULL || transmit == NULL ||
        capacity == 0U) {
        return NX_ERROR_INVALID;
    }
    if (uart->ops->submit == NULL || uart->ops->cancel == NULL ||
        uart->ops->service == NULL || uart->ops->read_bytes == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    const void* spans[] = {stream, uart, uart->ops, transmit};
    const size_t sizes[] = {sizeof(*stream), sizeof(*uart), sizeof(*uart->ops),
                            capacity};
    for (size_t i = 0U; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        for (size_t j = 0U; j < i; ++j) {
            if (overlap(spans[i], sizes[i], spans[j], sizes[j])) {
                return NX_ERROR_INVALID;
            }
        }
    }
    if (stream->initialized && (!stream->closed || stream->borrowed)) {
        return NX_ERROR_STATE;
    }
    nx_time_us_t now = clock.read(clock.context);
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    *stream = (nx_uart_stream_t){.uart = uart,
                                 .clock = clock,
                                 .face = {&operations, stream},
                                 .transmit = transmit,
                                 .capacity = capacity,
                                 .last_time = now,
                                 .initialized = true};
    nx_request_initialize(&stream->request.base);
    return NX_SUCCESS;
}

const nx_byte_stream_t* nx_uart_stream_face(const nx_uart_stream_t* stream) {
    return stream != NULL && stream->initialized ? &stream->face : NULL;
}

nx_result_t nx_uart_stream_service(nx_uart_stream_t* stream) {
    if (stream == NULL || !stream->initialized) {
        return NX_ERROR_INVALID;
    }
    if (stream->active) {
        return NX_ERROR_BUSY;
    }
    if (stream->closed) {
        return NX_ERROR_STATE;
    }
    stream->active = true;
    nx_uart_port_service(stream->uart);
    (void)clock_check(stream);
    nx_result_t result = observe(stream);
    stream->active = false;
    return result;
}

nx_result_t nx_uart_stream_stop(nx_uart_stream_t* stream) {
    if (stream == NULL || !stream->initialized) {
        return NX_ERROR_INVALID;
    }
    if (stream->active) {
        return NX_ERROR_BUSY;
    }
    if (stream->closed) {
        return NX_SUCCESS;
    }
    stream->closing = true;
    stream->active = true;
    (void)observe(stream);
    nx_result_t result = NX_SUCCESS;
    if (stream->borrowed) {
        result = nx_uart_port_cancel(stream->uart, &stream->request);
        nx_uart_port_service(stream->uart);
        (void)observe(stream);
    }
    if (stream->borrowed) {
        if (result == NX_SUCCESS) {
            result = NX_ERROR_BUSY;
        }
    } else {
        stream->closed = true;
        result = NX_SUCCESS;
    }
    stream->active = false;
    return result;
}
