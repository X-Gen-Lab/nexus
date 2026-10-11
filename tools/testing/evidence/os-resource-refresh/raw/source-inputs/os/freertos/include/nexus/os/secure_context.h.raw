/**
 * \file            secure_context.h
 * \brief           Caller-owned Secure context leases for a trusted NS kernel
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_OS_SECURE_CONTEXT_H
#define NEXUS_OS_SECURE_CONTEXT_H
#include "nexus/core/status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Minimum context capacity excludes the eight-byte ARM stack seal. */
#define NX_FREERTOS_SECURE_MIN_STACK_BYTES 64u
/**
 * \brief           Explicit Secure metadata, never stored in Nonsecure RAM.
 * \note            Zero-initialize before prepare. Fields are mechanism-owned
 *                  while prepared; applications cannot modify them. Metadata
 *                  and stack remain alive until the task is joined, lease
 *                  freed and no loaded context remains. Runtime Secure stack
 *                  capacity is an application budget, not this port minimum.
 */
typedef struct {
    uint8_t* stack;
    size_t stack_bytes;
    void* task;
    uint8_t* saved_sp;
    uint8_t* limit;
    uint8_t* top;
    uint32_t epoch;
    bool prepared;
    bool leased;
} nx_freertos_secure_context_t;
/**
 * \brief           Prepare exact Secure storage for one trusted task identity.
 * \param[out]      context: Zero-initialized caller-owned Secure metadata.
 * \param[in]       stack: Eight-byte-aligned Secure writable stack allocation.
 * \param[in]       bytes: Whole allocation, including eight-byte upper seal.
 * \param[in]       task: Opaque NS task identity, never dereferenced here.
 * \return          SUCCESS, INVALID, CONTEXT, STATE or BUSY.
 * \note            Secure privileged Thread startup/exclusive lifecycle owner.
 *                  Both memory spans must be Secure, writable and disjoint.
 *                  No NS task may run against this identity during prepare.
 *                  Repeat prepare is rejected; epochs are never reset. No
 *                  allocation, kernel operation or runtime registry is used.
 */
nx_result_t
nx_freertos_secure_context_prepare(nx_freertos_secure_context_t* context,
                                   void* stack, size_t bytes, void* task);
/**
 * \brief           Rebind retired metadata while preserving its epoch history.
 * \param[in,out]   context: Prepared, unleased and unloaded Secure metadata.
 * \param[in]       task: New trusted NS task identity.
 * \return          SUCCESS, INVALID, CONTEXT, STATE or BUSY.
 * \note            Secure lifecycle owner only after joining the previous
 *                  task and stopping every observer. Existing stack is reused;
 *                  stale handles remain invalid after the next allocation.
 */
nx_result_t
nx_freertos_secure_context_rebind(nx_freertos_secure_context_t* context,
                                  void* task);
/**
 * \brief           Resolve a trusted, statically composed Secure task record.
 * \param[in]       task: Opaque identity supplied by the trusted NS kernel.
 * \return          Prepared caller-owned metadata, or NULL when unauthorized.
 * \note            Secure application overrides the weak NULL default with
 *                  an immutable mapping. Called with both PRIMASK states held;
 *                  never dereference an NS-provided metadata pointer, allocate,
 *                  block, discover objects, call a kernel or mutate mappings.
 *                  The NS kernel and its SVC/PendSV are trusted: this mechanism
 *                  does not authenticate an adversarial NS privileged kernel.
 *                  NS Thread calls cannot spoof Handler origin.
 */
nx_freertos_secure_context_t* nx_freertos_secure_context_for_task(void* task);
/** \brief Secure-context integrity failures, distinct from normal rejection. */
typedef enum {
    NX_FREERTOS_SECURE_SEAL_FAILURE = 1,
    NX_FREERTOS_SECURE_STACK_FAILURE
} nx_freertos_secure_fault_t;
/**
 * \brief           Fail stop after a loaded Secure stack integrity failure.
 * \param[in]       fault: Seal or stack invariant violation.
 * \note            Secure application may override to record/reset safely.
 *                  Called with both masks held; must not return, block, call a
 *                  kernel or release retained storage. Default holds masks and
 *                  spins. No transport or product recovery policy is imposed.
 */
void nx_freertos_secure_fault(nx_freertos_secure_fault_t fault);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_SECURE_CONTEXT_H */
