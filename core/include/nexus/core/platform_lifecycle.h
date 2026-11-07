/**
 * \file            platform_lifecycle.h
 * \brief           Static platform coordination and call-scoped diagnostics.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_CORE_PLATFORM_LIFECYCLE_H
#define NEXUS_CORE_PLATFORM_LIFECYCLE_H

#include "nexus/core/lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Independent startup and cleanup results, retained effects.
 * \note            remaining_effects describes still-owned platform effects,
 *                  including an already-live platform after a refused restart;
 *                  it is not a count of effects newly acquired by this call.
 *                  Dedicated system nonzero effect masks remain intact. The
 *                  coordinator uses bit zero as a generic responsibility flag.
 */
typedef struct {
    nx_result_t primary;
    nx_result_t cleanup;
    uint32_t remaining_effects;
} nx_platform_start_result_t;

/** \brief Cold lifecycle stage; errors remain in the normal result. */
typedef enum {
    NX_PLATFORM_PHASE_NONE,
    NX_PLATFORM_PHASE_CONTEXT,
    NX_PLATFORM_PHASE_SYSTEM,
    NX_PLATFORM_PHASE_RESOURCES,
    NX_PLATFORM_PHASE_CONTROLLER
} nx_platform_phase_t;

/**
 * \brief           Caller-owned metadata, accessed only during this call.
 * \note            Controller indices are resolved controller positions, not
 *                  typed factory IDs. SIZE_MAX means no controller. Success
 *                  clears every field. Cleanup never replaces primary facts.
 */
typedef struct {
    nx_platform_phase_t phase;
    size_t controller_index;
    nx_platform_phase_t cleanup_phase;
    size_t cleanup_controller_index;
} nx_platform_diagnostic_t;

/**
 * \brief           Fixed platform connections, without per-controller state.
 * \note            Callbacks dispatch exact typed owners; no runtime lookup,
 *                  register interpreter or generic action stream is involved.
 *                  Every admission finishes before system_start. A successful
 *                  system_start establishes the system boundary; a failed one
 *                  reports its own cleanup and retained effects. Resources may
 *                  retain effects after failed acquisition and always receive
 *                  release before the system. resources_release also checks
 *                  permanent pre-C owners, even when no controller started.
 *                  Controller acquire reports failed-step responsibility;
 *                  release succeeds only after the concrete receipt is clear.
 *                  Permanent SRAM, RTC and watchdog policy remains in those
 *                  dedicated C owners. permanent_effects marks pre-C effects
 *                  on every failed start, including context rejection.
 *                  At most UINT16_MAX physical controllers are permitted;
 *                  larger plans reject before any owner callback.
 */
typedef struct {
    size_t controller_count;
    const size_t* ordinals;
    nx_result_t (*context_check)(void);
    nx_result_t (*admit)(size_t index);
    nx_platform_start_result_t (*system_start)(void);
    nx_result_t (*resources_acquire)(void);
    nx_result_t (*resources_release)(void);
    nx_result_t (*system_release)(bool started);
    nx_lifecycle_acquisition_t (*acquire)(size_t index);
    nx_result_t (*release)(size_t index);
    bool permanent_effects;
} nx_platform_lifecycle_plan_t;

/**
 * \brief           Zero-initialized storage for one serialized platform.
 * \note            Fields are readonly to callers. initialized is the remaining
 *                  physical-controller prefix; dedicated owners retain their
 *                  own receipts. bound is an opaque identity that forbids
 *                  changing a live plan. operating rejects synchronous callback
 *                  reentry; it is not a task lock.
 *                  Plan, static connections and owner storage remain alive and
 *                  unchanged until complete withdrawal. Separate platforms must
 *                  own disjoint hardware and mutable owner storage.
 */
typedef struct {
    const void* bound;
    uint16_t initialized;
    bool started;
    bool operating;
} nx_platform_lifecycle_state_t;

/**
 * \brief           Start the selected system, resources and physical owners.
 * \param[in]       plan: Immutable typed dispatch and resolved ordinals.
 * \param[in,out]   state: Caller-owned zero or completely released storage.
 * \param[out]      diagnostic: Optional call-scoped metadata; never retained.
 * \return          Primary result, independent cleanup result and effects.
 * \note            Caller-serialized cold context only. Context is checked
 *                  before effects and again before immediate rollback. Failed
 *                  initialization withdraws the retained prefix in reverse
 *                  order, stopping at the first refusal. Start of a live
 *                  platform returns STATE; synchronous reentry returns BUSY
 *                  without changing the supplied diagnostic output.
 *                  No allocation, discovery, scheduler or I/O forwarding.
 */
nx_platform_start_result_t
nx_platform_lifecycle_start(const nx_platform_lifecycle_plan_t* plan,
                            nx_platform_lifecycle_state_t* state,
                            nx_platform_diagnostic_t* diagnostic);

/**
 * \brief           Retry remaining controllers, shared resources and system.
 * \param[in]       plan: Exact live plan, or the original idle platform plan.
 * \param[in,out]   state: Same storage and retained concrete owner receipts.
 * \param[out]      diagnostic: Optional call-scoped output; never retained.
 * \return          SUCCESS only after all dedicated owners prove withdrawal.
 * \note            Join direct writers, IRQ consumers and borrowers first.
 *                  Continue service before retrying BUSY. A failure retains
 *                  the remaining prefix and system; already released later
 *                  controllers are not repeated. Even idle stop checks context
 *                  and permanent resource guards. Synchronous reentry returns
 *                  BUSY without writing diagnostic. Caller serialization and
 *                  each owner's context/deadline contracts remain mandatory.
 */
nx_result_t nx_platform_lifecycle_stop(const nx_platform_lifecycle_plan_t* plan,
                                       nx_platform_lifecycle_state_t* state,
                                       nx_platform_diagnostic_t* diagnostic);

#ifdef __cplusplus
}
#endif
#endif
