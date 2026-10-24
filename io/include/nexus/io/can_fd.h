/**
 * \file            can_fd.h
 * \brief           Optional CAN FD value ports and retained hardware requests
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_CAN_FD_H
#define NEXUS_IO_CAN_FD_H
#include "nexus/io/can.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact wire format; the classic interface remains independent. */
typedef enum { NX_CAN_WIRE_CLASSIC, NX_CAN_WIRE_FD } nx_can_wire_format_t;
#define NX_CAN_FD_TIMESTAMP_UNAVAILABLE 16U
/**
 * \brief Value frame with an exact byte length, never raw or shifted DLC.
 * \note FD admits0..8/12/16/20/24/32/48/64 bytes; no rounding or padding on
 *       wire. FD remote frames and classic BRS are invalid. A provider may
 *       reject a valid value when its selected hardware mode lacks that form.
 */
typedef struct {
    uint32_t id;
    nx_can_format_t format;
    nx_can_frame_kind_t kind;
    nx_can_wire_format_t wire;
    uint8_t payload_bytes;
    bool brs;
    uint8_t data[64];
} nx_can_fd_frame_t;
/** \brief Prepare copies a complete value; accepted request stays borrowed. */
typedef struct {
    nx_request_t base;
    nx_can_fd_frame_t frame;
} nx_can_fd_tx_request_t;
/**
 * \brief Acquired value event, including observed transmitter ESI.
 * \note acquired_us is provider acquisition time, never SOF. A disabled
 *       hardware counter reports raw0 plus TIMESTAMP_UNAVAILABLE. filter is
 *       the hardware index within the frame format's filter list. Hardware
 *       loss proves a lower bound; classic DLC9..15 normalizes to8 with a flag.
 */
typedef struct {
    nx_can_fd_frame_t frame;
    nx_time_us_t acquired_us;
    uint64_t lost;
    uint32_t flags;
    nx_can_event_kind_t kind;
    uint16_t hardware_timestamp;
    uint8_t fifo;
    uint8_t filter;
    uint8_t raw_dlc;
    bool error_passive;
    bool nonmatching;
} nx_can_fd_event_t;
/** \brief Immutable mask filter; no product register or protocol policy. */
typedef struct {
    uint32_t id;
    uint32_t mask;
    nx_can_format_t format;
    uint8_t fifo;
} nx_can_fd_filter_t;
/** \brief Latched protocol state; payload length never describes a frame count.
 */
typedef struct {
    nx_can_state_t controller;
    uint8_t data_last_error;
    bool error_warning;
    bool error_passive;
} nx_can_fd_state_t;
typedef struct {
    nx_result_t (*submit)(void*, nx_can_fd_tx_request_t*);
    nx_result_t (*cancel)(void*, nx_can_fd_tx_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*read_events)(void*, nx_can_fd_event_t*, size_t, size_t*);
    nx_result_t (*state)(void*, nx_can_fd_state_t*);
    nx_result_t (*stop)(void*);
    nx_result_t (*recover)(void*);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
} nx_can_fd_ops_t;
struct nx_can_fd_port {
    const nx_can_fd_ops_t* ops;
    void* context;
};
/**
 * \brief Validate and copy an exact frame into unborrowed request storage.
 * \param[in,out] request: Initialized READY or consumed SETTLED storage.
 * \param[in] frame: Whole value copied without retaining this address.
 * \param[in] deadline: Absolute provider-clock deadline, including admission.
 * \return Success, INVALID for a malformed frame, STATE while borrowed.
 * \note Serialized task owner. Failure changes no frame and creates no loan.
 */
nx_result_t nx_can_fd_tx_prepare(nx_can_fd_tx_request_t* request,
                                 const nx_can_fd_frame_t* frame,
                                 nx_time_us_t deadline);
/**
 * \brief Admit one exact value into a physical hardware transmit element.
 * \param[in] port: Stable immutable selected controller face.
 * \param[in,out] request: READY request retained until acquire SETTLED.
 * \return SUCCESS means ACCEPTED; every failure rejects without effects.
 * \note Serialized privileged task, incoming masks clear. No software FIFO
 *       ordering is implied by admission; a hardware queue arbitrates by ID.
 */
nx_result_t nx_can_fd_port_submit(const nx_can_fd_port_t* port,
                                  nx_can_fd_tx_request_t* request);
/**
 * \brief Request cancellation while preserving the accepted storage loan.
 * \param[in] port: Selected face owned by the sole task executor.
 * \param[in,out] request: Exact request previously accepted by this instance.
 * \return Success requests drain, STATE rejects an unrelated request.
 * \note Physical transmission may win cancellation and remain SUCCESS.
 *       Timeout/cancel do not release storage. Failed drain is QUARANTINED.
 */
nx_result_t nx_can_fd_port_cancel(const nx_can_fd_port_t* port,
                                  nx_can_fd_tx_request_t* request);
/**
 * \brief Process bounded hardware facts and absolute deadline progress.
 * \param[in] port: Serialized privileged task face, incoming masks clear.
 * \return Success, STATE while unavailable, CONTEXT for an illegal caller.
 * \note No hidden worker, retry or blocking wait. Wake remains only a hint.
 */
nx_result_t nx_can_fd_port_service(const nx_can_fd_port_t* port);
/**
 * \brief Copy ordered value events into unretained caller output storage.
 * \param[in] port: Stable instance with one task consumer.
 * \param[out] events: Writable output, disjoint from all borrowed storage.
 * \param[in] capacity: Positive number of complete output event records.
 * \param[out] count: Unretained number copied, disjoint from events.
 * \return Success, EMPTY with zero copied, INVALID, STATE or CONTEXT.
 * \note Earlier retained records precede a pending loss boundary. Neither
 *       full receive storage nor a failed hint may erase protocol state.
 */
nx_result_t nx_can_fd_port_read_events(const nx_can_fd_port_t* port,
                                       nx_can_fd_event_t* events,
                                       size_t capacity, size_t* count);
/**
 * \brief Copy authoritative latched protocol and ownership state.
 * \param[in] port: Stable selected instance, including after successful stop.
 * \param[out] state: Writable value, disjoint from all borrowed controls.
 * \return Success, INVALID or CONTEXT. Output is never retained.
 * \note Task-only; a full event ring does not hide bus-off or error state.
 */
nx_result_t nx_can_fd_port_state(const nx_can_fd_port_t* port,
                                 nx_can_fd_state_t* state);
/**
 * \brief Close admission, drain transmit loans and withdraw IRQ publishers.
 * \param[in] port: Sole task owner; separate observers must also join.
 * \return Success releases provider loans; failure retains incomplete leases.
 * \note Never reset a shared peripheral domain to revoke another owner.
 *       Retry after physical progress. Stop success is not observer join.
 */
nx_result_t nx_can_fd_port_stop(const nx_can_fd_port_t* port);
/**
 * \brief Recover the same hardware identity after complete transmit drain.
 * \param[in] port: Started task owner with no pending or quarantined request.
 * \return Success after bounded acknowledgement, BUSY while borrowed.
 * \note Explicit recovery never replays a frame or reassigns shared RAM.
 */
nx_result_t nx_can_fd_port_recover(const nx_can_fd_port_t* port);
/**
 * \brief Borrow a wake sink after validating the actual enabled IRQ source.
 * \param[in] port: Sole task owner, started and incoming masks clear.
 * \param[in] wake: Immutable sink retained through successful detach/stop.
 * \param[in] syscall_ceiling: Additional numerical BASEPRI ceiling.
 * \return Success, BUSY while publishing, PERMISSION, INVALID or CONTEXT.
 * \note NULL detaches. Rejection preserves the earlier sink. Notification
 *       must be bounded and legal in both task and configurable IRQ contexts.
 */
nx_result_t nx_can_fd_port_attach_wake(const nx_can_fd_port_t* port,
                                       const nx_irq_wake_t* wake,
                                       uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif
#endif
