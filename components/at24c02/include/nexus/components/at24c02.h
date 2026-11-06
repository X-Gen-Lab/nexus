/**
 * \file            at24c02.h
 * \brief           Settled AT24C02 page access and explicit write-cycle drain
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_AT24C02_H
#define NEXUS_COMPONENTS_AT24C02_H

#include "nexus/io/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_AT24C02_ADDRESS    0x50U
#define NX_AT24C02_BYTES      256U
#define NX_AT24C02_PAGE_BYTES 8U

/**
 * \brief           Results copied once at the end of an admitted write call
 * \note            confirmed_bytes includes only pages whose write-cycle end
 *                  was observed under the operation deadline. uncertain marks
 *                  an issued final page with unknown outcome; its bytes must
 *                  not be counted, replayed or consumed as confirmed. A later
 *                  sync proves readiness only and never repairs this result.
 */
typedef struct {
    size_t confirmed_bytes;
    bool uncertain;
} nx_at24c02_progress_t;

/**
 * \brief           Exact caller-owned instance, fresh zero before first init
 * \note            Task-only, one serialized executor and exclusive endpoint
 *                  current-address pointer. No concurrent or reentrant calls.
 *                  The immutable endpoint, ops, opaque contexts and clock
 *                  remain live and disjoint from device and payload storage.
 *                  Fields are implementation state, not reclaim permission.
 */
typedef struct {
    const nx_i2c_endpoint_t* endpoint;
    nx_clock_t clock;
    nx_time_us_t last_clock;
    uint16_t max_ready_probes;
    bool initialized;
    bool in_call;
    bool write_pending;
    bool clock_failed;
} nx_at24c02_t;

/**
 * \brief           Bind an explicitly selected polling endpoint without I/O
 * \param[in,out]   device: Fresh zero or successfully stopped storage
 * \param[in]       endpoint: Fixed 7-bit 50h endpoint, alive through stop
 * \param[in]       clock: Same stable monotonic domain as endpoint deadlines
 * \param[in]       max_ready_probes: Nonzero per-page polling attempt limit
 * \return          SUCCESS, INVALID, UNSUPPORTED or BUSY; no bus transaction
 * \note            Board A0-A2 must be low, WP low, exact 256-byte/8-byte-page
 *                  device selected and supplies valid. No discovery or runtime
 *                  address introspection is available through the opaque face;
 *                  the caller must provide the exact addressed endpoint.
 *                  Async-only endpoints reject; no stack-buffer submit
 * fallback.
 */
nx_result_t nx_at24c02_init(nx_at24c02_t* device,
                            const nx_i2c_endpoint_t* endpoint, nx_clock_t clock,
                            uint16_t max_ready_probes);

/**
 * \brief           Random-read an in-range nonempty byte span
 * \param[in,out]   device: Initialized, idle, nonuncertain exclusive device
 * \param[in]       offset: First byte, 0 through 255
 * \param[out]      data: Separate writable span; may be partial after I/O error
 * \param[in]       bytes: 1 through 256, not crossing the device boundary
 * \param[in]       deadline: Finite absolute time, equality is expired
 * \return          SUCCESS, INVALID, STATE, BUSY, TIMEOUT or exact bus error
 * \note            Word-address write plus repeated-START read is settled on
 *                  every return. No caller buffer is retained. On validation
 *                  rejection no bus access or output mutation occurs. A pending
 *                  uncertain write requires explicit sync; no automatic replay.
 */
nx_result_t nx_at24c02_read(nx_at24c02_t* device, size_t offset, uint8_t* data,
                            size_t bytes, nx_time_us_t deadline);

/**
 * \brief           Split a span at every physical page and await each cycle
 * \param[in,out]   device: Initialized idle exclusive device
 * \param[in]       offset: First byte, 0 through 255
 * \param[in]       data: Separate immutable input, borrowed only during call
 * \param[in]       bytes: Nonempty span wholly within the 256-byte device
 * \param[in]       deadline: One finite absolute budget for all pages/probes
 * \param[out]      progress: Separate result; unchanged on argument rejection
 * \return          SUCCESS only after every page is ready; exact bus failure,
 *                  TIMEOUT or BUSY when the explicit probe limit is exhausted
 * \note            Readiness uses a settled one-byte current-address read.
 *                  Only NACK during this driver's issued cycle is retried;
 *                  ARBITRATION, IO and other failures are never called busy.
 *                  The private current-address pointer may advance. Probe-limit
 *                  or failed withdrawal retains write_pending until sync under
 *                  a new cleanup budget; no MCU buffer survives any return.
 *                  A write is irreversible, non-atomic and not cancellable.
 *                  Endurance, checksums, readback and layout policy are
 * external.
 */
nx_result_t nx_at24c02_write(nx_at24c02_t* device, size_t offset,
                             const uint8_t* data, size_t bytes,
                             nx_time_us_t deadline,
                             nx_at24c02_progress_t* progress);

/**
 * \brief           Observe completion of a retained uncertain write cycle
 * \param[in,out]   device: Same live endpoint and clock binding
 * \param[in]       deadline: Finite cleanup budget, possibly later than write
 * \return          SUCCESS when idle; BUSY at probe limit, TIMEOUT or bus error
 * \note            No data write, replay, GPIO recovery, sleep or hidden
 * worker. Readiness never proves previous contents or write success. A
 * synchronous call cannot be preempted by another call.
 */
nx_result_t nx_at24c02_sync(nx_at24c02_t* device, nx_time_us_t deadline);

/**
 * \brief           Drop only a quiescent fully drained software binding
 * \param[in,out]   device: Serialized instance with no active call
 * \return          SUCCESS or INVALID; BUSY retains active/uncertain binding
 * \note            No bus access, controller stop or physical cancellation.
 *                  A pending write must first sync or remain owned for explicit
 *                  external recovery; a timeout cannot authorize forgetting it.
 */
nx_result_t nx_at24c02_stop(nx_at24c02_t* device);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_AT24C02_H */
