/**
 * \file            fsm.h
 * \brief           Synchronous table-driven finite state machines
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FSM_H
#define NEXUS_COMPONENTS_FSM_H

#include "nexus/core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief A borrowed event, retained only during synchronous dispatch. */
typedef struct {
    uint32_t id;
    const void* payload;
    size_t size;
} nx_fsm_event_t;

/** \brief Bounded callbacks on the caller's serialized execution context. */
typedef void (*nx_fsm_action_t)(void* context, const nx_fsm_event_t* event);
/** \brief A guard must not mutate the machine or retain the borrowed event. */
typedef bool (*nx_fsm_guard_t)(void* context, const nx_fsm_event_t* event);

/** \brief State actions; either callback may be NULL. */
typedef struct {
    nx_fsm_action_t entry;
    nx_fsm_action_t exit;
} nx_fsm_state_t;

/** \brief External transitions run exit/entry, including external self loops.
 */
typedef enum { NX_FSM_EXTERNAL, NX_FSM_INTERNAL } nx_fsm_transition_kind_t;

/** \brief A transition; internal transitions must have target == source. */
typedef struct {
    size_t source;
    size_t target;
    uint32_t event_id;
    nx_fsm_transition_kind_t kind;
    nx_fsm_guard_t guard;
    nx_fsm_action_t action;
} nx_fsm_transition_t;

/**
 * \brief           Immutable borrowed tables, valid until the machine is no
 *                  longer used and all calls have returned.
 */
typedef struct {
    const nx_fsm_state_t* states;
    size_t state_count;
    const nx_fsm_transition_t* transitions;
    size_t transition_count;
} nx_fsm_config_t;

/**
 * \brief           Caller-owned instance; zero before its first initialize.
 * \note            Fields are implementation storage, not mutation controls.
 *                  One execution owner serializes every call and lifecycle.
 *                  Callbacks cannot dispatch or initialize this same instance;
 *                  such calls return BUSY. Separate instances are independent.
 *                  No allocation, queue, task, timer or implicit clock exists.
 */
typedef struct {
    const nx_fsm_config_t* config;
    void* context;
    size_t current;
    bool busy;
} nx_fsm_t;

/**
 * \brief           Validate tables and enter one explicit initial state.
 * \param[in,out]   machine: Zero or idle caller-owned instance.
 * \param[in]       config: Stable immutable tables, distinct from machine.
 * \param[in]       context: Borrowed callback context, which may be NULL.
 * \param[in]       initial: State index below state_count.
 * \return          SUCCESS; INVALID for invalid tables or index; BUSY during
 *                  a callback. Rejection preserves the machine and runs no
 *                  callbacks.
 * \note            Initial entry receives a NULL event. Reinitializing an idle
 *                  instance replaces its configuration without an exit action.
 *                  This is an explicit reset, with no retained event to drain.
 *                  Work is bounded by table size and the one entry callback;
 *                  callbacks must not block and have no failure/rollback path.
 */
nx_result_t nx_fsm_initialize(nx_fsm_t* machine, const nx_fsm_config_t* config,
                              void* context, size_t initial);

/**
 * \brief           Consume one event using the first matching enabled row.
 * \param[in,out]   machine: Initialized, externally serialized instance.
 * \param[in]       event: Borrowed immutable event and payload for this call.
 * \return          SUCCESS when a row ran; EMPTY when no row passed; INVALID
 *                  for invalid arguments; STATE for an uninitialized/corrupt
 *                  instance; BUSY on same-instance callback reentry.
 * \note            Nonzero payload size requires a non-NULL payload. Guards
 *                  run in table order after source/event matching. External
 *                  transitions run source exit, transition action, then target
 *                  entry. Exit/action observe the source state; entry observes
 *                  the target. Internal transitions run only their action.
 *                  There are no automatic/completion or ancestor transitions.
 *                  EMPTY and rejected dispatch leave the machine unchanged;
 *                  external callback effects cannot be rolled back.
 */
nx_result_t nx_fsm_dispatch(nx_fsm_t* machine, const nx_fsm_event_t* event);

#ifdef __cplusplus
}
#endif

#endif
