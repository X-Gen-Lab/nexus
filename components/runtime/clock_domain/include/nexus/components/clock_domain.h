/**
 * \file            clock_domain.h
 * \brief           Explicit clock consumers and epoch-bound drain control
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_CLOCK_DOMAIN_H
#define NEXUS_COMPONENTS_CLOCK_DOMAIN_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Opaque provider, statically supplied by its privileged owner. */
typedef struct nx_clock_domain nx_clock_domain_t;
/**
 * \brief           One stable caller-owned consumer token
 * \note            Zero-initialize before first acquisition. On SUCCESS the
 *                  provider borrows this exact object; do not copy, mutate or
 *                  reclaim it until release succeeds. Internal fields are not
 *                  application controls. A release is the provider's final
 *                  access, not proof that a peripheral has stopped. Release
 *                  only after that consumer's IRQ, DMA and observer drain.
 */
typedef struct nx_clock_lease {
    nx_clock_domain_t* owner;
    struct nx_clock_lease* next;
    uint64_t epoch;
} nx_clock_lease_t;
/**
 * \brief           Epoch-bound close control and quiescence evidence
 * \note            A value copy is allowed. This is not an authorization token
 *                  or proof about unregistered consumers. Domain storage lives
 *                  longer than every copy. A successful reopen invalidates all
 *                  prior controls without resetting the epoch.
 */
typedef struct {
    nx_clock_domain_t* domain;
    uint64_t epoch;
} nx_clock_barrier_t;
/** \brief Only shallow sleep preserves the established clock/time domain. */
typedef enum {
    NX_CLOCK_SHALLOW_SLEEP = 0,
    NX_CLOCK_DEEP_STOP,
    NX_CLOCK_DYNAMIC_RUN
} nx_clock_power_mode_t;
/**
 * \brief           Acquire one admitted clock consumer without waiting
 * \return          SUCCESS, BUSY for closed admission, EXHAUSTED for budget,
 *                  or INVALID/STATE/CONTEXT with no retained token or effects
 * \note            Privileged Thread or configurable IRQ only; same CPU,
 *                  saved-mask metadata guard. NMI/HardFault/SMP excluded.
 *                  Capacity bounds token/alias scans. Token remains borrowed
 *                  until successful release, even while admission is closed.
 */
nx_result_t nx_clock_domain_acquire(nx_clock_domain_t* domain,
                                    nx_clock_lease_t* lease);
/**
 * \brief           Remove the exact retained token after consumer drain
 * \return          SUCCESS or INVALID/STATE/CONTEXT without removing a borrow
 * \note            Same contexts as acquire. Copied tokens cannot release the
 *                  original. There is no callback or last access after SUCCESS.
 */
nx_result_t nx_clock_domain_release(nx_clock_domain_t* domain,
                                    nx_clock_lease_t* lease);
/**
 * \brief           Close admission and return the current epoch control
 * \note            Serialized privileged Thread only. No waiting, cancellation,
 *                  clock writes or implicit consumer shutdown. Idempotent in
 *                  the same closed epoch; output must not alias live storage.
 */
nx_result_t nx_clock_domain_close(nx_clock_domain_t* domain,
                                  nx_clock_barrier_t* control);
/**
 * \brief           Validate closed admission and zero retained consumers
 * \return          SUCCESS, BUSY while consumers remain, STATE for stale/open
 *                  control, INVALID or CONTEXT; no state changes
 * \note            Same CPU privileged Thread. This certifies only explicitly
 *                  enrolled consumers. It does not authorize PLL reprogramming,
 *                  suspend kernel Tick or change the Core deadline domain.
 */
nx_result_t nx_clock_domain_quiescent(nx_clock_barrier_t control);
/**
 * \brief           Reopen a drained domain under a new admission epoch
 * \return          SUCCESS, BUSY, STATE, EXHAUSTED or CONTEXT without changes
 * \note            Serialized privileged Thread. Does not restart the clock or
 *                  reinterpret existing deadlines. Actual cold clock changes
 *                  additionally require all Tick/time providers stopped and a
 *                  new explicit deadline domain; this API performs neither.
 */
nx_result_t nx_clock_domain_reopen(nx_clock_barrier_t control);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_CLOCK_DOMAIN_H */
