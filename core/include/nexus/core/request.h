/**
 * \file            request.h
 *
 * \brief           Caller-owned admission, borrow, settlement and stable
 *                  cancellation.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_REQUEST_H
#define NEXUS_REQUEST_H

#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Request state; only READY and SETTLED have no provider
 *                  borrow.
 */
typedef enum {
    NX_REQUEST_READY = 0,
    NX_REQUEST_QUEUED,
    NX_REQUEST_ACTIVE,
    NX_REQUEST_DRAINING,
    NX_REQUEST_SETTLED,
    NX_REQUEST_QUARANTINED
} nx_request_state_t;
/**
 * \brief           Caller-owned request metadata, without payload or
 *                  callbacks.
 *
 * \note            Fields are private to helpers/providers while borrowed.
 *                  Initialize before first use. One owner prepares/submits and
 *                  consumes results. GCC/Clang lock-free 32-bit atomic access
 *                  publishes state; fields are read only after acquire
 *                  SETTLED.
 */
typedef struct {
    uint32_t state;
    nx_result_t result;
    size_t transferred;
    nx_time_us_t deadline;
} nx_request_t;
/**
 * \brief           Adapter-owned stable cancellation identity.
 *
 * \note            The slot, not a possibly freed request, is targeted by
 *                  cross- task commands. Serialize bind/lookup/release in the
 *                  adapter. Keep slot and command storage until command
 *                  acknowledgement; epoch exhaustion rejects rebinding rather
 *                  than reusing IDs.
 */
typedef struct {
    nx_request_t* request;
    uint64_t epoch;
} nx_request_slot_t;
/**
 * \brief           Initialize fresh unborrowed request storage.
 *
 * \param[out]      request: Fresh storage, not visible to a provider/observer.
 *
 * \note            Task-only. Never use initialization to revoke a live
 *                  borrow.
 */
void nx_request_initialize(nx_request_t* request);
/**
 * \brief           Prepare a READY or consumed SETTLED request.
 *
 * \param[in,out]   request: Storage with no remaining observers or adapter
 *                  loans.
 *
 * \param[in]       deadline: Absolute transfer deadline, including queue time.
 *
 * \return          NX_SUCCESS; invalid pointer or borrowed state is rejected.
 *
 * \note            Single owner, task-only; failure creates no borrow.
 */
nx_result_t nx_request_prepare(nx_request_t* request, nx_time_us_t deadline);
/**
 * \brief           Read request state with acquire ordering.
 *
 * \param[in]       request: Initialized storage kept alive by the caller.
 *
 * \return          Current state, or QUARANTINED for NULL.
 *
 * \note            Task/IRQ bounded; SETTLED guarantees provider's final
 *                  access has occurred, not that separate adapter observers
 *                  exited.
 */
nx_request_state_t nx_request_state(const nx_request_t* request);
/**
 * \brief           Establish the chain's unique admission and provider borrow.
 *
 * \param[in,out]   request: READY request; caller owns storage until SETTLED.
 *
 * \param[in]       state: QUEUED for adapter or ACTIVE for direct provider.
 *
 * \return          NX_SUCCESS means ACCEPTED. Any failure means REJECTED with
 *                  zero retained references, payload access or residual
 *                  effects.
 *
 * \note            One serialized execution owner; validate hardware/buffers
 *                  first. Multi-producer adapters hold their metadata guard
 *                  across admission; concurrent admissions are unsupported.
 */
nx_result_t nx_request_admit(nx_request_t* request, nx_request_state_t state);
/**
 * \brief           Advance an already admitted request without readmission.
 *
 * \param[in,out]   request: Admitted request owned by the executor.
 *
 * \param[in]       state: ACTIVE, DRAINING or QUARANTINED.
 *
 * \return          NX_SUCCESS or NX_ERROR_STATE for an illegal transition.
 *
 * \note            Execution owner; short task/IRQ metadata operation.
 */
nx_result_t nx_request_transition(nx_request_t* request,
                                  nx_request_state_t state);
/**
 * \brief           Release-publish SETTLED as the final provider access.
 *
 * \param[in,out]   request: Borrowed storage after all sources have drained.
 *
 * \param[in]       result: Final transfer result, not admission status.
 *
 * \param[in]       transferred: Valid transferred bytes/samples.
 *
 * \note            Detach active/queued references and capture any static wake
 *                  target first. No hardware/IRQ/deferred observer may access
 *                  request or payload afterwards. Cancel/timeout alone do not
 *                  satisfy this requirement. A failed drain stays QUARANTINED.
 */
void nx_request_settle(nx_request_t* request, nx_result_t result,
                       size_t transferred);
/**
 * \brief           Copy the unique settled result after acquire publication.
 *
 * \param[in]       request: Live request storage with no concurrent reuse.
 *
 * \param[out]      result: Final operation result.
 *
 * \param[out]      transferred: Valid byte/sample count.
 *
 * \return          NX_SUCCESS, NX_ERROR_BUSY while borrowed or invalid
 *                  arguments.
 */
nx_result_t nx_request_result(const nx_request_t* request, nx_result_t* result,
                              size_t* transferred);
/**
 * \brief           Bind a stable slot to a new request incarnation.
 *
 * \param[in,out]   slot: Zero-initialized, unbound adapter-owned slot.
 *
 * \param[in]       request: Live storage to bind; binding itself is not
 *                  admission.
 *
 * \param[out]      epoch: Nonzero identity for later control commands.
 *
 * \return          NX_SUCCESS, BUSY, INVALID or EXHAUSTED; no identity
 *                  repeats.
 */
nx_result_t nx_request_slot_bind(nx_request_slot_t* slot, nx_request_t* request,
                                 uint64_t* epoch);
/**
 * \brief           Resolve cancellation using stable storage before
 *                  dereference.
 *
 * \param[in]       slot: Stable slot protected by adapter synchronization.
 *
 * \param[in]       expected_epoch: Incarnation named by the command.
 *
 * \return          Live request only for a matching bound incarnation; else
 *                  NULL.
 */
nx_request_t* nx_request_slot_lookup(const nx_request_slot_t* slot,
                                     uint64_t expected_epoch);
/**
 * \brief           Drop a slot reference only after settlement and control
 *                  drain.
 *
 * \param[in,out]   slot: Stable slot whose commands/observers have exited.
 *
 * \param[in]       expected_epoch: Incarnation to release.
 *
 * \return          NX_SUCCESS, STATE for stale identity, BUSY while borrowed.
 */
nx_result_t nx_request_slot_release(nx_request_slot_t* slot,
                                    uint64_t expected_epoch);
#ifdef __cplusplus
}
#endif

#endif
