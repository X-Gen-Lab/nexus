/**
 * \file            spi_transaction.c
 * \brief           Checked optional segmented SPI dispatch and span contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/spi_transaction.h"

/** \brief Query only spans already proved not to wrap the address domain. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return a != NULL && b != NULL && (uintptr_t)a < (uintptr_t)b + bn &&
           (uintptr_t)b < (uintptr_t)a + an;
}

/** \brief Check physical bytes before typed access or interval arithmetic. */
static bool valid_span(const void* address, size_t length, size_t alignment) {
    return address == NULL || ((uintptr_t)address % alignment == 0U &&
                               length <= UINTPTR_MAX - (uintptr_t)address);
}

/** \brief All phases are checked before the first hardware or output write. */
nx_result_t nx_spi_transaction_validate(const nx_spi_segment_t* segments,
                                        size_t count, uint8_t word_bits) {
    if (segments == NULL || count == 0U ||
        (uintptr_t)segments % _Alignof(nx_spi_segment_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (count > NX_SPI_MAX_SEGMENTS || (word_bits != 8U && word_bits != 16U)) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t descriptors = count * sizeof(*segments);
    if (!valid_span(segments, descriptors, _Alignof(nx_spi_segment_t))) {
        return NX_ERROR_INVALID;
    }
    size_t width = word_bits / 8U;
    size_t total = 0U;
    for (size_t i = 0U; i < count; ++i) {
        const nx_spi_segment_t* item = &segments[i];
        if (item->words == 0U || item->words > SIZE_MAX / width ||
            item->words > SIZE_MAX - total ||
            (word_bits == 8U && item->dummy > UINT8_MAX)) {
            return NX_ERROR_INVALID;
        }
        size_t bytes = item->words * width;
        if (!valid_span(item->tx, bytes, width) ||
            !valid_span(item->rx, bytes, width) ||
            overlap(item->rx, bytes, segments, descriptors)) {
            return NX_ERROR_INVALID;
        }
        total += item->words;
        /* RX cannot change any future immutable TX or another RX phase. */
        for (size_t j = 0U; j < count; ++j) {
            const nx_spi_segment_t* other = &segments[j];
            if (other->words > SIZE_MAX / width) {
                return NX_ERROR_INVALID;
            }
            size_t other_bytes = other->words * width;
            if (!valid_span(other->tx, other_bytes, width) ||
                !valid_span(other->rx, other_bytes, width) ||
                overlap(item->rx, bytes, other->tx, other_bytes) ||
                (i != j && overlap(item->rx, bytes, other->rx, other_bytes))) {
                return NX_ERROR_INVALID;
            }
        }
    }
    return NX_SUCCESS;
}

/** \brief Reject output aliases before resetting the caller's count. */
static bool output_safe(const nx_spi_segment_t* segments, size_t count,
                        uint8_t bits, const size_t* output) {
    if (output == NULL ||
        !valid_span(output, sizeof(*output), _Alignof(size_t)) ||
        segments == NULL || count == 0U || count > NX_SPI_MAX_SEGMENTS ||
        (bits != 8U && bits != 16U) ||
        !valid_span(segments, count * sizeof(*segments),
                    _Alignof(nx_spi_segment_t)) ||
        overlap(output, sizeof(*output), segments, count * sizeof(*segments))) {
        return false;
    }
    size_t width = bits / 8U;
    for (size_t i = 0U; i < count; ++i) {
        if (segments[i].words > SIZE_MAX / width) {
            return false;
        }
        size_t bytes = segments[i].words * width;
        if (!valid_span(segments[i].tx, bytes, width) ||
            !valid_span(segments[i].rx, bytes, width) ||
            overlap(output, sizeof(*output), segments[i].tx, bytes) ||
            overlap(output, sizeof(*output), segments[i].rx, bytes)) {
            return false;
        }
    }
    return true;
}

/** \brief Independent methods preserve the legacy face/table/state footprint.
 */
nx_result_t
nx_spi_transaction_transfer(const nx_spi_transaction_endpoint_t* endpoint,
                            const nx_spi_segment_t* segments, size_t count,
                            uint8_t word_bits, nx_time_us_t deadline,
                            size_t* transferred) {
    if (endpoint == NULL || endpoint->ops == NULL ||
        endpoint->context == NULL ||
        !output_safe(segments, count, word_bits, transferred) ||
        overlap(transferred, sizeof(*transferred), endpoint,
                sizeof(*endpoint)) ||
        overlap(transferred, sizeof(*transferred), endpoint->ops,
                sizeof(*endpoint->ops))) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0U;
    nx_result_t result =
        nx_spi_transaction_validate(segments, count, word_bits);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (size_t i = 0U; i < count; ++i) {
        size_t bytes = segments[i].words * (word_bits / 8U);
        if (overlap(segments[i].rx, bytes, endpoint, sizeof(*endpoint)) ||
            overlap(segments[i].rx, bytes, endpoint->ops,
                    sizeof(*endpoint->ops))) {
            return NX_ERROR_INVALID;
        }
    }
    return endpoint->ops->transfer == NULL
               ? NX_ERROR_UNSUPPORTED
               : endpoint->ops->transfer(endpoint->context, segments, count,
                                         word_bits, deadline, transferred);
}
