/**
 * \file            ethernet.h
 * \brief           Caller-owned untagged Ethernet II frames and receive loans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_ETHERNET_H
#define NEXUS_IO_ETHERNET_H
#include "nexus/core/request.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_ETH_FRAME_MIN       14U
#define NX_ETH_FRAME_MAX       1514U
#define NX_ETH_RX_STORAGE_MIN  1518U
#define NX_ETH_RX_CRC          1U
#define NX_ETH_RX_LENGTH       2U
#define NX_ETH_RX_FRAGMENT     4U
#define NX_ETH_RX_UNSUPPORTED  8U
#define NX_ETH_RX_DMA_ERROR    16U
#define NX_ETH_RX_BACKPRESSURE 32U
/** \brief Caller-owned immutable contiguous frame, excluding its hardware FCS.
 */
typedef struct {
    nx_request_t base;
    const uint8_t* data;
    size_t length;
} nx_eth_tx_request_t;
/** \brief Observed PHY fact, supplied explicitly by the external link owner. */
typedef struct {
    bool up;
    bool full_duplex;
    uint16_t speed_mbps;
} nx_eth_link_t;
/**
 * \brief Complete immutable frame; release requires this exact incarnation.
 * \note data excludes FCS; storage stays live until matching release. Copying a
 *       token does not create a second loan. Timestamp is acquisition, not SOF.
 */
typedef struct {
    const uint8_t* data;
    size_t length;
    size_t slot;
    const void* identity;
    uint64_t epoch;
    nx_time_us_t timestamp_us;
} nx_eth_rx_frame_t;
/** \brief Latched facts survive receive backpressure and failed wake hints. */
typedef struct {
    nx_eth_link_t link;
    uint64_t received;
    uint64_t discarded;
    uint64_t unavailable_events;
    uint32_t receive_flags;
    size_t pending_tx;
    size_t held_rx;
    bool started;
    bool closing;
    bool fatal_bus_error;
    bool transmit_underflow;
} nx_eth_state_t;
typedef struct {
    nx_result_t (*submit)(void*, nx_eth_tx_request_t*);
    nx_result_t (*cancel)(void*, nx_eth_tx_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*acquire)(void*, nx_eth_rx_frame_t*);
    nx_result_t (*release)(void*, const nx_eth_rx_frame_t*);
    nx_result_t (*state)(void*, nx_eth_state_t*);
    nx_result_t (*apply_link)(void*, const nx_eth_link_t*);
    nx_result_t (*stop)(void*);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
} nx_eth_ops_t;
typedef struct {
    const nx_eth_ops_t* ops;
    void* context;
} nx_eth_port_t;
/**
 * \brief Prepare an initialized, unborrowed request and its immutable frame.
 * \param[in,out] request: Initialized READY or consumed SETTLED storage.
 * \param[in] data: Immutable contiguous Ethernet II frame.
 * \param[in] length: Bytes excluding FCS, from 14 through 1514.
 * \param[in] deadline: Absolute provider-clock deadline or NEVER.
 * \return Success, INVALID for frame/overlap, STATE while borrowed.
 * \note Task owner. Ethernet II only, 14..1514 bytes excluding FCS; no VLAN or
 *       length-coded 802.3. Failure changes no request or payload.
 */
nx_result_t nx_eth_tx_prepare(nx_eth_tx_request_t* request, const void* data,
                              size_t length, nx_time_us_t deadline);
/**
 * \brief Admit one frame into caller-owned hardware descriptors.
 * \param[in] port: Stable face and live private assembly.
 * \param[in,out] request: Prepared storage borrowed on acceptance.
 * \return Success means ACCEPTED; BUSY/STATE/IO/invalid inputs reject.
 * \note Privileged serialized task, masks clear. SUCCESS is ACCEPTED. Borrow
 *       includes request and frame until acquire-observed SETTLED. Completion
 *       reports MAC wire status, never delivery to another application.
 */
nx_result_t nx_eth_port_submit(const nx_eth_port_t* port,
                               nx_eth_tx_request_t* request);
/**
 * \brief Close TX admission and drain the shared TX engine without replay.
 * \param[in] port: Serialized task-owner face.
 * \param[in,out] request: Live request accepted by this instance.
 * \return Success requests drain; STATE for a foreign request.
 * \note Task owner. Cancel is controller-wide: other pending TX requests also
 *       become CANCELLED. It never revokes a borrow. Stuck sources quarantine.
 *       Apply a confirmed link again after successful drain to reopen TX.
 */
nx_result_t nx_eth_port_cancel(const nx_eth_port_t* port,
                               nx_eth_tx_request_t* request);
/** \brief Task-owner bounded progress; no worker, lock, pool or wait.
 * \param[in] port: Serialized task-owner face and live storage.
 * \return Success, STATE before startup, or invalid context/input.
 */
nx_result_t nx_eth_port_service(const nx_eth_port_t* port);
/** \brief Task-consumer complete-frame loan; EMPTY creates no borrow.
 * \param[in] port: Stable started instance and one consumer.
 * \param[out] frame: Exact immutable frame loan, not output storage borrow.
 * \return Success, EMPTY without a loan, or invalid state/context.
 */
nx_result_t nx_eth_port_acquire(const nx_eth_port_t* port,
                                nx_eth_rx_frame_t* frame);
/**
 * \brief Final consumer access; stale, foreign and double releases reject.
 * \param[in] port: The same provider identity as acquisition.
 * \param[in] frame: Exact acquired token after final consumer access.
 * \return Success, STATE for an altered token, or invalid input/context.
 * \note A held descriptor is never rearmed, even at ring wrap or stop.
 */
nx_result_t nx_eth_port_release(const nx_eth_port_t* port,
                                const nx_eth_rx_frame_t* frame);
/** \brief Copy latched instance/link/error facts; output is not retained.
 * \param[in] port: Stable private instance.
 * \param[out] state: Unretained output, separate from all provider storage.
 * \return Success, INVALID for output alias, or CONTEXT.
 */
nx_result_t nx_eth_port_state(const nx_eth_port_t* port, nx_eth_state_t* state);
/**
 * \brief Apply external PHY observations, not auto-negotiate or invent link.
 * \param[in] port: Serialized task-owner face.
 * \param[in] link: Observed up/speed/duplex fact, never a guess.
 * \return Success, BUSY until real drain, or STATE/IO/context error.
 * \note Task owner; changing speed/duplex first requires explicit link-down
 *       and observed idle TX/RX engines and FIFOs. BUSY keeps the old mode.
 *       Reapplying an unchanged up fact is idempotent. Link-down
 *       closes admission and drains pending TX with IO; later up requires real
 *       quiescence. This is not a PHY power/reset/electrical proof.
 */
nx_result_t nx_eth_port_apply_link(const nx_eth_port_t* port,
                                   const nx_eth_link_t* link);
/**
 * \brief Close admission and withdraw MAC/DMA/IRQ sources with readback.
 * \param[in] port: Live face and all storage, including held loans.
 * \return Success releases sources; BUSY/IO retain remaining ownership.
 * \note Task owner. Closing rejects new RX loans; existing loans still release.
 *       BUSY retains hardware/storage/wake ownership; timeout
 *       quarantines pending TX. Held RX loans remain valid and must be released
 *       before SUCCESS. Caller joins IRQ/consumers before reclaiming storage.
 */
nx_result_t nx_eth_port_stop(const nx_eth_port_t* port);
/**
 * \brief Bind an optional borrowed hint after actual IRQ-policy validation.
 * \param[in] port: Started serialized task-owner face.
 * \param[in] wake: Borrowed IRQ hint; NULL requests detach.
 * \param[in] syscall_ceiling: Exact configured unshifted syscall ceiling.
 * \return Success, BUSY while publishers live, or policy/context error.
 * \note Task owner with no pending TX; notify is bounded and legal in IRQ.
 *       Service does not invoke the IRQ hint from a task. Detach after stop.
 */
nx_result_t nx_eth_port_attach_wake(const nx_eth_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif
#endif
