/**
 * \file            framing.h
 * \brief           Bounded COBS packets with explicit caller-owned buffers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FRAMING_H
#define NEXUS_COMPONENTS_FRAMING_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Encode one COBS packet including its zero delimiter.
 * \param[in]       data: Input bytes, NULL only for zero size; not retained.
 * \param[in]       size: Input byte count.
 * \param[out]      output: Disjoint caller-owned encoded storage.
 * \param[in]       capacity: Writable output extent.
 * \param[out]      written: Distinct encoded byte count, unchanged on failure.
 * \return          SUCCESS, INVALID, OVERFLOW or EXHAUSTED; no partial output
 *                  on capacity failure. Task/IRQ pure bounded-by-size
 * operation. \note            COBS provides framing, not integrity or
 * authentication. Add the selected CRC before encoding and verify after
 * decoding.
 */
nx_result_t nx_cobs_encode(const uint8_t* data, size_t size, uint8_t* output,
                           size_t capacity, size_t* written);
/**
 * \brief           Decode one COBS packet without its zero delimiter.
 * \param[in]       data: Encoded input, nonzero size, readable until return.
 * \param[in]       size: Bytes excluding the delimiter.
 * \param[out]      output: Disjoint buffer or exactly data for in-place decode.
 * \param[in]       capacity: Writable output extent.
 * \param[out]      written: Distinct decoded byte count, unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED; malformed/capacity checks
 *                  precede output mutation. No allocation, time or I/O.
 */
nx_result_t nx_cobs_decode(const uint8_t* data, size_t size, uint8_t* output,
                           size_t capacity, size_t* written);
/** \brief Serialized stream accumulator; initialize once, then keep private. */
typedef struct {
    uint8_t* storage;
    size_t capacity;
    size_t size;
    bool ready;
    bool discarding;
} nx_frame_decoder_t;
/**
 * \brief           Initialize a bounded COBS receive accumulator.
 * \param[out]      decoder: Unused or quiesced metadata.
 * \param[in,out]   storage: Nonempty disjoint buffer, retained until join.
 * \param[in]       capacity: Maximum encoded packet bytes, without delimiter.
 * \return          SUCCESS or INVALID; no I/O or implicit queue/worker.
 */
nx_result_t nx_frame_decoder_init(nx_frame_decoder_t* decoder, uint8_t* storage,
                                  size_t capacity);
/**
 * \brief           Feed one byte in a serialized execution context.
 * \param[in,out]   decoder: Initialized caller-owned state.
 * \param[in]       byte: Wire byte; zero terminates a packet.
 * \return          SUCCESS for a complete decoded packet; BUSY for partial
 *                  packet or a still-borrowed previous packet; OVERFLOW closes
 *                  admission until next zero; INVALID rejects malformed packet.
 * \note            No time/I/O/retained input; BUSY while ready consumes no
 * byte.
 */
nx_result_t nx_frame_decoder_feed(nx_frame_decoder_t* decoder, uint8_t byte);
/**
 * \brief           Borrow the current decoded packet until release.
 * \param[in]       decoder: Initialized serialized decoder.
 * \param[out]      data: Distinct pointer to retained caller storage.
 * \param[out]      size: Distinct payload byte count; empty packet is valid.
 * \return          SUCCESS, EMPTY or INVALID; failure leaves outputs unchanged.
 */
nx_result_t nx_frame_decoder_peek(const nx_frame_decoder_t* decoder,
                                  const uint8_t** data, size_t* size);
/**
 * \brief           Release the decoded packet after all borrowers have joined.
 * \param[in,out]   decoder: Initialized serialized decoder.
 * \return          SUCCESS, EMPTY or INVALID; no hardware effects.
 */
nx_result_t nx_frame_decoder_release(nx_frame_decoder_t* decoder);
#ifdef __cplusplus
}
#endif
#endif
