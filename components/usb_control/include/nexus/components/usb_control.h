/**
 * \file            usb_control.h
 * \brief           Caller-owned single-executor EP0 packetizer
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_USB_CONTROL_H
#define NEXUS_COMPONENTS_USB_CONTROL_H
#include "nexus/io/usb_profile.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    NX_USB_CONTROL_IN_REPLY,
    NX_USB_CONTROL_OUT_REPLY,
    NX_USB_CONTROL_STATUS_REPLY
} nx_usb_control_reply_kind_t;
typedef enum {
    NX_USB_CONTROL_IDLE,
    NX_USB_CONTROL_WAIT_REPLY,
    NX_USB_CONTROL_DATA_IN,
    NX_USB_CONTROL_DATA_IN_ZLP,
    NX_USB_CONTROL_DATA_OUT,
    NX_USB_CONTROL_DATA_READY,
    NX_USB_CONTROL_STATUS_IN,
    NX_USB_CONTROL_STATUS_OUT,
    NX_USB_CONTROL_DRAINING,
    NX_USB_CONTROL_NEEDS_DEVICE_STOP,
    NX_USB_CONTROL_STALLED,
    NX_USB_CONTROL_CLOSED
} nx_usb_control_phase_t;
typedef struct {
    nx_request_t base;
    const uint8_t* in;
    uint8_t* out;
    size_t length;
    uint64_t setup_epoch;
    nx_usb_control_reply_kind_t kind;
} nx_usb_control_request_t;
typedef struct {
    const uint8_t* bytes;
    size_t length;
    uint64_t setup_epoch;
} nx_usb_control_out_view_t;
typedef struct {
    nx_usb_device_state_t device;
    size_t confirmed_bytes;
    nx_usb_control_phase_t phase;
    bool outer_active;
} nx_usb_control_snapshot_t;
/**
 * \brief           Adapter-private fields, caller-owned and fixed capacity.
 *
 * \note            Public context/request/output metadata require natural C
 *                  alignment and nonwrapping extents. Misalignment is INVALID
 *                  before typed access. Pointers still require documented
 *                  readable/writable storage: numeric checks cannot validate
 *                  arbitrary address maps or opaque private provider contexts.
 */
typedef struct {
    const nx_usb_device_profile_t* profile;
    nx_usb_request_t packets[2];
    nx_usb_control_request_t* active;
    nx_usb_device_state_t latest;
    nx_usb_setup_t setup;
    uint64_t completed_epoch;
    size_t goal;
    size_t confirmed_bytes;
    nx_result_t terminal;
    nx_usb_control_phase_t phase;
    bool borrowed[2];
    bool cancel_sent[2];
    bool cancel_pending;
    bool stall_pending;
} nx_usb_control_t;
/**
 * \brief           Bind zero-initialized caller storage to a stable validated
 *                  ROM profile.
 *
 * \param[in,out]   control: Zero-initialized storage, disjoint from the
 *                  profile.
 *
 * \param[in]       profile: Immutable facts and faces outliving every adapter
 *                  loan.
 *
 * \return          Success or rejection; no hardware, task or allocation is
 *                  created.
 *
 * \note            Every function except preparation uses one serialized
 *                  privileged task executor with IRQ masks clear. No
 *                  callback, second task or ISR may reenter. The owner gives
 *                  this adapter exclusive EP0 packet use; it retains external
 *                  event-ring and explicit device recovery policy. No
 *                  controller/endpoint reset invalidates a loan. A successful
 *                  bind owns no provider references until begin.
 */
nx_result_t nx_usb_control_initialize(nx_usb_control_t* control,
                                      const nx_usb_device_profile_t* profile);
/**
 * \brief           Prepare a reply without borrowing or reading immutable IN
 *                  bytes.
 *
 * \param[in,out]   request: Initialized READY or consumed SETTLED storage.
 *
 * \param[in]       bytes: Immutable external descriptor/reply; NULL only if
 *                  empty.
 *
 * \param[in]       length: Available bytes; reply is clipped to SETUP
 *                  wLength.
 *
 * \param[in]       epoch: Nonzero SETUP identity.
 *
 * \param[in]       deadline: Absolute independent monotonic deadline for all
 *                  stages.
 *
 * \return          Success, INVALID or STATE; rejection retains no
 *                  references.
 *
 * \note            Task-only preparation; bytes stay alive until the admitted
 *                  outer acquire SETTLED. A short full-MPS reply emits an
 *                  acknowledged DATA ZLP.
 */
nx_result_t nx_usb_control_in_prepare(nx_usb_control_request_t* request,
                                      const uint8_t* bytes, size_t length,
                                      uint64_t epoch, nx_time_us_t deadline);
/**
 * \brief           Prepare caller storage for OUT DATA requiring explicit
 *                  approval.
 *
 * \param[in,out]   request: Initialized unborrowed request.
 *
 * \param[out]      bytes: Stable writable payload, no early writes at
 *                  preparation.
 *
 * \param[in]       capacity: Must cover matching SETUP wLength at admission.
 *
 * \param[in]       epoch: Nonzero SETUP identity.
 *
 * \param[in]       deadline: Absolute independent deadline, including policy
 *                  delay.
 *
 * \return          Success, INVALID or STATE; no loan before successful
 *                  begin.
 *
 * \note            Task-only. Accepted bytes remain borrowed through STATUS
 *                  or actual drain. DATA_READY grants only the documented
 *                  const inspection view.
 */
nx_result_t nx_usb_control_out_prepare(nx_usb_control_request_t* request,
                                       uint8_t* bytes, size_t capacity,
                                       uint64_t epoch, nx_time_us_t deadline);
/**
 * \brief           Prepare only the zero-data request's status-IN reply.
 *
 * \param[in,out]   request: Initialized unborrowed request.
 *
 * \param[in]       epoch: Nonzero matching SETUP identity.
 *
 * \param[in]       deadline: Absolute independent deadline.
 *
 * \return          Success, INVALID or STATE; no transfer is started.
 *
 * \note            Task-only. External standard-request policy must stage
 *                  SET_ADDRESS itself and inspect authoritative
 *                  address/closing state after wire ACK.
 */
nx_result_t nx_usb_control_status_prepare(nx_usb_control_request_t* request,
                                          uint64_t epoch,
                                          nx_time_us_t deadline);
/**
 * \brief           Admit one explicit reply to the current authoritative
 *                  SETUP epoch.
 *
 * \param[in,out]   control: Bound single-executor adapter; no other live
 *                  reply.
 *
 * \param[in,out]   request: READY metadata and disjoint payload. OUT capacity
 *                  must cover wLength; IN sends min(length,wLength),
 *                  including required ZLP.
 *
 * \return          Success means ACCEPTED; every failure retains no request
 *                  or payload.
 *
 * \note            No packet is submitted by admission. Payload, request,
 *                  profile and adapter remain alive until acquire-observed
 *                  outer SETTLED. Never call prepare/reinitialize to revoke a
 *                  loan. Epochs do not repeat. Products stage SET_ADDRESS
 *                  explicitly; transport success means wire ACK only.
 */
nx_result_t nx_usb_control_begin(nx_usb_control_t* control,
                                 nx_usb_control_request_t* request);
/**
 * \brief           Make one bounded progress step, at most one poll and one
 *                  packet action.
 *
 * \param[in,out]   control: Same serialized executor and lifetime as begin.
 *
 * \return          Success or provider error; an error never revokes accepted
 *                  storage.
 *
 * \note            No queue or worker. All packet deadlines use the outer
 *                  independent absolute monotonic deadline. Poll while
 *                  borrowed, even after cancel, reset, SETUP supersession or
 *                  device stop. Cancel/drain/quarantine retain descriptor
 *                  bytes and outer context. OUT0 quarantine exposes
 *                  NEEDS_DEVICE_STOP; only the external controller owner may
 *                  authorize stop. This adapter never stops the shared
 *                  controller automatically. One SETTLED packet is folded
 *                  before observing newer controller epochs.
 */
nx_result_t nx_usb_control_step(nx_usb_control_t* control);
/**
 * \brief           Request cancellation without revoking the old packet or
 *                  outer loan.
 *
 * \param[in,out]   control: Same serialized privileged task executor.
 *
 * \param[in,out]   request: Exact active outer request, kept alive until
 *                  SETTLED.
 *
 * \param[in]       epoch: Exact active SETUP identity; no cross-task raw
 *                  pointer use.
 *
 * \return          Success requests drain; STATE rejects stale identity
 *                  without effects.
 *
 * \note            An already acquired STATUS ACK wins. The next step issues
 *                  at most one packet cancel; continue polling and retain
 *                  quarantine until actual source withdrawal. OUT0 recovery
 *                  may need explicit device-owner stop.
 */
nx_result_t nx_usb_control_cancel(nx_usb_control_t* control,
                                  nx_usb_control_request_t* request,
                                  uint64_t epoch);
/**
 * \brief           Inspect completed OUT DATA before the external policy
 *                  approves STATUS.
 *
 * \param[in]       control: DATA_READY with no provider packet references.
 *
 * \param[out]      view: Disjoint value output, unchanged on rejection.
 *
 * \return          Success grants a const inspection view only; outer borrow
 *                  continues.
 *
 * \note            The view expires at the next same-owner adapter call. No
 *                  mutation, free, cross-task access or implicit status
 *                  acknowledgement is allowed.
 */
nx_result_t nx_usb_control_out_view(const nx_usb_control_t* control,
                                    nx_usb_control_out_view_t* view);
/**
 * \brief           Approve OUT policy before STATUS IN, or reject with a
 *                  bounded STALL.
 *
 * \param[in,out]   control: DATA_READY same-owner adapter.
 *
 * \param[in]       epoch: Exact active SETUP epoch, checked against fresh
 *                  state.
 *
 * \param[in]       accepted: True permits status; false stalls the old
 *                  transaction.
 *
 * \return          Success changes policy only; the next step performs
 *                  packet/STALL.
 *
 * \note            Bytes, class dispatch, configuration readiness and
 *                  recovery policy are external. No CDC/HID stack is
 *                  supplied. A guarded decision excludes an already
 *                  software-published replacement SETUP. Physical bus packets
 *                  not yet collected by IRQ are outside that software epoch
 *                  boundary. Cancellation drains before the outer loan ends.
 */
nx_result_t nx_usb_control_approve(nx_usb_control_t* control, uint64_t epoch,
                                   bool accepted);
/**
 * \brief           Copy the last acquired authoritative state and adapter
 *                  phase.
 *
 * \param[in]       control: Same single task executor; no provider operation
 *                  occurs.
 *
 * \param[out]      snapshot: Disjoint value output, unchanged on rejection.
 *
 * \return          Success, INVALID or CONTEXT.
 *
 * \note            Call step to refresh controller state. Wire success does
 *                  not imply address/configuration readiness. Snapshots never
 *                  release a loan.
 */
nx_result_t nx_usb_control_snapshot(const nx_usb_control_t* control,
                                    nx_usb_control_snapshot_t* snapshot);
#ifdef __cplusplus
}
#endif
#endif
