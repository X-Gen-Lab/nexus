/**
 * \file            frame_stream.h
 * \brief           Complete framed blocks and strided DMA consumer loans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_FRAME_STREAM_H
#define NEXUS_IO_FRAME_STREAM_H

#include "nexus/io/dma.h"
#include "nexus/io/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_FRAME_MAX_PLANES 4U

/** \brief Memory representations; these values never configure a wire. */
typedef enum {
    NX_FRAME_PCM_SIGNED,
    NX_FRAME_PCM_UNSIGNED,
    NX_FRAME_PDM_BITS
} nx_frame_sample_t;
/** \brief Serial framing whose complete units determine block geometry. */
typedef enum {
    NX_FRAME_AUDIO_I2S,
    NX_FRAME_AUDIO_SAI,
    NX_FRAME_AUDIO_PDM
} nx_frame_audio_kind_t;
/** \brief Position of useful sample bits in each stored slot container. */
typedef enum { NX_FRAME_SAMPLE_LSB, NX_FRAME_SAMPLE_MSB } nx_frame_alignment_t;
/**
 * \brief           Immutable audio memory format, independent of codec policy.
 *
 * \note            PCM frames store every slot, including inactive slots, in
 *                  ascending slot order; slot_bits is the container width.
 *                  I2S has two 16/32-bit slots; SAI has 1..32 8/16/32-bit
 * slots. PDM has 1..8 byte-interleaved lanes; each byte holds eight successive
 * bits of one lane, so frames must divide by eight. active_slots selects valid
 * lanes/slots without compacting storage. Sample byte order is explicitly
 * declared.
 */
typedef struct {
    nx_frame_audio_kind_t kind;
    nx_frame_sample_t sample;
    nx_frame_alignment_t alignment;
    size_t frames;
    uint32_t active_slots;
    uint8_t slots;
    uint8_t sample_bits;
    uint8_t slot_bits;
    bool big_endian;
} nx_frame_audio_format_t;
/**
 * \brief           One plane; the last row needs no trailing stride padding.
 *
 * \note            row_bytes/stride are bytes, rows is a complete line count.
 *                  A plane occupies (rows - 1) * stride + row_bytes bytes.
 */
typedef struct {
    size_t offset;
    size_t row_bytes;
    size_t stride;
    size_t rows;
} nx_frame_plane_t;
/**
 * \brief           Immutable frame geometry copied into a bound stream.
 *
 * \note            Plane intervals, including inter-row padding, cannot
 * overlap. bytes is the exact maximum plane end, not summed row bytes. Audio
 * formats are separate immutable caller metadata; this layout represents their
 * checked complete-unit storage.
 */
typedef struct {
    nx_frame_plane_t planes[NX_FRAME_MAX_PLANES];
    size_t plane_count;
    size_t bytes;
} nx_frame_layout_t;
/**
 * \brief           Caller-owned binding; the existing stream owns every queue.
 *
 * \note            Initialize before publication. One serialized producer and
 *                  consumer use framed publish/acquire exclusively until stop.
 *                  Hardware, queue capacity and synchronization remain
 * explicit.
 */
typedef struct {
    nx_stream_t* stream;
    nx_frame_layout_t layout;
    uint32_t underruns;
} nx_frame_stream_t;
/**
 * \brief           Immutable consumer block retained through final DMA access.
 *
 * \note            Zero-initialize before first acquisition. Keep this exact
 *                  control live and exclusively owned until release; copied
 *                  tokens never authorize earlier reuse or another instance.
 */
typedef struct nx_frame_dma_loan {
    nx_frame_stream_t* owner;
    nx_stream_block_t block;
    const struct nx_frame_dma_loan* identity;
} nx_frame_dma_loan_t;
/** \brief Checked 2D byte rectangle for an explicit DMA adapter. */
typedef struct {
    const uint8_t* data;
    size_t row_bytes;
    size_t stride;
    size_t rows;
    size_t span_bytes;
} nx_frame_dma_view_t;

/**
 * \brief           Compute complete audio storage without touching payload.
 * \param[in]       format: Explicit framing, sample and stored slot geometry.
 * \param[out]      layout: Separate fresh output, unchanged on rejection.
 *
 * \return          SUCCESS, INVALID for incompatible framing or EXHAUSTED on
 *                  arithmetic overflow; never retains an input or allocates.
 *
 * \note            Task/IRQ bounded pure calculation. No format conversion,
 *                  serializer configuration or codec negotiation is performed.
 */
nx_result_t nx_frame_layout_audio(const nx_frame_audio_format_t* format,
                                  nx_frame_layout_t* layout);
/**
 * \brief           Check and copy planar/strided frame geometry.
 * \param[in]       planes: 1..NX_FRAME_MAX_PLANES immutable plane descriptions.
 * \param[in]       count: Exact plane count, not a runtime pool maximum.
 * \param[out]      layout: Fresh output, unchanged on every rejection.
 *
 * \return          SUCCESS, INVALID for empty/overlapping/short-stride planes,
 *                  or EXHAUSTED on byte arithmetic overflow.
 *
 * \note            Bounded pure calculation. Pixel/color meanings remain with
 *                  the selected camera/display/DMA adapter.
 */
nx_result_t nx_frame_layout_planar(const nx_frame_plane_t* planes, size_t count,
                                   nx_frame_layout_t* layout);
/**
 * \brief           Cold-bind a complete-frame layout to an existing queue.
 * \param[out]      framed: Fresh control disjoint from queue and payloads.
 * \param[in,out]   stream: Initialized unborrowed queue with all slots free.
 * \param[in]       layout: Checked complete-frame geometry, copied on success.
 *
 * \return          SUCCESS, INVALID for aliases/short buffers/bad layout,
 *                  BUSY for outstanding blocks or STATE while stopping.
 *
 * \note            Task-only cold assembly, O(n squared) storage checks.
 *                  No hardware or DMA-domain admission is inferred. Keep all
 *                  queue/control/payload storage live through final stop.
 */
nx_result_t nx_frame_stream_initialize(nx_frame_stream_t* framed,
                                       nx_stream_t* stream,
                                       const nx_frame_layout_t* layout);
/**
 * \brief           Publish one complete frame after final producer access.
 * \param[in,out]   framed: Exclusive serialized producer binding.
 * \param[in]       fill: Existing stream producer loan, detached from all DMA.
 * \param[in]       flags: Existing observed stream boundary flags.
 *
 * \return          Existing stream result; rejection retains the fill loan.
 *
 * \note            Bounded task/IRQ metadata. Reserve/prepost/abort still use
 *                  the existing stream; publication always has exact frame
 * size.
 */
nx_result_t nx_frame_stream_publish(nx_frame_stream_t* framed,
                                    const nx_stream_fill_t* fill,
                                    uint32_t flags);
/**
 * \brief           Publish one complete frame from the existing posted FIFO.
 * \param[in,out]   framed: Same producer binding and complete-frame layout.
 * \param[in,out]   producer: Existing posted producer bound to this stream.
 * \param[in]       fill: Oldest pending loan, absent from all hardware targets.
 * \param[in]       flags: Observed existing stream boundary flags.
 *
 * \return          Existing posted-publication result, INVALID for foreign
 * pool.
 *
 * \note            Bounded task/IRQ metadata; preserves pending and FIFO
 * counts.
 */
nx_result_t nx_frame_stream_post_publish(nx_frame_stream_t* framed,
                                         nx_stream_producer_t* producer,
                                         const nx_stream_fill_t* fill,
                                         uint32_t flags);
/**
 * \brief           Acquire the next complete immutable DMA consumer loan.
 *
 * \param[in,out]   framed: Same serialized consumer, one call per observed
 *                  output boundary when using underrun accounting.
 *
 * \param[in,out]   loan: Zero or previously released control, unchanged on
 * error.
 *
 * \return          SUCCESS, EMPTY with saturating underrun count, BUSY for a
 *                  live loan, INVALID for output alias or STATE for an
 * incomplete queued block.
 *
 * \note            Task/IRQ; output disjointness scans the fixed pool in O(n)
 *                  before the O(1) guarded queue operation. Output cannot alias
 *                  queue controls or any full-capacity pool payload. EMPTY
 *                  creates no loan and never repeats a previous frame. Caller
 *                  owns silence/blanking/stop policy. Do not use retries as
 *                  additional observed boundaries.
 */
nx_result_t nx_frame_stream_acquire(nx_frame_stream_t* framed,
                                    nx_frame_dma_loan_t* loan);
/**
 * \brief           Derive a strided byte rectangle from a live frame loan.
 * \param[in]       loan: Exact live immutable DMA consumer loan.
 * \param[in]       plane: Plane index in the bound layout.
 * \param[in]       x: Byte column within a row, not a pixel count.
 * \param[in]       y: First complete row.
 * \param[in]       width: Positive bytes per selected row.
 * \param[in]       height: Positive selected row count.
 * \param[out]      view: Fresh output unchanged on failure.
 *
 * \return          SUCCESS, INVALID for out-of-plane rectangles or STATE for
 *                  stale/foreign loans; no DMA access begins here.
 *
 * \note            Task/IRQ; an O(n) immutable-pool output check precedes the
 *                  O(1) guarded view calculation. Output cannot alias the loan,
 *                  queue controls or any full-capacity pool payload. The
 *                  adapter checks pixel alignment, resource and DMA domain.
 */
nx_result_t nx_frame_dma_view(const nx_frame_dma_loan_t* loan, size_t plane,
                              size_t x, size_t y, size_t width, size_t height,
                              nx_frame_dma_view_t* view);
/**
 * \brief           Release only after the adapter proves final DMA access.
 * \param[in,out]   framed: Original owner; another instance cannot release.
 * \param[in,out]   loan: Exact acquired control, cleared only on success.
 * \param[in]       drain: Actual provider observations of engine, IRQ and
 *                  peripheral quiescence; counter completion alone is not
 * proof.
 *
 * \return          SUCCESS, BUSY while any source remains, STATE for
 * stale or foreign tokens, or INVALID for impossible drain counters. \note
 * Bounded task/IRQ metadata. Timeout/cancel is not quiescence; failed drain
 * retains the queue borrow, including during stop. This helper checks proof
 * facts and does not stop hardware.
 */
nx_result_t nx_frame_dma_release(nx_frame_stream_t* framed,
                                 nx_frame_dma_loan_t* loan,
                                 const nx_dma_drain_facts_t* drain);
#ifdef __cplusplus
}
#endif
#endif
