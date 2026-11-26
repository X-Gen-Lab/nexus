/**
 * \file            lifecycle_probe.h
 * \brief           Explicit fault and observation ports for a host reference
 * \author          Nexus Team
 * \note            Test-only single owner; controls are used after complete
 *                  platform stop. No ISR, task concurrency or timing claim.
 */
#ifndef NEXUS_REFERENCE_LIFECYCLE_PROBE_H
#define NEXUS_REFERENCE_LIFECYCLE_PROBE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { NX_REFERENCE_PREPARE = 1, NX_REFERENCE_STOP = 2 };
/** \brief One copied cold-call observation; no provider storage escapes. */
typedef struct {
    uint32_t kind;
    uint32_t mask;
} nx_reference_event_t;

/** \brief Clear the fault and observations after complete platform stop.
 * \note Single owner only; no retained requests, allocations or waiting.
 */
void nx_reference_reset(void);
/** \brief Request a side-effect-free prepare failure for this exact mask.
 * \param[in] mask Zero disables; this fixture uses 1 and 8 for its two views.
 * \note Call only between platform lifecycles, never from an ISR.
 */
void nx_reference_fail_prepare(uint32_t mask);
/** \brief Read the bounded cold-call count without consuming observations.
 * \return Number of retained observations, at most the fixed fixture capacity.
 * \note Single owner; no state transitions or hardware access.
 */
size_t nx_reference_event_count(void);
/** \brief Copy an observation without exposing the implementation's storage.
 * \param[in] index Zero-based observation index.
 * \return The copied event, or both fields zero for an invalid index.
 * \note Single owner; no waiting or retained destination pointer.
 */
nx_reference_event_t nx_reference_event(size_t index);
#ifdef __cplusplus
}
#endif
#endif
