/**
 * \file            spi_transaction.h
 * \brief           Optional multi-segment whole-CS 8/16-bit polling SPI
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_SPI_TRANSACTION_H
#define NEXUS_IO_SPI_TRANSACTION_H

#include "nexus/core/time.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Bounded descriptor count; no hidden concatenation buffer exists. */
#define NX_SPI_MAX_SEGMENTS 16U

/**
 * \brief           One phase within a single CS interval.
 * \note            Words have the transaction's common width of 8 or 16 bits.
 *                  NULL TX repeats dummy; NULL RX discards. Both may be NULL
 *                  for clock-only words. Non-NULL spans contain words elements;
 *                  16-bit spans are uint16_t-aligned. Descriptor and TX storage
 *                  stay immutable until return. RX never overlaps descriptors,
 *                  TX storage, other RX spans or the completion output.
 * Ordinary byte-span SPI remains a separate unchanged interface.
 */
typedef struct {
    const void* tx;
    void* rx;
    size_t words;
    uint16_t dummy;
} nx_spi_segment_t;

/** \brief Shared immutable methods; absent transfer reports UNSUPPORTED. */
typedef struct {
    nx_result_t (*transfer)(void* context, const nx_spi_segment_t* segments,
                            size_t count, uint8_t word_bits,
                            nx_time_us_t deadline, size_t* transferred);
} nx_spi_transaction_ops_t;

/** \brief Optional face sharing the selected provider's existing bus state. */
typedef struct {
    const nx_spi_transaction_ops_t* ops;
    void* context;
} nx_spi_transaction_endpoint_t;

/**
 * \brief           Validate every descriptor and span without hardware effects.
 * \param[in]       segments: Aligned immutable array, retained by the caller.
 * \param[in]       count: Positive descriptor count, at most MAX_SEGMENTS.
 * \param[in]       word_bits: Exactly 8 or 16; fixed for the whole CS interval.
 * \return          SUCCESS, INVALID for malformed/overlapping/overflowing
 * spans, or UNSUPPORTED for excess descriptor count or word width. \note No
 * memory is borrowed, copied, allocated or modified. NULL data pointers are
 * legal dummy/discard operations. This does not validate provider-specific
 * controller, rate or limits.
 */
nx_result_t nx_spi_transaction_validate(const nx_spi_segment_t* segments,
                                        size_t count, uint8_t word_bits);

/**
 * \brief           Execute all segments under exactly one asserted CS interval.
 * \param[in]       endpoint: Optional enhanced face with stable private state.
 * \param[in]       segments: Caller-owned descriptors and their data spans.
 * \param[in]       count: Number of phases, all validated before hardware use.
 * \param[in]       word_bits: Common hardware frame width, 8 or 16 bits.
 * \param[in]       deadline: Absolute monotonic deadline for all phases.
 * \param[out]      transferred: Complete received words, never bytes; separate
 *                  writable size_t storage, reset before provider execution.
 * \return          SUCCESS or
 * INVALID/UNSUPPORTED/CONTEXT/STATE/BUSY/TIMEOUT/IO. \note            Task-only
 * single executor; polling is CPU-active. Return releases descriptor/payload
 * borrows after wire drain or provider reset and CS release. Deadline cannot
 * preempt an in-flight word. Errors never replay prior segments. No allocation,
 * OS lock, hidden scratch, width changes or CS toggles occur between phases.
 * This is synchronous: existing finite DMA request and cancellation contracts
 * are unchanged.
 */
nx_result_t
nx_spi_transaction_transfer(const nx_spi_transaction_endpoint_t* endpoint,
                            const nx_spi_segment_t* segments, size_t count,
                            uint8_t word_bits, nx_time_us_t deadline,
                            size_t* transferred);

#ifdef __cplusplus
}
#endif
#endif
