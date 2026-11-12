/**
 * \file            capture.h
 * \brief           Explicit camera snapshots and opaque storage block streams
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_CAPTURE_H
#define NEXUS_IO_CAPTURE_H

#include "nexus/core/request.h"
#include "nexus/io/frame_stream.h"
#include "nexus/io/fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One capture; every output is borrowed through observed SETTLED.
 * \note Initialize/prepare base before submission. destination is an open,
 *       idle producer stream. For RAW, raw_frame binds that same stream to
 *       one contiguous complete image layout of exactly storage_bytes. For
 *       JPEG, raw_frame is NULL and storage_bytes is the writable capacity.
 *       Storage is a positive multiple of four, including owned tail padding.
 *       SUCCESS transfers the exact RAW storage length, or the JPEG DMA word
 *       prefix after FRAME, source/FIFO/bus and both IRQs actually drain.
 *       A JPEG prefix includes hardware zero padding in its final word; its
 *       length NEVER identifies JPEG codestream bytes. The caller parses JPEG.
 *       Errors publish no image and report zero bytes. Failed drain retains
 *       request, destination, frame metadata and every FILLING producer loan.
 */
typedef struct {
    nx_request_t base;
    nx_stream_t* destination;
    nx_frame_stream_t* raw_frame;
    size_t storage_bytes;
} nx_capture_request_t;

/** \brief A finite series of complete snapshot frames with explicit gaps.
 * \note base.transferred counts successfully published whole frames, including
 *       a valid prefix on failure. storage_bytes is each RAW exact frame size
 *       or each JPEG capacity; JPEG words include hardware padding, not an
 *       exact codestream length. frame_count and deadline are positive/finite.
 *       Each frame fully joins source/FIFO/DMA/IRQs/addresses before publish
 *       and rearm. This is CM1 snapshots with gaps, never gapless CM0 capture.
 *       Missing free slots leaves the source off while the session deadline
 *       continues. Stop cancels and joins this session; it cannot revoke
 *       READY/BORROWED consumer frames, which retain their independent loans.
 */
typedef struct {
    nx_request_t base;
    nx_stream_t* destination;
    nx_frame_stream_t* raw_frame;
    size_t storage_bytes;
    size_t frame_count;
} nx_capture_frames_request_t;

/** \brief Selected camera operation; no sensor, clock or codec policy.
 * \note One privileged serialized task owns every operation.
 *       IRQ entries only latch hardware observations in provider state. The
 *       caller continues service; no hidden worker or scheduler is created.
 *       Platform prepare acquires resources. stop joins the current capture
 *       source, FIFO, DMA, IRQs and addresses; Platform close releases the
 *       selected resources and closes further controller admission. BUSY/IO
 *       preserves all loans and cleanup receipts. Repeated service/stop is
 *       required when an external sensor delays CAPTURE exit until frame end.
 *       recover may reset only the exclusively owned capture peripheral after
 *       observing disabled DMA and detached IRQs; it never resets shared DMA.
 *       Recovery invalidates format configuration. Join and restart Platform
 *       before submitting a new capture after a successful recovery.
 *       Blocks are RAW storage chunks or native 32-bit JPEG storage words,
 *       NOT images or an exact frame counter. JPEG words may include frame-end
 *       zero padding; block length is never a JPEG codestream byte length.
 *       No frame-boundary flags or parser are supplied for continuous blocks.
 *       Missing replacement or ambiguous DMA bank progress halts with LOSS;
 *       READY/BORROWED blocks are never reused. Stop does not revoke consumer
 *       blocks, which remain with the original stream until explicitly freed.
 *       Stop this owner before stopping/reclocking its Platform time domain.
 */
typedef struct {
    nx_result_t (*snapshot)(void* context, nx_capture_request_t* request);
    nx_result_t (*start_blocks)(void* context, nx_stream_t* destination);
    nx_result_t (*service)(void* context);
    nx_result_t (*cancel)(void* context, nx_capture_request_t* request);
    nx_result_t (*stop)(void* context);
    nx_result_t (*recover)(void* context);
    nx_result_t (*start_frames)(void* context,
                                nx_capture_frames_request_t* request);
} nx_capture_ops_t;

/** \brief Immutable selected controller face; context outlives every loan. */
struct nx_capture_port {
    const nx_capture_ops_t* ops;
    void* context;
};
/** \brief Same immutable typed controller face for direct operation use. */
typedef nx_capture_port_t nx_capture_t;

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_CAPTURE_H */
