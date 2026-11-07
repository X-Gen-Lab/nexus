/**
 * \file            lifecycle.h
 * \brief           Static, caller-owned cold resource transactions.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_CORE_LIFECYCLE_H
#define NEXUS_CORE_LIFECYCLE_H

#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Context admission reason; callbacks never establish context. */
typedef enum {
    NX_LIFECYCLE_START = 0,
    NX_LIFECYCLE_STOP
} nx_lifecycle_operation_t;

/**
 * \brief           Actual acquisition outcome and failed-step responsibility.
 * \note            A successful acquisition always receives one release call.
 *                  On failure, retained means the current step still requires
 *                  release, including a failed write with uncertain readback.
 *                  The driver owns its concrete receipt in entry state.
 */
typedef struct {
    nx_result_t result;
    bool retained;
} nx_lifecycle_acquisition_t;

/**
 * \brief           One resource's effect-free admission and reversible work.
 * \note            admit validates without writes or retained references.
 *                  acquire reports actual partial responsibility on failure.
 *                  release returns SUCCESS only when that step has no remaining
 *                  responsibility; failure preserves its receipt for retry.
 *                  Successful acquire may perform no writes; its release then
 *                  performs no work according to its concrete receipt.
 */
typedef struct {
    nx_result_t (*admit)(void* state, const void* config);
    nx_lifecycle_acquisition_t (*acquire)(void* state, const void* config);
    nx_result_t (*release)(void* state, const void* config);
} nx_lifecycle_ops_t;

/** \brief Readonly binding; opaque state/config may be NULL if ops allow it. */
typedef struct {
    const nx_lifecycle_ops_t* ops;
    void* state;
    const void* config;
} nx_lifecycle_entry_t;

/**
 * \brief           Fixed dependency order and explicit context admission.
 * \note            All entries, ops, config and context remain immutable and
 *                  alive until complete release. Entry state remains alive and
 *                  exclusively owned. context_check is bounded and effect-free;
 *                  START admission also covers immediate failure rollback.
 *                  STOP admission runs on each explicit non-idle stop attempt.
 */
typedef struct {
    const nx_lifecycle_entry_t* entries;
    size_t count;
    nx_result_t (*context_check)(const void* context,
                                 nx_lifecycle_operation_t operation);
    const void* context;
} nx_lifecycle_plan_t;

/** \brief Public observation of a serialized transaction, never a lock. */
typedef enum {
    NX_LIFECYCLE_IDLE = 0,
    NX_LIFECYCLE_STARTING,
    NX_LIFECYCLE_ACTIVE,
    NX_LIFECYCLE_STOPPING,
    NX_LIFECYCLE_DRAINING
} nx_lifecycle_phase_t;

/**
 * \brief           Zero-initialized caller-owned engine state.
 * \note            Fields are readonly to callers after first use. cursor is
 *                  the unreleased prefix, not a hardware receipt. primary keeps
 *                  the most recent start failure; cleanup keeps the latest
 *                  cleanup result independently. No config pointer is copied
 *                  into RAM beyond bound plan identity. Separate live engines
 *                  must own disjoint entry state and hardware responsibility.
 */
typedef struct {
    const nx_lifecycle_plan_t* bound;
    size_t cursor;
    nx_result_t primary;
    nx_result_t cleanup;
    nx_lifecycle_phase_t phase;
} nx_lifecycle_state_t;

/**
 * \brief           Admit every entry before acquiring in fixed order.
 * \param[in]       plan: Explicit valid plan; an empty plan is permitted.
 * \param[in,out]   state: Fresh zero state or completely released engine.
 * \return          Start result; state.cleanup separately holds cleanup status.
 *
 * \note            Caller-serialized cold context only. No heap, registry,
 *                  graph resolution or I/O forwarding. Invalid table bindings
 *                  reject before callbacks. A failed admission has no effects.
 *                  Failed acquisition releases the retained current step and
 *                  previous steps in reverse order. First release failure
 *                  stops rollback and preserves the remaining cursor.
 *                  Reentry returns BUSY. ACTIVE/DRAINING start returns STATE.
 *                  Caller proves aligned disjoint storage and callback safety.
 *                  Permanent pre-C memory owners, watchdogs and RTC/backup
 *                  domains stay outside this reversible plan.
 */
nx_result_t nx_lifecycle_start(const nx_lifecycle_plan_t* plan,
                               nx_lifecycle_state_t* state);

/**
 * \brief           Release the remaining prefix in reverse order, with retry.
 * \param[in]       plan: Exact plan used by the live engine.
 * \param[in,out]   state: Same live storage; never reset to escape a failure.
 * \return          Cleanup result; state.primary is preserved independently.
 * \note            Complete release returns IDLE; repeated idle stop succeeds
 *                  without callbacks. A refused context or release preserves
 *                  remaining responsibility. Already released later entries
 *                  are never released again. A different live plan is rejected.
 *                  Engine state is neither thread-safe nor interrupt-safe.
 */
nx_result_t nx_lifecycle_stop(const nx_lifecycle_plan_t* plan,
                              nx_lifecycle_state_t* state);

#ifdef __cplusplus
}
#endif
#endif
