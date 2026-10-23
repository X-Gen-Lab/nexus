/**
 * \file            can.h
 * \brief           Classic CAN value frames and caller-owned mailbox requests
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_CAN_H
#define NEXUS_IO_CAN_H

#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { NX_CAN_STANDARD, NX_CAN_EXTENDED } nx_can_format_t;
typedef enum { NX_CAN_DATA, NX_CAN_REMOTE } nx_can_frame_kind_t;
/** \brief Classic 2.0A/B value frame; DLC is always 0..8, never CAN FD. */
typedef struct {
    uint32_t id;
    nx_can_format_t format;
    nx_can_frame_kind_t kind;
    uint8_t dlc;
    uint8_t data[8];
} nx_can_frame_t;
/** \brief Caller-owned frame copied at prepare; base must first be initialized.
 */
typedef struct {
    nx_request_t base;
    nx_can_frame_t frame;
} nx_can_tx_request_t;
typedef enum { NX_CAN_EVENT_FRAME, NX_CAN_EVENT_LOSS } nx_can_event_kind_t;
#define NX_CAN_LOSS_SOFTWARE             1U
#define NX_CAN_LOSS_HARDWARE_LOWER_BOUND 2U
#define NX_CAN_LOSS_SATURATED            4U
#define NX_CAN_DLC_NORMALIZED            8U
/**
 * \brief Value receive fact or ordered loss boundary.
 * \note Timestamp is IRQ acquisition/report time, not hardware SOF. Hardware
 *       overrun proves only a lower bound. Raw DLC 9..15 is normalized to 8.
 */
typedef struct {
    nx_can_frame_t frame;
    nx_time_us_t timestamp_us;
    uint64_t lost;
    uint32_t flags;
    nx_can_event_kind_t kind;
    uint8_t fifo;
    uint8_t filter;
    uint8_t raw_dlc;
} nx_can_event_t;
/** \brief Latched controller state, independent of receive-ring fullness. */
typedef struct {
    bool started;
    bool closing;
    bool bus_off;
    uint8_t pending;
    uint8_t transmit_errors;
    uint8_t receive_errors;
    uint8_t last_error;
} nx_can_state_t;
/** \brief Shared immutable operations; execution owner serializes task methods.
 */
typedef struct {
    nx_result_t (*submit)(void*, nx_can_tx_request_t*);
    nx_result_t (*cancel)(void*, nx_can_tx_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*read_events)(void*, nx_can_event_t*, size_t, size_t*);
    nx_result_t (*state)(void*, nx_can_state_t*);
    nx_result_t (*stop)(void*);
    nx_result_t (*recover)(void*);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
} nx_can_ops_t;
/** \brief Immutable face; lookup does not allocate, start or acquire hardware.
 */
struct nx_can_port {
    const nx_can_ops_t* ops;
    void* context;
};

/**
 * \brief Validate and copy a classic frame into unborrowed request storage.
 * \param[in,out] request: Initialized READY or consumed SETTLED storage.
 * \param[in] frame: Value copied here; remote data bytes are ignored.
 * \param[in] deadline: Absolute provider-clock deadline, including admission.
 * \return Success, INVALID for nonclassic frame, STATE while borrowed.
 * \note Task owner only; failure retains no reference and changes no frame.
 */
nx_result_t nx_can_tx_prepare(nx_can_tx_request_t* request,
                              const nx_can_frame_t* frame,
                              nx_time_us_t deadline);
/**
 * \brief Admit one frame into one of three hardware mailboxes.
 * \param[in] port: Stable immutable face and private storage.
 * \param[in,out] request: READY value request, kept alive until SETTLED.
 * \return Success means ACCEPTED, not on-wire completion; other results reject.
 * \note Serialized privileged task owner, incoming IRQ masks clear. REJECTED
 *       retains nothing. TXOK plus completed/empty hardware proves success.
 */
nx_result_t nx_can_port_submit(const nx_can_port_t* port,
                               nx_can_tx_request_t* request);
/**
 * \brief Request abort without revoking a borrowed request.
 * \param[in] port: Stable face owned by the serialized task executor.
 * \param[in,out] request: Live request previously accepted by this instance.
 * \return Success requests drain; STATE for an unrelated request.
 * \note Abort may race real transmission; observed TXOK remains SUCCESS.
 *       Failed abort remains QUARANTINED and must keep storage alive.
 */
nx_result_t nx_can_port_cancel(const nx_can_port_t* port,
                               nx_can_tx_request_t* request);
/**
 * \brief Process latched completions and absolute timeout/drain deadlines.
 * \param[in] port: Serialized task-owner face with incoming masks clear.
 * \return Success, STATE before start, CONTEXT for an illegal caller.
 * \note Bounded three-mailbox/six-frame work, no worker, blocking or replay.
 */
nx_result_t nx_can_port_service(const nx_can_port_t* port);
/**
 * \brief Consume value events without overwriting earlier unconsumed facts.
 * \param[in] port: Stable instance; one task consumer owns this operation.
 * \param[out] events: Caller capacity; no retained output reference.
 * \param[in] capacity: Nonzero number of event records.
 * \param[out] count: Number copied; EMPTY means zero available.
 * \return Success, EMPTY, INVALID or CONTEXT.
 * \note Each bounded record copy has a short metadata exclusion; pending loss
 *       is ordered after older records and before future frames.
 */
nx_result_t nx_can_port_read_events(const nx_can_port_t* port,
                                    nx_can_event_t* events, size_t capacity,
                                    size_t* count);
/**
 * \brief Copy authoritative error/admission state, even when RX is full.
 * \param[in] port: Live instance and storage.
 * \param[out] state: Unretained value output.
 * \return Success, INVALID or CONTEXT.
 * \note Task-only, bounded; bus-off is latched until explicit recovery.
 */
nx_result_t nx_can_port_state(const nx_can_port_t* port, nx_can_state_t* state);
/**
 * \brief Close admission, abort TX and detach IRQ/RX publishers.
 * \param[in] port: Serialized owner; external observers must also join.
 * \return Success after hardware drain; BUSY retains requests/ring/wake loans.
 * \note Never resets the shared primary controller to release a peer's loan.
 *       Retry after progress. Success ends provider access to RX/wake storage.
 */
nx_result_t nx_can_port_stop(const nx_can_port_t* port);
/**
 * \brief Explicitly reenter initialization after bus-off and complete drain.
 * \param[in] port: Started owner with no active/quarantined TX slots.
 * \return Success after bounded hardware acknowledgement, BUSY while borrowed.
 * \note No automatic recovery or retransmission; domain/filter identity stays.
 */
nx_result_t nx_can_port_recover(const nx_can_port_t* port);
/**
 * \brief Borrow an optional wake target after checking all four actual IRQs.
 * \param[in] port: Serialized task owner, started and masks clear.
 * \param[in] wake: Immutable target kept alive until successful stop/detach.
 * \param[in] syscall_ceiling: Additional numerical BASEPRI ceiling.
 * \return Success, BUSY while an earlier publisher is live, PERMISSION,
 *         INVALID or CONTEXT, with no new borrow on failure.
 * \note No implicit OS policy; NULL detaches. Wake is only a hint. The sink
 *       must support both task service/cancel and configurable IRQ contexts.
 */
nx_result_t nx_can_port_attach_wake(const nx_can_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif
#endif
