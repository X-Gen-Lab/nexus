/**
 * \file            usb_acm.h
 * \brief           Optional caller-owned CDC ACM control subset
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_USB_ACM_H
#define NEXUS_COMPONENTS_USB_ACM_H
#include "nexus/components/usb_control.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief CDC wire values, not a claim that a UART supports every value. */
typedef struct {
    uint32_t rate;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t data_bits;
} nx_usb_acm_line_coding_t;
/** \brief Last acknowledged software intent; physical application is external.
 */
typedef struct {
    nx_usb_acm_line_coding_t line_coding;
    uint16_t control_lines;
    uint16_t break_duration;
} nx_usb_acm_state_t;
typedef enum {
    NX_USB_ACM_GET_LINE_CODING,
    NX_USB_ACM_SET_LINE_CODING,
    NX_USB_ACM_SET_CONTROL_LINE_STATE,
    NX_USB_ACM_SEND_BREAK
} nx_usb_acm_change_t;
/** \brief Value copy for external authorization, never a mutable DATA view. */
typedef struct {
    nx_usb_acm_state_t proposed;
    uint64_t epoch;
    nx_usb_acm_change_t change;
} nx_usb_acm_pending_t;
typedef struct {
    nx_usb_acm_state_t committed;
    uint64_t committed_epoch;
    uint64_t completed_epoch;
    nx_result_t result;
    nx_result_t wire_result;
    nx_usb_control_phase_t phase;
    bool active;
} nx_usb_acm_snapshot_t;
/**
 * \brief           Fixed caller storage; fields are private while bound.
 *
 * \note            Metadata require natural C alignment and nonwrapping
 *                  extents; wire bytes require only byte alignment. Bind once
 *                  from zero initialization. Never zero/rebind to revoke a
 *                  loan. The profile, faces and this complete context outlive
 *                  every packet/outer loan and separate observer. No dynamic
 *                  allocation, per-instance operation table, worker or default
 *                  instance exists. Provider faces retain shared readonly ops.
 */
typedef struct {
    nx_usb_control_t control;
    nx_usb_control_request_t reply;
    nx_usb_acm_state_t committed;
    nx_usb_acm_pending_t pending;
    nx_time_us_t deadline;
    uint64_t last_epoch;
    uint64_t committed_epoch;
    uint64_t completed_epoch;
    nx_result_t result;
    nx_result_t wire_result;
    uint8_t wire[7];
    uint8_t interface_number;
    bool active;
    bool outer_started;
    bool approved;
} nx_usb_acm_t;
/**
 * \brief           Bind one explicit communication interface and initial state.
 *
 * \param[in,out]   acm: Naturally aligned zero-initialized caller storage.
 *
 * \param[in]       profile: Immutable selected USB profile with a guarded SETUP
 *                  rejection port, disjoint from acm.
 *
 * \param[in]       interface_number: Exact externally described ACM interface.
 *
 * \param[in]       initial: Disjoint naturally aligned initial software intent.
 *
 * \return          SUCCESS, INVALID, STATE or CONTEXT; no hardware is started.
 *
 * \note            Same privileged unmasked serialized task as USBControl; no
 *                  ISR, callback or second-task reentry. The external
 *                  dispatcher gives this adapter exclusive EP0 use while
 *                  active. Interface descriptors, class capabilities and
 *                  configuration readiness are external; initialization does
 *                  not validate class bytes.
 */
nx_result_t nx_usb_acm_initialize(nx_usb_acm_t* acm,
                                  const nx_usb_device_profile_t* profile,
                                  uint8_t interface_number,
                                  const nx_usb_acm_state_t* initial);
/**
 * \brief           Dispatch the current software-published SETUP to the subset.
 *
 * \param[in,out]   acm: Bound inactive adapter; no other EP0 packet user.
 *
 * \param[in]       epoch: Exact nonzero published SETUP identity.
 *
 * \param[in]       deadline: One independent absolute deadline for all stages
 *                  and external approval; NEVER permits an indefinite wait.
 *
 * \return          SUCCESS accepts the operation; BUSY/STATE/TIMEOUT reject
 *                  without a new loan. Unknown requests return UNSUPPORTED and
 *                  malformed fields INVALID after guarded STALL succeeds;
 *                  otherwise the guarded rejection error is returned.
 *
 * \note            Exact class/interface direction, wIndex, wValue and wLength
 *                  are checked. GET_LINE_CODING returns seven committed bytes.
 *                  SET_LINE_CODING receives exactly seven bytes but waits for
 *                  explicit approval. Control-line/break requests wait before
 *                  starting STATUS. Invalid/unknown dispatch deliberately has
 *                  the documented STALL effect, but retains no new loan. Route
 *                  standard/other-class requests externally before calling.
 */
nx_result_t nx_usb_acm_begin(nx_usb_acm_t* acm, uint64_t epoch,
                             nx_time_us_t deadline);
/**
 * \brief           Advance USBControl once and fold only acquired completion.
 *
 * \param[in,out]   acm: Same executor; keep all storage while active.
 *
 * \return          Progress/provider result; an error never revokes a loan.
 *
 * \note            At most one underlying USBControl step, with its one
 *                  service, one state read and one packet action bounds. No
 *                  loop drains a transaction. Continue after
 *                  cancel/supersession/timeouts. OUT0 quarantine still needs
 *                  explicit external device-owner stop, then further steps.
 *                  Never auto-stop the controller. Only approved outer acquire
 *                  SUCCESS commits intent: it proves the matching STATUS
 *                  packet's real ACK. A later SETUP/reset, closing or failed
 *                  state read cannot revoke an ACK already acquired by the
 *                  packetizer. Supersession before ACK aborts and drains
 *                  without commit. Resetting class state after a completed
 *                  command is separate external owner policy. Software epoch
 *                  checks do not promise atomicity with uncollected physical
 *                  SETUP packets.
 */
nx_result_t nx_usb_acm_step(nx_usb_acm_t* acm);
/**
 * \brief           Copy staged intent for external validation/authorization.
 *
 * \param[in]       acm: Same task; SET_LINE_CODING must be DATA_READY.
 *
 * \param[out]      pending: Disjoint naturally aligned value output; unchanged
 *                  on rejection. No pointer into the borrowed payload escapes.
 *
 * \return          SUCCESS, STATE, INVALID or CONTEXT. INVALID on a malformed
 *                  or short line-coding DATA stage; owner can reject
 *                  explicitly.
 *
 * \note            Wire format allows nonzero rate, stop bits 0..2, parity 0..4
 *                  and data bits 5/6/7/8/16. External hardware/policy may
 *                  support fewer values. This value may become stale; approve
 *                  rechecks the software epoch. Inspecting is not approval.
 */
nx_result_t nx_usb_acm_pending(const nx_usb_acm_t* acm,
                               nx_usb_acm_pending_t* pending);
/**
 * \brief           Authorize staged intent before STATUS, or reject this epoch.
 *
 * \param[in,out]   acm: Same active adapter and serialized task executor.
 *
 * \param[in]       epoch: Exact operation identity.
 *
 * \param[in]       accepted: True authorizes only; false requests guarded
 *                  STALL.
 *
 * \return          SUCCESS changes authorization; other results retain live
 *                  loans and do not authorize a change. Malformed line coding
 *                  cannot be approved; false is still allowed at DATA_READY.
 *
 * \note            Approval must not apply UART/physical lines. After a newer
 *                  committed_epoch appears, the external owner applies the
 *                  copied intent and owns application failures/recovery. No
 *                  callback runs here. SEND_BREAK records 0 (stop), 0xffff
 *                  (indefinite), or a finite millisecond intent, without
 *                  timer, automatic expiry or assertion that physical break
 *                  occurred.
 */
nx_result_t nx_usb_acm_approve(nx_usb_acm_t* acm, uint64_t epoch,
                               bool accepted);
/**
 * \brief           Cancel one active identity, preserving all admitted storage.
 *
 * \param[in,out]   acm: Same executor, retained through actual drain.
 *
 * \param[in]       epoch: Exact active identity.
 *
 * \return          SUCCESS requests drain or discards unadmitted zero-data
 *                  intent; STATE for stale identity. Cancellation only
 *                  requests drain. A matching STATUS ACK acquired before or
 *                  during cancellation still wins at the packetizer; step
 *                  commits the approved original epoch on acquire outer
 *                  SUCCESS without later epoch revocation.
 */
nx_result_t nx_usb_acm_cancel(nx_usb_acm_t* acm, uint64_t epoch);
/**
 * \brief           Copy current software intent and the last observed outcome.
 *
 * \param[in]       acm: Same executor; this does not refresh device state.
 *
 * \param[out]      snapshot: Disjoint naturally aligned value output, unchanged
 *                  on error. result is the adapter outcome, wire_result the
 *                  outer transport outcome (STATE if no outer was admitted).
 *
 * \return          SUCCESS, INVALID or CONTEXT; no loan is released.
 *
 * \note            A committed_epoch update is a software notification, not an
 *                  event queue or physical apply acknowledgement. Poll after
 *                  each step before dispatching the next operation if every
 *                  committed change must be consumed. GET never changes it.
 */
nx_result_t nx_usb_acm_snapshot(const nx_usb_acm_t* acm,
                                nx_usb_acm_snapshot_t* snapshot);
#ifdef __cplusplus
}
#endif
#endif
