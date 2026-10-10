/**
 * \file            request.c
 *
 * \brief           Allocation-free request publication and stable
 *                  cancellation.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/request.h"
#include <limits.h>
#include <stdatomic.h>
#include <stddef.h>

#if !(defined(__GNUC__) || defined(__clang__))
#error "Nexus request publication requires GCC/Clang atomic builtins"
#endif
_Static_assert(sizeof(uint32_t) == 4 && sizeof(unsigned) == 4 && CHAR_BIT == 8,
               "request publication requires aligned 32-bit unsigned words");
_Static_assert(_Alignof(nx_request_t) >= _Alignof(uint32_t) &&
                   offsetof(nx_request_t, state) % _Alignof(uint32_t) == 0,
               "request state must retain natural word alignment");
#if defined(__ARM_ARCH_6M__)
/* ARMv6-M has atomic aligned word load/store with compiler acquire/release
 * barriers. Its generic INT lock-free macro also covers unsupported RMW;
 * request publication never performs RMW or needs an IRQ guard. The real
 * compiler gate verifies that these operations have no library helpers. */
_Static_assert(ATOMIC_INT_LOCK_FREE == 1 || ATOMIC_INT_LOCK_FREE == 2,
               "ARMv6-M request publication requires reviewed compiler words");
#else
_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
               "request publication requires the maintained word port");
#endif

/** \brief Initialize never-borrowed storage; this is not a revocation API. */
void nx_request_initialize(nx_request_t* request) {
    if (request != NULL) {
        request->result = NX_SUCCESS;
        request->transferred = 0;
        request->deadline = NX_DEADLINE_NEVER;
        __atomic_store_n(&request->state, NX_REQUEST_READY, __ATOMIC_RELAXED);
    }
}

/** \brief Return storage to READY only when every independent loan has exited.
 */
nx_result_t nx_request_prepare(nx_request_t* request, nx_time_us_t deadline) {
    if (request == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_request_state_t state = nx_request_state(request);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED) {
        return NX_ERROR_STATE;
    }
    request->deadline = deadline;
    request->result = NX_SUCCESS;
    request->transferred = 0;
    __atomic_store_n(&request->state, NX_REQUEST_READY, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}

/** \brief Acquire the level predicate independently of notification delivery.
 */
nx_request_state_t nx_request_state(const nx_request_t* request) {
    if (request == NULL) {
        return NX_REQUEST_QUARANTINED;
    }
    return (nx_request_state_t)__atomic_load_n(&request->state,
                                               __ATOMIC_ACQUIRE);
}

/** \brief Establish the serialized unique admission with one release state
 * store. */
nx_result_t nx_request_admit(nx_request_t* request, nx_request_state_t state) {
    if (request == NULL ||
        (state != NX_REQUEST_QUEUED && state != NX_REQUEST_ACTIVE)) {
        return NX_ERROR_INVALID;
    }
    if (nx_request_state(request) != NX_REQUEST_READY) {
        return NX_ERROR_STATE;
    }
    /* A request has one serialized admission owner. The adapter guard protects
     * its multiple producers; no competing CAS loop belongs on this path. */
    __atomic_store_n(&request->state, state, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}

/** \brief Keep admission intact across execution, cancellation and quarantine.
 */
nx_result_t nx_request_transition(nx_request_t* request,
                                  nx_request_state_t state) {
    if (request == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_request_state_t previous = nx_request_state(request);
    bool allowed =
        (previous == NX_REQUEST_QUEUED &&
         (state == NX_REQUEST_ACTIVE || state == NX_REQUEST_DRAINING)) ||
        (previous == NX_REQUEST_ACTIVE &&
         (state == NX_REQUEST_DRAINING || state == NX_REQUEST_QUARANTINED)) ||
        (previous == NX_REQUEST_DRAINING && state == NX_REQUEST_QUARANTINED);
    if (!allowed) {
        return NX_ERROR_STATE;
    }
    __atomic_store_n(&request->state, state, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}

/** \brief Publish the result and make the release store the final access. */
void nx_request_settle(nx_request_t* request, nx_result_t result,
                       size_t transferred) {
    if (request == NULL) {
        return;
    }
    nx_request_state_t previous = nx_request_state(request);
    if (previous == NX_REQUEST_READY || previous == NX_REQUEST_SETTLED) {
        return;
    }
    request->result = result;
    request->transferred = transferred;
    __atomic_store_n(&request->state, NX_REQUEST_SETTLED, __ATOMIC_RELEASE);
}

/** \brief Expose fields only after acquire has observed the terminal level. */
nx_result_t nx_request_result(const nx_request_t* request, nx_result_t* result,
                              size_t* transferred) {
    if (request == NULL || result == NULL || transferred == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_request_state_t state = nx_request_state(request);
    if (state != NX_REQUEST_SETTLED) {
        return state == NX_REQUEST_READY ? NX_ERROR_STATE : NX_ERROR_BUSY;
    }
    *result = request->result;
    *transferred = request->transferred;
    return NX_SUCCESS;
}

/** \brief Allocate a nonrepeating incarnation in stable adapter storage. */
nx_result_t nx_request_slot_bind(nx_request_slot_t* slot, nx_request_t* request,
                                 uint64_t* epoch) {
    if (slot == NULL || request == NULL || epoch == NULL) {
        return NX_ERROR_INVALID;
    }
    if (slot->request != NULL) {
        return NX_ERROR_BUSY;
    }
    if (slot->epoch == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    slot->epoch++;
    slot->request = request;
    *epoch = slot->epoch;
    return NX_SUCCESS;
}

/** \brief Check a live stable identity before ever dereferencing a request. */
nx_request_t* nx_request_slot_lookup(const nx_request_slot_t* slot,
                                     uint64_t expected_epoch) {
    if (slot == NULL || expected_epoch == 0 || slot->epoch != expected_epoch) {
        return NULL;
    }
    return slot->request;
}

/** \brief Drop the last stable reference after settlement or admission
 * rejection. */
nx_result_t nx_request_slot_release(nx_request_slot_t* slot,
                                    uint64_t expected_epoch) {
    nx_request_t* request = nx_request_slot_lookup(slot, expected_epoch);
    if (request == NULL) {
        return NX_ERROR_STATE;
    }
    nx_request_state_t state = nx_request_state(request);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED) {
        return NX_ERROR_BUSY;
    }
    slot->request = NULL;
    return NX_SUCCESS;
}
