/**
 * \file            spi_dma_transaction.c
 * \brief           Complete segmented DMA span checks and typed dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/spi_dma_transaction.h"
#include "spi_dma_sequence.h"

/** \brief All intervals passed here have already been proved nonwrapping. */
static bool overlaps(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Validate immutable fields before request preparation writes storage.
 */
static nx_result_t validate(const nx_spi_dma_transaction_request_t* request,
                            const nx_spi_dma_segment_t* segments, size_t count,
                            uint8_t bits) {
    if (request == NULL || segments == NULL || count == 0U ||
        (uintptr_t)request % _Alignof(nx_spi_dma_transaction_request_t) != 0U ||
        sizeof(*request) > UINTPTR_MAX - (uintptr_t)request ||
        (uintptr_t)segments % _Alignof(nx_spi_dma_segment_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (count > NX_SPI_MAX_SEGMENTS || (bits != 8U && bits != 16U)) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t descriptors = count * sizeof(*segments);
    if (descriptors > UINTPTR_MAX - (uintptr_t)segments ||
        overlaps(request, sizeof(*request), segments, descriptors)) {
        return NX_ERROR_INVALID;
    }
    size_t width = bits / 8U;
    size_t total = 0U;
    for (size_t i = 0U; i < count; ++i) {
        const nx_spi_dma_segment_t* s = &segments[i];
        if (s->tx == NULL || s->rx == NULL || s->words == 0U ||
            s->words > SIZE_MAX / width || s->words > SIZE_MAX - total ||
            (s->memory_flags &
             ~(NX_SPI_MEMORY_TX_FIXED | NX_SPI_MEMORY_RX_FIXED)) != 0U ||
            (uintptr_t)s->tx % width != 0U || (uintptr_t)s->rx % width != 0U) {
            return NX_ERROR_INVALID;
        }
        total += s->words;
        size_t tx = (s->memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U
                        ? width
                        : s->words * width;
        size_t rx = (s->memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U
                        ? width
                        : s->words * width;
        if (tx > UINTPTR_MAX - (uintptr_t)s->tx ||
            rx > UINTPTR_MAX - (uintptr_t)s->rx ||
            overlaps(s->tx, tx, request, sizeof(*request)) ||
            overlaps(s->rx, rx, request, sizeof(*request)) ||
            overlaps(s->rx, rx, segments, descriptors)) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0U; j < count; ++j) {
            const nx_spi_dma_segment_t* other = &segments[j];
            if (other->tx == NULL || other->rx == NULL ||
                other->words > SIZE_MAX / width) {
                return NX_ERROR_INVALID;
            }
            size_t otx = (other->memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U
                             ? width
                             : other->words * width;
            size_t orx = (other->memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U
                             ? width
                             : other->words * width;
            if (otx > UINTPTR_MAX - (uintptr_t)other->tx ||
                orx > UINTPTR_MAX - (uintptr_t)other->rx ||
                overlaps(s->rx, rx, other->tx, otx)) {
                return NX_ERROR_INVALID;
            }
            bool shared_discard =
                s->rx == other->rx &&
                (s->memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U &&
                (other->memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U;
            if (i != j && !shared_discard &&
                overlaps(s->rx, rx, other->rx, orx)) {
                return NX_ERROR_INVALID;
            }
        }
    }
    return NX_SUCCESS;
}
/** \brief Public validation does not inspect or change request state. */
nx_result_t nx_spi_dma_transaction_validate(
    const nx_spi_dma_transaction_request_t* request) {
    if (request == NULL ||
        (uintptr_t)request % _Alignof(nx_spi_dma_transaction_request_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    return validate(request, request->segments, request->count,
                    request->word_bits);
}
/** \brief No descriptor or payload is retained by preparation. */
nx_result_t
nx_spi_dma_transaction_prepare(nx_spi_dma_transaction_request_t* request,
                               const nx_spi_dma_segment_t* segments,
                               size_t count, uint8_t word_bits,
                               nx_time_us_t deadline) {
    nx_result_t result = validate(request, segments, count, word_bits);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->segments = segments;
        request->count = count;
        request->word_bits = word_bits;
    }
    return result;
}
/** \brief Checked dispatch retains references only after successful admission.
 */
nx_result_t
nx_spi_dma_transaction_submit(const nx_spi_dma_transaction_endpoint_t* endpoint,
                              nx_spi_dma_transaction_request_t* request) {
    if (endpoint == NULL || endpoint->ops == NULL ||
        endpoint->context == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_spi_dma_transaction_validate(request);
    if (result != NX_SUCCESS) {
        return result;
    }
    return endpoint->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : endpoint->ops->submit(endpoint->context, request);
}
/** \brief Cancellation cannot revoke any request or DMA memory loan. */
nx_result_t
nx_spi_dma_transaction_cancel(const nx_spi_dma_transaction_port_t* port,
                              nx_spi_dma_transaction_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL ||
        request == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}
/** \brief Bounded task progression uses the exact selected controller state. */
void nx_spi_dma_transaction_service(const nx_spi_dma_transaction_port_t* port) {
    if (port != NULL && port->ops != NULL && port->context != NULL &&
        port->ops->service != NULL) {
        port->ops->service(port->context);
    }
}
/** \brief Stop never implies release while requests remain borrowed. */
nx_result_t
nx_spi_dma_transaction_stop(const nx_spi_dma_transaction_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}

/** \brief A broad DMA domain never authorizes destruction of owner metadata. */
nx_result_t nx_spi_dma_sequence_storage_validate(
    const nx_spi_dma_transaction_request_t* request, const void* storage,
    size_t bytes) {
    if (request == NULL || storage == NULL || bytes == 0U ||
        bytes > UINTPTR_MAX - (uintptr_t)storage || request->segments == NULL ||
        request->count == 0U || request->count > NX_SPI_MAX_SEGMENTS ||
        (request->word_bits != 8U && request->word_bits != 16U)) {
        return NX_ERROR_INVALID;
    }
    if (overlaps(storage, bytes, request, sizeof(*request)) ||
        overlaps(storage, bytes, request->segments,
                 request->count * sizeof(*request->segments))) {
        return NX_ERROR_INVALID;
    }
    size_t width = request->word_bits / 8U;
    for (size_t i = 0U; i < request->count; ++i) {
        const nx_spi_dma_segment_t* segment = &request->segments[i];
        if (segment->words > SIZE_MAX / width) {
            return NX_ERROR_INVALID;
        }
        size_t tx = (segment->memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U
                        ? width
                        : segment->words * width;
        size_t rx = (segment->memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U
                        ? width
                        : segment->words * width;
        if (tx > UINTPTR_MAX - (uintptr_t)segment->tx ||
            rx > UINTPTR_MAX - (uintptr_t)segment->rx ||
            overlaps(storage, bytes, segment->tx, tx) ||
            overlaps(storage, bytes, segment->rx, rx)) {
            return NX_ERROR_INVALID;
        }
    }
    return NX_SUCCESS;
}
