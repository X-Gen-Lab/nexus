/**
 * \file            ethernet_frames.h
 * \brief           Optional scatter-gather frame and MAC-clock loans
 * \author          Nexus Team
 * \note            Physical qualification is separate from software validation.
 */
#ifndef NEXUS_IO_ETHERNET_FRAMES_H
#define NEXUS_IO_ETHERNET_FRAMES_H
#include "nexus/io/ethernet.h"
#include "nexus/io/fwd.h"
#include "nexus/io/observation.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Immutable TX fragment; its bytes remain borrowed until SETTLED. */
typedef struct {
    const uint8_t* data;
    size_t length;
} nx_eth_fragment_t;
/** \brief Capture diagnostics; UNKNOWN stamp remains unusable measurement. */
#define NX_ETH_FRAMES_CAPTURE_CORRUPT 1U
/* nx_io_timestamp_t is the sole time-fact type. Captures have source
 * HARDWARE_EVENT, extended nanosecond ticks, rate1e9, assembly domain/epoch and
 * unknown uncertainty. Missing/corrupt capture is UNKNOWN, never acquired_us.
 */
/** \brief Single in-band 0x8100 C-tag; TCI is host-order, with PCP/DEI/VID. */
typedef struct {
    uint16_t tci;
    bool present;
} nx_eth_vlan_t;
/**
 * \brief One whole L2 frame, excluding preamble, hardware pad and hardware FCS.
 * \note The fragment array and every fragment remain borrowed after ACCEPTED.
 *       Requested TX capture is optional: wire success may have an unavailable
 *       or corrupt stamp. Read stamp only after acquire-observed SETTLED.
 */
typedef struct {
    nx_request_t base;
    const nx_eth_fragment_t* fragments;
    size_t fragment_count;
    bool timestamp_requested;
    nx_io_timestamp_t stamp;
    uint32_t capture_flags;
} nx_eth_frames_tx_request_t;
/**
 * \brief One complete receive loan over all descriptors of one frame.
 * \note Output fragment views exclude FCS, even when FCS spans two buffers.
 *       The views array is an unretained output. Its underlying bytes stay
 *       immutable until exact release. Copying this token creates no loan.
 */
typedef struct {
    const void* identity;
    uint64_t epoch;
    size_t first_slot;
    size_t descriptor_count;
    size_t fragment_count;
    size_t length;
    nx_time_us_t acquired_us;
    nx_io_timestamp_source_t acquired_source;
    nx_eth_vlan_t vlan;
    nx_io_timestamp_t stamp;
    uint32_t capture_flags;
} nx_eth_frames_rx_loan_t;
/** \brief Latched engine facts and selected clock epoch, not protocol state. */
typedef struct {
    nx_eth_state_t mac;
    uint32_t clock_epoch;
    uint64_t receive_gaps;
    uint32_t timestamp_resolution_ns;
    bool quarantined;
} nx_eth_frames_state_t;
typedef struct {
    nx_result_t (*submit)(void*, nx_eth_frames_tx_request_t*);
    nx_result_t (*cancel)(void*, nx_eth_frames_tx_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*acquire)(void*, nx_eth_frames_rx_loan_t*, nx_eth_fragment_t*,
                           size_t);
    nx_result_t (*release)(void*, const nx_eth_frames_rx_loan_t*);
    nx_result_t (*state)(void*, nx_eth_frames_state_t*);
    nx_result_t (*clock)(void*, nx_io_timestamp_t*);
    nx_result_t (*apply_link)(void*, const nx_eth_link_t*);
    nx_result_t (*stop)(void*);
    nx_result_t (*recover)(void*, nx_time_us_t);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
} nx_eth_frames_ops_t;
struct nx_eth_frames_port {
    const nx_eth_frames_ops_t* ops;
    void* context;
};
/**
 * \brief Prepare unborrowed request storage without hardware effects.
 * \note Serialized task owner. Validate nonempty fragments, overflow and the
 *       complete Ethernet II header across fragments; permit one in-band C-tag.
 *       Untagged length is 14..1514, tagged 18..1518, excluding FCS. Hardware
 *       inserts padding/FCS. Length-coded 802.3, QinQ and 0x88A8 reject.
 *       Failure preserves request and retains no references. Deadline is an
 *       absolute monotonic provider time, distinct from the PTP clock domain.
 */
nx_result_t nx_eth_frames_tx_prepare(nx_eth_frames_tx_request_t* request,
                                     const nx_eth_fragment_t* fragments,
                                     size_t fragment_count, bool timestamp,
                                     nx_time_us_t deadline);
/**
 * \brief ACCEPT one complete frame after all storage and slots are validated.
 * \note Task owner, privileged/unmasked; no pool, heap, worker or OS lock.
 *       REJECTED retains no references or OWN effects. ACCEPTED borrows
 * request, fragment array and every payload until acquire-observed SETTLED. One
 *       returned descriptor does not prove wire completion. transferred is
 *       whole frame bytes on proven MAC success, otherwise zero.
 */
nx_result_t nx_eth_frames_port_submit(const nx_eth_frames_port_t* port,
                                      nx_eth_frames_tx_request_t* request);
/**
 * \brief Request controller-wide TX withdrawal without replay.
 * \note Other pending TX are cancelled too; observed completed wire success
 *       can win. No cancellation revokes a borrow. Missing drain proof keeps
 *       QUARANTINED loans. This does not reset the shared MAC/PTP/RX owner.
 */
nx_result_t nx_eth_frames_port_cancel(const nx_eth_frames_port_t* port,
                                      nx_eth_frames_tx_request_t* request);
/** \brief Bounded task progress; IRQ handles at most one descriptor per side.
 */
nx_result_t nx_eth_frames_port_service(const nx_eth_frames_port_t* port);
/**
 * \brief Acquire one complete frame with whole-frame capacity admission.
 * \note One task consumer. EMPTY creates no loan; OVERFLOW for too few views
 *       leaves the complete frame queued and all outputs unchanged. Stop closes
 *       new acquisitions. Held descriptors are never rearmed at wrap or stop.
 */
nx_result_t nx_eth_frames_port_acquire(const nx_eth_frames_port_t* port,
                                       nx_eth_frames_rx_loan_t* loan,
                                       nx_eth_fragment_t* fragments,
                                       size_t capacity);
/** \brief Final consumer access; stale/altered/foreign/double release rejects.
 */
nx_result_t nx_eth_frames_port_release(const nx_eth_frames_port_t* port,
                                       const nx_eth_frames_rx_loan_t* loan);
/** \brief Copy latched facts to unretained output disjoint from every loan. */
nx_result_t nx_eth_frames_port_state(const nx_eth_frames_port_t* port,
                                     nx_eth_frames_state_t* state);
/**
 * \brief Read a coherent current hardware counter in its own domain.
 * \note No clock synchronization, UTC mapping, acquired-time fallback or PHY
 *       compensation. source is SERVICE_OBSERVATION, not an SFD event. The
 *       owner extends seconds wrap and rejects regression, overflow and
 *       ambiguous gaps; ticks are nanoseconds with rate1e9 and unknown
 *       uncertainty.
 */
nx_result_t nx_eth_frames_port_clock(const nx_eth_frames_port_t* port,
                                     nx_io_timestamp_t* stamp);
/** \brief Apply explicit external PHY observation after real TX/RX quiet. */
nx_result_t nx_eth_frames_port_apply_link(const nx_eth_frames_port_t* port,
                                          const nx_eth_link_t* link);
/**
 * \brief Close admission and prove DMA/MAC/IRQ withdrawal before release.
 * \note Held RX and captured publishers retain ownership. Any failure keeps
 *       unfinished leases. Do not flush a nonempty TX FIFO to fake quiet.
 *       Caller joins all external producers/consumers/observers separately.
 */
nx_result_t nx_eth_frames_port_stop(const nx_eth_frames_port_t* port);
/**
 * \brief Explicit recovery by the exclusive MAC owner after failed drain.
 * \note Task owner; finite absolute deadline. Held RX or captured publishers
 *       return BUSY without reset. Reset acknowledgement is mandatory before
 *       detached pending TX can settle with their locked terminal cause; never
 *       report wire success without its actual status. No auto replay. Reset
 *       invalidates the old hardware clock epoch and records an RX gap. Source
 *       failure/timeout retains QUARANTINED loans and recovery responsibility.
 */
nx_result_t nx_eth_frames_port_recover(const nx_eth_frames_port_t* port,
                                       nx_time_us_t deadline);
/** \brief Optional borrowed IRQ hint; validate actual priority, detach/join. */
nx_result_t nx_eth_frames_port_attach_wake(const nx_eth_frames_port_t* port,
                                           const nx_irq_wake_t* wake,
                                           uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif
#endif
