/**
 * \file            timers.h
 * \brief           Caller-owned bounded deadline container without workers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_TIMERS_H
#define NEXUS_COMPONENTS_TIMERS_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Stable identity for one arm; old identities never cancel a new arm.
 */
typedef struct {
    size_t slot;
    uint64_t epoch;
} nx_timer_ticket_t;
/** \brief Exact supplied storage; only container operations mutate it. */
typedef struct {
    nx_time_us_t deadline;
    uint64_t epoch;
    uint64_t order;
    uintptr_t tag;
    bool active;
} nx_timer_slot_t;
/** \brief Single-owner O(capacity) profile with no callback or internal queue.
 */
typedef struct {
    nx_timer_slot_t* slots;
    size_t capacity;
    uint64_t next_order;
    nx_time_us_t last_observed;
    bool initialized;
} nx_timers_t;
/** \brief Copied due event; no pointer to mutable slot storage. */
typedef struct {
    nx_timer_ticket_t ticket;
    nx_time_us_t deadline;
    uintptr_t tag;
} nx_timer_event_t;
/**
 * \brief           Initialize unused container and exact slot storage.
 * \param[out]      timers: Fresh single-owner state, no retained old controls.
 * \param[in,out]   slots: Distinct writable capacity slots, retained until
 * join. \param[in]       capacity: Positive actual count; no fixed maximum
 * pool. \param[in]       now: Finite initial timestamp in the selected Core
 * domain. \return          SUCCESS or INVALID; no clock read/OS/allocation.
 */
nx_result_t nx_timers_initialize(nx_timers_t* timers, nx_timer_slot_t* slots,
                                 size_t capacity, nx_time_us_t now);
/**
 * \brief           Arm one exact finite deadline, preserving same-deadline
 * FIFO. \param[in,out]   timers: Initialized unique-owner container. \param[in]
 * deadline: Finite absolute deadline, including past values. \param[in] tag:
 * Copied application value; pointer lifetime stays caller's. \param[out]
 * ticket: Distinct stable control, unchanged on rejection. \return SUCCESS,
 * INVALID/STATE or EXHAUSTED for capacity/identity. \note O(capacity), no
 * worker. Epoch/order exhaustion never wraps.
 */
nx_result_t nx_timers_arm(nx_timers_t* timers, nx_time_us_t deadline,
                          uintptr_t tag, nx_timer_ticket_t* ticket);
/**
 * \brief           Cancel an exact active identity without invoking a callback.
 * \param[in,out]   timers: Same initialized serialized container.
 * \param[in]       ticket: Identity returned from a successful arm.
 * \return          SUCCESS, INVALID or STATE for stale/inactive identity.
 */
nx_result_t nx_timers_cancel(nx_timers_t* timers, nx_timer_ticket_t ticket);
/**
 * \brief           Peek earliest finite due value without consuming it.
 * \param[in]       timers: Live serialized container, borrowed until return.
 * \param[out]      out: Distinct result, unchanged when empty or rejected.
 * \return          SUCCESS, EMPTY or INVALID/STATE; O(capacity), no clock read.
 */
nx_result_t nx_timers_next_due(const nx_timers_t* timers, nx_time_us_t* out);
/**
 * \brief           Remove at most one due item; equal deadlines preserve arm
 * order. \param[in,out]   timers: Initialized unique-owner container.
 * \param[in]       now: Finite nondecreasing snapshot in the selected epoch.
 * \param[out]      out: Distinct copied event, unchanged on EMPTY/error.
 * \return          SUCCESS, EMPTY or INVALID/STATE; clock reversal changes
 * none. \note            Caller dispatches events through existing
 * FSM/queue/owner. EMPTY may advance last_observed; there is no hidden worker.
 */
nx_result_t nx_timers_pop_due(nx_timers_t* timers, nx_time_us_t now,
                              nx_timer_event_t* out);
/**
 * \brief           Cancel all arms and explicitly admit a new clock epoch.
 * \param[in,out]   timers: Unique-owner container, all former users joined.
 * \param[in]       now: Finite first timestamp in the replacement domain.
 * \return          SUCCESS or INVALID/STATE; retains epoch/order counters.
 * \note            Old tickets remain invalid; reset does not recycle identity.
 */
nx_result_t nx_timers_reset(nx_timers_t* timers, nx_time_us_t now);
#ifdef __cplusplus
}
#endif
#endif
