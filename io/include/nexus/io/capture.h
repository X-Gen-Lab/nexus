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
 *       and rearm. Finite snapshots allow gaps; they do not promise gapless
 *       continuous frames.
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

/** \brief Admit one snapshot using the selected source and storage contract.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_capture_port_snapshot(const nx_capture_port_t* port,
                                     nx_capture_request_t* request);
/** \brief Start an opaque block session using caller-owned stream storage.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] destination: Original stable storage under the
 * operation loan contract. \return Provider result, INVALID for malformed
 * public objects, or UNSUPPORTED when this selected operation is absent. \note
 * The selected owner defines context and timing. Dispatch retains no
 *       references, starts no worker and never revokes a live borrow. Provider
 *       admission still checks private state and payload aliases. The original
 *       argument reaches that owner without a temporary or format conversion.
 */
nx_result_t nx_capture_port_start_blocks(const nx_capture_port_t* port,
                                         nx_stream_t* destination);
/** \brief Advance bounded source progress and withdrawal without a hidden
 * worker. \param[in] port: Stable face under the selected serialized execution
 * contract. \return Provider result, INVALID for malformed public objects, or
 *         UNSUPPORTED when this selected operation is absent.
 * \note The selected owner defines context and timing. Dispatch retains no
 *       references, starts no worker and never revokes a live borrow. Provider
 *       admission still checks private state and payload aliases. The original
 *       argument reaches that owner without a temporary or format conversion.
 */
nx_result_t nx_capture_port_service(const nx_capture_port_t* port);
/** \brief Request snapshot withdrawal without revoking its producer loans.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_capture_port_cancel(const nx_capture_port_t* port,
                                   nx_capture_request_t* request);
/** \brief Join the producer; published consumer blocks keep independent loans.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \return Provider result, INVALID for malformed public objects, or
 *         UNSUPPORTED when this selected operation is absent.
 * \note The selected owner defines context and timing. Dispatch retains no
 *       references, starts no worker and never revokes a live borrow. Provider
 *       admission still checks private state and payload aliases. The original
 *       argument reaches that owner without a temporary or format conversion.
 */
nx_result_t nx_capture_port_stop(const nx_capture_port_t* port);
/** \brief Attempt exclusive recovery; successful recovery requires Platform
 * restart. \param[in] port: Stable face under the selected serialized execution
 * contract. \return Provider result, INVALID for malformed public objects, or
 *         UNSUPPORTED when this selected operation is absent.
 * \note The selected owner defines context and timing. Dispatch retains no
 *       references, starts no worker and never revokes a live borrow. Provider
 *       admission still checks private state and payload aliases. The original
 *       argument reaches that owner without a temporary or format conversion.
 */
nx_result_t nx_capture_port_recover(const nx_capture_port_t* port);
/** \brief Admit finite complete snapshots with the selected inter-frame gaps.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_capture_port_start_frames(const nx_capture_port_t* port,
                                         nx_capture_frames_request_t* request);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_CAPTURE_H */
