/**
 * \file            audio.h
 * \brief           Finite and continuous I2S transmit/receive DMA loans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_AUDIO_H
#define NEXUS_IO_AUDIO_H

#include "nexus/core/request.h"
#include "nexus/io/frame_stream.h"
#include "nexus/io/fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One finite block in the statically selected serial audio format.
 * \note Initialize/prepare base first. TX takes one exact immutable frame
 *       consumer loan; RX takes one FILLING loan from an idle framed stream.
 *       Full duplex requires both and equal complete frame storage lengths.
 *       Unselected directions must be NULL. Format conversion is never done.
 *       Storage is queried from the selected prepared owner with layout.
 *       The I2S provider uses one little endian halfword per 16-bit slot;
 *       I2S 24/32 bit samples use HIGH halfword then LOW halfword, in DMA
 * order. For 24 bit data the low halfword has eight trailing padding bits. This
 * is a sequence of halfwords, not native little endian uint32 PCM. SAI stores
 * each sample in one native little endian 8/16/32-bit right-aligned container,
 * with no implicit I2S halfword conversion. The I2S/left/right justified blocks
 * contain both channel slots; PCM blocks contain complete single channel
 * synchronization periods. Data length determines stored halfwords; wire
 * padding is not stored. I2S finite master RX requires at least two stored
 * halfwords. ACCEPTED borrows this request and both controls until SETTLED.
 * Only proved serializer, DMA, IRQ and address withdrawal returns TX or
 *       publishes RX. Error/cancel reports zero and aborts RX after drain.
 *       An external slave clock that stops while a word remains can prevent
 *       drain indefinitely; timeout and failed stop NEVER revoke the loan.
 */
typedef struct {
    nx_request_t base;
    nx_frame_dma_loan_t* tx;
    nx_frame_stream_t* rx;
} nx_audio_request_t;

/** \brief Continuous fixed-size blocks with a caller-owned session lifetime.
 * \note TX is a stream of READY immutable blocks; RX starts with FREE blocks.
 *       Each selected direction supplies a distinct stream. DMA borrows two
 *       blocks per direction; the next replacement must be available before
 *       the hardware returns to the old bank. Published RX and returned TX
 *       blocks are detached from every DMA address before consumer access.
 *       Unselected directions are NULL. block_bytes is a positive whole audio
 *       frame count, identical for both directions. No silence, sample repeat,
 *       conversion, allocation or hidden replenishment is provided.
 *       Initialize/prepare base first. The session and producer/consumer rights
 *       remain borrowed through acquire-observed SETTLED. Consumer loans from
 *       completed RX blocks remain valid after session stop and must be joined
 *       separately before reclaiming the stream or buffers. Stop discards the
 *       uncompleted hardware blocks but preserves previously published RX.
 *       transferred counts detached RX bytes when RX is selected, otherwise
 *       detached TX bytes; it describes a prefix even on error/cancel, never
 *       the number of physical wire frames. NX_DEADLINE_NEVER is permitted.
 */
typedef struct {
    nx_request_t base;
    nx_stream_t* tx;
    nx_stream_t* rx;
    size_t block_bytes;
} nx_audio_blocks_request_t;

/** \brief Exact rational nominal rate; external slave rate is unknown (0/0).
 * \note This is prescaler arithmetic, not a physical clock measurement.
 */
typedef struct {
    uint64_t numerator_hz;
    uint64_t denominator;
} nx_audio_rate_t;

/** \brief Byte/halfword ordering of one stored sample, not wire bit order. */
typedef enum {
    NX_AUDIO_STORAGE_LITTLE_ENDIAN,
    NX_AUDIO_STORAGE_HIGH_HALFWORD_FIRST
} nx_audio_storage_order_t;
/** \brief Exact selected storage; wire padding is never silently stored.
 * \note container_bits is the DMA storage width, not the wire slot width.
 *       sample_alignment names useful bits within that container, not address
 *       alignment. HIGH_HALFWORD_FIRST stores two little endian halfwords,
 *       the most significant one first. I2S24 therefore reports container32,
 *       MSB alignment and HIGH_HALFWORD_FIRST; SAI24 reports container32,
 *       LSB alignment and LITTLE_ENDIAN. slots is 1..16 stored slots per frame;
 *       active_slots is their wire mask. The first F4 SAI mode requires every
 *       slot active and never compacts a sparse mask. No conversion is done.
 */
typedef struct {
    nx_audio_storage_order_t storage_order;
    nx_frame_alignment_t sample_alignment;
    uint16_t active_slots;
    uint8_t sample_bits;
    uint8_t container_bits;
    uint8_t slots;
} nx_audio_layout_t;

/** \brief Allocation-free serialized owner, driven by explicit service.
 * \note Platform prepare acquires the selected source, transport and clock
 *       resources. Factory lookup never opens hardware. Start external slave
 *       clocks only after acquire-observed ACTIVE; frame synchronization must
 *       remain at the selected idle boundary during preparation. ACTIVE proves
 *       the selected provider's startup preparation, not physical timing.
 *       Caller controls external electrical timing and stop/drain clocks.
 *       IRQs latch errors/completion, with no task creation. One privileged
 *       unmasked task serializes methods and resource lifecycle; explicit
 *       service progresses operations. Late IRQs cannot settle an old loan.
 *       stop cancels and joins the current block; failed drain retains full
 *       request/loan/resource storage. Platform close prevents further
 *       admission before attempting resource release. Join this owner before
 *       Platform stop or clock changes.
 */
typedef struct {
    nx_result_t (*submit)(void* context, nx_audio_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*cancel)(void* context, nx_audio_request_t* request);
    nx_result_t (*stop)(void* context);
    nx_result_t (*rate)(void* context, nx_audio_rate_t* rate);
    /** \brief Optional continuous personality; NULL for a finite controller.
     * \note SUCCESS accepts the full session, including a drained immediate
     *       failure. Rejection retains no session or stream references.
     */
    nx_result_t (*start_blocks)(void* context,
                                nx_audio_blocks_request_t* request);
    /** \brief Optional read-only layout of the selected prepared owner. */
    nx_result_t (*layout)(void* context, nx_audio_layout_t* result);
} nx_audio_ops_t;

/** \brief Immutable controller identity; vendor Handles remain private. */
struct nx_audio_port {
    const nx_audio_ops_t* ops;
    void* context;
};
/** \brief Same immutable typed controller face for direct operation use. */
typedef nx_audio_port_t nx_audio_t;

/** \brief Admit one finite audio block without converting its stored samples.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_audio_port_submit(const nx_audio_port_t* port,
                                 nx_audio_request_t* request);
/** \brief Advance bounded completion and withdrawal; final results belong to
 * requests. \param[in] port: Stable face under the selected serialized
 * execution contract. \return Provider result, INVALID for malformed public
 * objects, or UNSUPPORTED when this selected operation is absent. \note The
 * selected owner defines context and timing. Dispatch retains no references,
 * starts no worker and never revokes a live borrow. Provider admission still
 * checks private state and payload aliases. The original argument reaches that
 * owner without a temporary or format conversion.
 */
nx_result_t nx_audio_port_service(const nx_audio_port_t* port);
/** \brief Request finite-block withdrawal without revoking any outstanding
 * loan. \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_audio_port_cancel(const nx_audio_port_t* port,
                                 nx_audio_request_t* request);
/** \brief Join finite or continuous producers; consumer RX loans remain
 * independent. \param[in] port: Stable face under the selected serialized
 * execution contract. \return Provider result, INVALID for malformed public
 * objects, or UNSUPPORTED when this selected operation is absent. \note The
 * selected owner defines context and timing. Dispatch retains no references,
 * starts no worker and never revokes a live borrow. Provider admission still
 * checks private state and payload aliases. The original argument reaches that
 * owner without a temporary or format conversion.
 */
nx_result_t nx_audio_port_stop(const nx_audio_port_t* port);
/** \brief Copy the exact nominal rate; an external unknown rate remains
 * unknown. \param[in] port: Stable face under the selected serialized execution
 * contract. \param[out] rate: Independent output, changed only as the provider
 * contract permits. \return Provider result, INVALID for malformed public
 * objects, or UNSUPPORTED when this selected operation is absent. \note The
 * selected owner defines context and timing. Dispatch retains no references,
 * starts no worker and never revokes a live borrow. Provider admission still
 * checks private state and payload aliases. The original argument reaches that
 * owner without a temporary or format conversion.
 */
nx_result_t nx_audio_port_rate(const nx_audio_port_t* port,
                               nx_audio_rate_t* rate);
/** \brief Admit a continuous caller-owned session without hidden replenishment.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_audio_port_start_blocks(const nx_audio_port_t* port,
                                       nx_audio_blocks_request_t* request);
/** \brief Copy exact storage layout without converting or borrowing payloads.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[out] result: Independent output, changed only as the
 * provider contract permits. \return Provider result, INVALID for malformed
 * public objects, or UNSUPPORTED when this selected operation is absent. \note
 * The selected owner defines context and timing. Dispatch retains no
 *       references, starts no worker and never revokes a live borrow. Provider
 *       admission still checks private state and payload aliases. The original
 *       argument reaches that owner without a temporary or format conversion.
 */
nx_result_t nx_audio_port_layout(const nx_audio_port_t* port,
                                 nx_audio_layout_t* result);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_AUDIO_H */
