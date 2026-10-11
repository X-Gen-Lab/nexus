/**
 * \file            stream.h
 *
 * \brief           Bounded caller-owned blocks with explicit producer/consumer
 *                  loans
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_STREAM_H
#define NEXUS_IO_STREAM_H

#include "nexus/core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_STREAM_BOUNDARY_IDLE    1U
#define NX_STREAM_BOUNDARY_TRIGGER 2U
#define NX_STREAM_BOUNDARY_LOSS    4U
#define NX_STREAM_BOUNDARY_ERROR   8U

typedef enum {
    NX_STREAM_SLOT_FREE,
    NX_STREAM_SLOT_FILLING,
    NX_STREAM_SLOT_READY,
    NX_STREAM_SLOT_BORROWED
} nx_stream_slot_state_t;

/**
 * \brief           Caller-owned block metadata; initialize data and capacity
 *                  only.
 *
 * \note            Storage remains live through producer and consumer
 *                  quiescence.
 */
typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t length;
    uint64_t epoch;
    uint32_t flags;
    uint32_t lost_blocks;
    nx_stream_slot_state_t state;
} nx_stream_slot_t;

/**
 * \brief           Caller-owned stream control, initialized before publication.
 *
 * \note            outstanding counts nonfree blocks, including filling,
 *                  ready and borrowed states. Only stream operations mutate it.
 */
typedef struct {
    nx_stream_slot_t* slots;
    size_t count;
    size_t producer;
    size_t consumer;
    size_t outstanding;
    uint64_t epoch;
    uint32_t losses;
    bool stopping;
} nx_stream_t;

/** \brief Mutable producer loan; hardware must stop before abort or publish. */
typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t slot;
    uint64_t epoch;
} nx_stream_fill_t;

/** \brief Immutable consumer loan, valid until matching release succeeds. */
typedef struct {
    const uint8_t* data;
    size_t length;
    size_t slot;
    uint64_t epoch;
    uint32_t flags;
    uint32_t lost_blocks;
} nx_stream_block_t;

/**
 * \brief           Initialize an unborrowed stream and exact supplied block
 *                  array.
 *
 * \param[out]      stream: Fresh storage, not visible to any producer/consumer.
 *
 * \param[in,out]   slots: Buffers distinct from all control and slot metadata.
 *
 * \param[in]       count: Positive exact block count, not a maximum pool size.
 *
 * \return          Success or INVALID; never allocates or retains bad inputs.
 *
 * \note            Task-only cold assembly. This helper does not validate DMA
 *                  domains; the hardware provider must validate every block.
 */
nx_result_t nx_stream_initialize(nx_stream_t* stream, nx_stream_slot_t* slots,
                                 size_t count);

/**
 * \brief           Reserve the next free block for one serialized producer.
 *
 * \param[in,out]   stream: Live initialized stream with one producer.
 *
 * \param[out]      fill: Producer loan valid until publish or proved abort.
 *
 * \return          Success, OVERFLOW without overwrite, STATE while stopping or
 *                  EXHAUSTED when the monotonic loan identity cannot advance.
 *
 * \note            Bounded task/IRQ metadata operation; payload writes occur
 *                  outside the short interrupt mask, within the returned loan.
 */
nx_result_t nx_stream_reserve(nx_stream_t* stream, nx_stream_fill_t* fill);
/**
 * \brief           Query the next physical slot without creating a loan or
 *                  loss.
 *
 * \param[in]       stream: Live initialized stream with one serialized
 *                  producer.
 *
 * \return          True only when admission is open and the next slot is free.
 *
 * \note            Task/IRQ bounded hint. A producer still calls reserve before
 *                  accessing payload; another context must not race admission.
 */
bool nx_stream_can_reserve(const nx_stream_t* stream);
/**
 * \brief           Latch explicitly observed dropped producer boundaries.
 *
 * \param[in,out]   stream: Live stream controlled by the serialized producer.
 *
 * \param[in]       count: Positive lost block/boundary count, never guessed
 *                  bytes.
 *
 * \note            Task/IRQ bounded. Saturates at UINT32_MAX; the next
 *                  published block carries LOSS independently of its useful
 *                  payload length.
 */
void nx_stream_note_loss(nx_stream_t* stream, uint32_t count);

/**
 * \brief           Publish a complete block after all hardware writes detached.
 *
 * \param[in,out]   stream: Same serialized producer as reserve.
 *
 * \param[in]       fill: Current producer loan, not retained after publication.
 *
 * \param[in]       length: Positive produced bytes within the reserved
 *                  capacity.
 *
 * \param[in]       flags: IDLE/TRIGGER boundaries observed by the provider.
 *
 * \return          Success, INVALID or STATE; failure retains producer
 *                  ownership.
 *
 * \note            Task/IRQ bounded. A DMA half/full flag is insufficient if
 *                  the engine can still write the published block.
 *                  Consumer-visible block bytes stay immutable until the
 *                  consumer releases them.
 */
nx_result_t nx_stream_publish(nx_stream_t* stream, const nx_stream_fill_t* fill,
                              size_t length, uint32_t flags);

/**
 * \brief           Acquire the next ready block for one serialized consumer.
 *
 * \param[in,out]   stream: Live stream; consumer may drain during stop.
 *
 * \param[out]      block: Immutable consumer loan with exact incarnation token.
 *
 * \return          Success, EMPTY or INVALID; empty creates no loan.
 *
 * \note            Task/IRQ bounded metadata; consumer processing happens
 *                  outside the mask. Release may occur out of order without
 *                  overwrite.
 */
nx_result_t nx_stream_acquire(nx_stream_t* stream, nx_stream_block_t* block);

/**
 * \brief           Release only the matching consumer incarnation after final
 *                  use.
 *
 * \param[in,out]   stream: Same consumer owner; no remaining observers of
 *                  block.
 *
 * \param[in]       block: Exact acquired token, never a reconstructed pointer.
 *
 * \return          Success, STATE for stale/double release or INVALID.
 *
 * \note            Task/IRQ bounded; release is the consumer's final block
 *                  access.
 */
nx_result_t nx_stream_release(nx_stream_t* stream,
                              const nx_stream_block_t* block);

/**
 * \brief           Abort a producer loan only after memory write sources
 *                  detach.
 *
 * \param[in,out]   stream: Same producer owner as reserve.
 *
 * \param[in]       fill: Current exact loan.
 *
 * \param[in]       memory_quiesced: True only after provider proves DMA/IRQ
 *                  stopped.
 *
 * \return          Success, BUSY without proven drain, STATE for a stale token.
 *
 * \note            Cancellation/deadline alone never establishes quiescence.
 */
nx_result_t nx_stream_abort(nx_stream_t* stream, const nx_stream_fill_t* fill,
                            bool memory_quiesced);

/**
 * \brief           Close producer admission while retaining every outstanding
 *                  loan.
 *
 * \param[in,out]   stream: Live control kept alive until final successful stop.
 *
 * \return          Success only when all blocks are free, BUSY until producers
 *                  detach and consumers drain/release, INVALID for bad control.
 *
 * \note            Task/IRQ constant metadata work independent of block count.
 *                  This operation does not stop hardware or revoke loans.
 */
nx_result_t nx_stream_stop(nx_stream_t* stream);

#ifdef __cplusplus
}
#endif

#endif
