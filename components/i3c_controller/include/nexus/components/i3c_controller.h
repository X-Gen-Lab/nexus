/**
 * \file            i3c_controller.h
 * \brief           Explicit bounded I3C discovery, transfers and event
 * ownership \author          Nexus Team \version         1.0.0 \date 2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_I3C_CONTROLLER_H
#define NEXUS_COMPONENTS_I3C_CONTROLLER_H
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_i3c_controller nx_i3c_controller_t;
/** \brief Controller-bound confirmed target incarnation, never a factory ID. */
typedef struct {
    nx_i3c_controller_t* controller;
    size_t index;
    uint64_t epoch;
} nx_i3c_endpoint_t;
/** \brief Stable operation identity; controller outlives all copied controls.
 */
typedef struct {
    nx_i3c_controller_t* controller;
    uint64_t epoch;
} nx_i3c_ticket_t;
typedef enum { NX_I3C_DISCOVER = 0, NX_I3C_PRIVATE, NX_I3C_CCC } nx_i3c_kind_t;
/**
 * \brief           Caller-owned finite operation; no implicit bus discovery
 * \note            Initialize base once with nx_request_initialize. Accepted
 *                  metadata and payload stay borrowed until acquire SETTLED.
 *                  TX stays immutable; transferred counts bytes handed to the
 *                  FIFO, not acknowledged target effects. No replay after
 * errors. Public fields are immutable throughout an accepted borrow.
 */
typedef struct {
    nx_request_t base;
    nx_i3c_endpoint_t endpoint;
    uint8_t* data;
    size_t bytes;
    nx_i3c_kind_t kind;
    uint8_t ccc;
    bool read;
    bool broadcast;
} nx_i3c_request_t;
typedef enum {
    NX_I3C_EVENT_IBI = 0,
    NX_I3C_EVENT_HOT_JOIN
} nx_i3c_event_kind_t;
/**
 * \brief           Copied event; no callback, target creation or hidden work
 * \note            IBI endpoint retains its exact confirmed target epoch.
 *                  Confirmed BCR bit1 must advertise IBI. Bit2 requires 1..4
 *                  payload bytes including MDB; otherwise zero data bytes.
 *                  HOT_JOIN carries no endpoint and only requests explicit
 *                  caller-controlled discovery. Payload is at most four bytes,
 *                  as in the selected H563 reference; no timestamp claim.
 */
typedef struct {
    nx_i3c_event_kind_t kind;
    nx_i3c_endpoint_t endpoint;
    uint8_t payload[4];
    uint8_t bytes;
} nx_i3c_event_t;
/** \brief Explicit loss accounting; counters saturate rather than wrap. */
typedef struct {
    uint32_t overflow;
    uint32_t rejected_ibi;
} nx_i3c_event_status_t;
/**
 * \brief           Prepare unused metadata without retaining its payload
 * \note            Serialized owner Thread. DISCOVER has no payload or target.
 *                  PRIVATE permits 1..65535 bytes. CCC permits only ENEC/DISEC
 *                  broadcast/direct writes and direct GETMWL/GETMRL/GETPID/
 *                  GETBCR/GETDCR/GETSTATUS reads, with exact defined lengths.
 *                  GETMRL uses two bytes, or three when confirmed BCR bit2
 *                  advertises an IBI payload. Address-changing, HDR and
 *                  controller-handoff CCCs reject.
 *                  All APIs require one serialized executor. Port and clock
 *                  callbacks must not reenter the core, invoke application
 *                  code or mutate retained state. No ISR, allocation, OS lock
 *                  or worker. Absolute deadlines use the injected clock.
 */
nx_result_t nx_i3c_request_prepare(nx_i3c_request_t* request,
                                   nx_i3c_kind_t kind,
                                   nx_i3c_endpoint_t endpoint, uint8_t ccc,
                                   uint8_t* data, size_t bytes, bool read,
                                   bool broadcast, nx_time_us_t deadline);
/**
 * \brief           Admit a finite operation after cold validation and readiness
 * \note            SUCCESS transfers the borrow. Rejection has no effects or
 *                  retained references. Outputs cannot alias retained
 *                  controller, port table, target/event storage, policy,
 *                  request or payload. Port and clock context spans remain
 *                  the caller's responsibility.
 */
nx_result_t nx_i3c_submit(nx_i3c_controller_t* controller,
                          nx_i3c_request_t* request, nx_i3c_ticket_t* ticket);
/**
 * \brief           Perform one bounded FIFO/event phase or one drain proof
 * \note            Partial DAA, malformed observations and errors fault
 * admission while retaining every known/tentative assigned address.
 *                  Cancel/timeout/failed drain never release a loan. Failed
 *                  quiescence stays QUARANTINED; continued service may recover.
 *                  An event overflow returns OVERFLOW and latches its counter,
 *                  without discarding an already retained queued event.
 */
nx_result_t nx_i3c_service(nx_i3c_controller_t* controller);
/** \brief Request withdrawal using stable epoch before request dereference. */
nx_result_t nx_i3c_cancel(nx_i3c_ticket_t ticket);
/** \brief Obtain only a confirmed target; tentative/retired records reject. */
nx_result_t nx_i3c_endpoint(nx_i3c_controller_t* controller, size_t index,
                            nx_i3c_endpoint_t* endpoint);
/** \brief Copy one retained event; EMPTY leaves output unchanged. */
nx_result_t nx_i3c_event_take(nx_i3c_controller_t* controller,
                              nx_i3c_event_t* event);
/** \brief Copy saturating loss counters without clearing them. */
nx_result_t nx_i3c_event_status(nx_i3c_controller_t* controller,
                                nx_i3c_event_status_t* status);
/** \brief Close admission without changing loans, target identities or events.
 */
nx_result_t nx_i3c_close(nx_i3c_controller_t* controller);
/**
 * \brief           Prove a whole-bus reset before invalidating dynamic
 * addresses \note            Requires closed admission. Hardware proof covers
 * real wire, FIFO and IRQ quiescence AND every target's dynamic address
 *                  invalidated. Peripheral reset or FIFO flush alone is never
 *                  this proof. Failure preserves all loans and address records.
 *                  Success settles outstanding work and invalidates endpoints;
 *                  retained events preserve their old epochs for the caller.
 */
nx_result_t nx_i3c_reset(nx_i3c_controller_t* controller);
/** \brief Reopen an idle unfaulted pause; faults require explicit bus reset. */
nx_result_t nx_i3c_reopen(nx_i3c_controller_t* controller);
/**
 * \brief           Close, withdraw and finally relinquish the unique controller
 * \note            Active work or queued events return BUSY. Caller continues
 *                  service, consumes events and retries. Assigned/tentative
 *                  addresses or a fault return STATE until whole-bus reset
 * proof invalidates them. Successful final wire/ FIFO/IRQ quiescence releases
 * ownership, never reconstructing this object. Storage outlives copied
 * endpoints and tickets.
 */
nx_result_t nx_i3c_stop(nx_i3c_controller_t* controller);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_I3C_CONTROLLER_H */
