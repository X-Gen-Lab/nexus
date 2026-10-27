/**
 * \file            rtc_lptim.c
 * \brief           Independent raw-calendar and finite-shot Native providers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "rtc_lptim_internal.h"
#include "rtc_lptim_provider.h"

/** \brief Full private-owner validation precedes any state read. */
static bool rtc_owner(const nx_native_rtc_state_t* state) {
    return nx_time_span(state, sizeof(*state), _Alignof(nx_native_rtc_state_t));
}
/** \brief Full private-owner validation precedes any state read. */
static bool lptim_owner(const nx_native_lptim_state_t* state) {
    return nx_time_span(state, sizeof(*state),
                        _Alignof(nx_native_lptim_state_t));
}
/** \brief Protect complete known owner and borrowed-request spans. */
static bool rtc_argument(const nx_native_rtc_state_t* state,
                         const void* pointer, size_t size, size_t alignment,
                         bool protect_active) {
    return nx_time_span(pointer, size, alignment) &&
           !nx_time_overlap(pointer, size, state, sizeof(*state)) &&
           (!protect_active || state->active == NULL ||
            !nx_time_overlap(pointer, size, state->active,
                             sizeof(*state->active)));
}
/** \brief Opaque clock context extent remains a cold assembly obligation. */
static bool lptim_argument(const nx_native_lptim_state_t* state,
                           const void* pointer, size_t size, size_t alignment,
                           bool protect_active) {
    return nx_time_span(pointer, size, alignment) &&
           !nx_time_overlap(pointer, size, state, sizeof(*state)) &&
           (!protect_active || state->active == NULL ||
            !nx_time_overlap(pointer, size, state->active,
                             sizeof(*state->active)));
}
/** \brief Single fresh construction copies a face but never reads its clock. */
nx_result_t nx_native_rtc_construct(nx_native_rtc_state_t* state,
                                    const nx_clock_t* clock) {
    if (!rtc_owner(state) ||
        !nx_time_span(clock, sizeof(*clock), _Alignof(nx_clock_t)) ||
        nx_time_overlap(clock, sizeof(*clock), state, sizeof(*state)) ||
        clock->read == NULL) {
        return NX_ERROR_INVALID;
    }
    if (state->constructed) {
        return NX_ERROR_STATE;
    }
    nx_native_rtc_state_t fresh = {0};
    fresh.clock = *clock;
    fresh.generation = 1U;
    fresh.constructed = true;
    fresh.opened = true;
    *state = fresh;
    return NX_SUCCESS;
}
/** \brief Fixed positive units belong to this distinct static owner. */
nx_result_t nx_native_lptim_construct(nx_native_lptim_state_t* state,
                                      const nx_clock_t* clock,
                                      uint32_t tick_hz) {
    if (!lptim_owner(state) ||
        !nx_time_span(clock, sizeof(*clock), _Alignof(nx_clock_t)) ||
        nx_time_overlap(clock, sizeof(*clock), state, sizeof(*state)) ||
        clock->read == NULL || tick_hz == 0U) {
        return NX_ERROR_INVALID;
    }
    if (state->constructed) {
        return NX_ERROR_STATE;
    }
    nx_native_lptim_state_t fresh = {0};
    fresh.clock = *clock;
    fresh.generation = 1U;
    fresh.tick_hz = tick_hz;
    fresh.constructed = true;
    fresh.opened = true;
    *state = fresh;
    return NX_SUCCESS;
}
/** \brief Reopen only after all borrowed/source responsibility is joined. */
nx_result_t nx_native_rtc_start(nx_native_rtc_state_t* state) {
    if (!rtc_owner(state)) {
        return NX_ERROR_INVALID;
    }
    if (state->active != NULL || state->armed || state->pending ||
        state->terminal) {
        return NX_ERROR_BUSY;
    }
    if (!state->constructed || state->opened) {
        return NX_ERROR_STATE;
    }
    if (state->generation == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    state->generation++;
    state->opened = true;
    return NX_SUCCESS;
}
/** \brief Preserve fixed clock/rate/fault facts and never reset shot epochs. */
nx_result_t nx_native_lptim_start(nx_native_lptim_state_t* state) {
    if (!lptim_owner(state)) {
        return NX_ERROR_INVALID;
    }
    if (state->active != NULL || state->armed || state->pending ||
        state->terminal) {
        return NX_ERROR_BUSY;
    }
    if (!state->constructed || state->opened) {
        return NX_ERROR_STATE;
    }
    if (state->generation == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    state->generation++;
    state->opened = true;
    return NX_SUCCESS;
}
/** \brief Keep owner lifecycle selection in the tested provider. */
nx_result_t nx_native_rtc_prepare(nx_native_rtc_state_t* state,
                                  const nx_clock_t* clock) {
    if (!rtc_owner(state)) {
        return NX_ERROR_INVALID;
    }
    return state->constructed ? nx_native_rtc_start(state)
                              : nx_native_rtc_construct(state, clock);
}
/** \brief Never reset the fixed source binding or overwrite retained loans. */
nx_result_t nx_native_lptim_prepare(nx_native_lptim_state_t* state,
                                    const nx_clock_t* clock, uint32_t tick_hz) {
    if (!lptim_owner(state)) {
        return NX_ERROR_INVALID;
    }
    return state->constructed
               ? nx_native_lptim_start(state)
               : nx_native_lptim_construct(state, clock, tick_hz);
}
/** \brief Copy a coherent setting without an implicit clock or date policy. */
static nx_result_t rtc_read(const void* context, nx_rtc_snapshot_t* snapshot) {
    const nx_native_rtc_state_t* state = context;
    if (!rtc_owner(state) || !rtc_argument(state, snapshot, sizeof(*snapshot),
                                           _Alignof(nx_rtc_snapshot_t), true)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed || !state->opened || !state->valid) {
        return NX_ERROR_STATE;
    }
    nx_rtc_snapshot_t value = {.domain = {state, state->generation},
                               .calendar = state->calendar};
    *snapshot = value;
    return NX_SUCCESS;
}
/** \brief Setting changes identity, never Core time or an active alarm. */
static nx_result_t rtc_set(void* context, const nx_rtc_calendar_t* calendar) {
    nx_native_rtc_state_t* state = context;
    if (!rtc_owner(state) ||
        !rtc_argument(state, calendar, sizeof(*calendar),
                      _Alignof(nx_rtc_calendar_t), true) ||
        nx_rtc_calendar_validate(calendar) != NX_SUCCESS) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed || !state->opened) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (state->generation == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_rtc_calendar_t value = *calendar;
    state->generation++;
    state->calendar = value;
    state->valid = true;
    return NX_SUCCESS;
}
/** \brief No current-second ambiguity is admitted as a new future alarm. */
static bool alarm_matches(const nx_rtc_calendar_t* calendar,
                          const nx_rtc_alarm_t* alarm) {
    return calendar->day == alarm->day && calendar->hour == alarm->hour &&
           calendar->minute == alarm->minute &&
           calendar->second == alarm->second;
}
/** \brief All static checks precede even the external deadline read. */
static nx_result_t rtc_submit(void* context, nx_rtc_request_t* request) {
    nx_native_rtc_state_t* state = context;
    if (!rtc_owner(state) || !rtc_argument(state, request, sizeof(*request),
                                           _Alignof(nx_rtc_request_t), false)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed || !state->opened || !state->valid) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        request->domain.identity != state ||
        request->domain.generation != state->generation) {
        return NX_ERROR_STATE;
    }
    if (nx_rtc_alarm_validate(&request->alarm) != NX_SUCCESS) {
        return NX_ERROR_INVALID;
    }
    if (alarm_matches(&state->calendar, &request->alarm)) {
        return NX_ERROR_STATE;
    }
    if (request->base.deadline != NX_DEADLINE_NEVER &&
        nx_deadline_expired(request->base.deadline,
                            state->clock.read(state->clock.context))) {
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t status = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (status != NX_SUCCESS) {
        return status;
    }
    state->alarm = request->alarm;
    state->active = request;
    state->terminal = false;
    state->pending = false;
    state->armed = true;
    return NX_SUCCESS;
}
/** \brief Hold retains the source and request; publication is always last. */
static nx_result_t rtc_withdraw(nx_native_rtc_state_t* state,
                                nx_result_t cause) {
    if (!state->terminal) {
        state->terminal = true;
        state->terminal_result = cause;
    }
    if (state->armed && state->hold_withdraw) {
        if (nx_request_state(&state->active->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&state->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return NX_ERROR_BUSY;
    }
    nx_rtc_request_t* request = state->active;
    nx_result_t result = state->terminal_result;
    state->armed = false;
    state->pending = false;
    state->active = NULL;
    state->terminal = false;
    state->terminal_result = NX_SUCCESS;
    nx_request_settle(&request->base, result, result == NX_SUCCESS ? 1U : 0U);
    return NX_SUCCESS;
}
/** \brief Numeric identity checks occur before any foreign request access. */
static nx_result_t rtc_cancel(void* context, nx_rtc_request_t* request) {
    nx_native_rtc_state_t* state = context;
    if (!rtc_owner(state)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed || request == NULL || state->active != request) {
        return NX_ERROR_STATE;
    }
    (void)rtc_withdraw(state, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
/** \brief A prior terminal reason bypasses clock reads and cannot be replaced.
 */
static nx_result_t rtc_service(void* context) {
    nx_native_rtc_state_t* state = context;
    if (!rtc_owner(state)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed) {
        return NX_ERROR_STATE;
    }
    if (state->active == NULL) {
        return NX_SUCCESS;
    }
    if (state->terminal) {
        return rtc_withdraw(state, state->terminal_result);
    }
    nx_time_us_t deadline = state->active->base.deadline;
    if (deadline != NX_DEADLINE_NEVER &&
        nx_deadline_expired(deadline,
                            state->clock.read(state->clock.context))) {
        return rtc_withdraw(state, NX_ERROR_TIMEOUT);
    }
    return state->pending ? rtc_withdraw(state, NX_SUCCESS) : NX_ERROR_BUSY;
}
/** \brief Closing does not erase the permanent construction/generation marker.
 */
static nx_result_t rtc_stop(void* context) {
    nx_native_rtc_state_t* state = context;
    if (!rtc_owner(state)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed) {
        return NX_ERROR_STATE;
    }
    state->opened = false;
    if (state->active != NULL) {
        return rtc_withdraw(state, NX_ERROR_CANCELLED);
    }
    return state->armed || state->pending || state->terminal ? NX_ERROR_BUSY
                                                             : NX_SUCCESS;
}
/** \brief One modulo sample, not a wrap-extension or elapsed-time estimate. */
static nx_result_t lptim_sample(const void* context,
                                nx_lptim_sample_t* sample) {
    const nx_native_lptim_state_t* state = context;
    if (!lptim_owner(state) ||
        !lptim_argument(state, sample, sizeof(*sample),
                        _Alignof(nx_lptim_sample_t), true)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed || !state->opened) {
        return NX_ERROR_STATE;
    }
    nx_lptim_sample_t value = {.domain = {state, state->generation},
                               .tick_hz = state->tick_hz,
                               .counter = (uint16_t)(state->counted & 65535U),
                               .running = state->armed};
    *sample = value;
    return NX_SUCCESS;
}
/** \brief Reserve final epoch capacity before any accepted loan or clock read.
 */
static nx_result_t lptim_submit(void* context, nx_lptim_request_t* request) {
    nx_native_lptim_state_t* state = context;
    if (!lptim_owner(state) ||
        !lptim_argument(state, request, sizeof(*request),
                        _Alignof(nx_lptim_request_t), false)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed || !state->opened) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        request->domain.identity != state ||
        request->domain.generation != state->generation) {
        return NX_ERROR_STATE;
    }
    if (request->ticks == 0U || request->ticks > 65536U) {
        return NX_ERROR_INVALID;
    }
    if (state->generation == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    if (request->base.deadline != NX_DEADLINE_NEVER &&
        nx_deadline_expired(request->base.deadline,
                            state->clock.read(state->clock.context))) {
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t status = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (status != NX_SUCCESS) {
        return status;
    }
    state->limit = request->ticks;
    state->counted = 0U;
    state->active = request;
    state->terminal = false;
    state->pending = false;
    state->armed = true;
    return NX_SUCCESS;
}
/** \brief Final progress is observed at withdrawal, not at first cause latch.
 */
static nx_result_t lptim_withdraw(nx_native_lptim_state_t* state,
                                  nx_result_t cause) {
    if (!state->terminal) {
        state->terminal = true;
        state->terminal_result = cause;
    }
    if (state->armed && state->hold_withdraw) {
        if (nx_request_state(&state->active->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&state->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return NX_ERROR_BUSY;
    }
    nx_lptim_request_t* request = state->active;
    nx_result_t result = state->terminal_result;
    uint32_t counted = state->counted;
    state->armed = false;
    state->pending = false;
    state->active = NULL;
    state->terminal = false;
    state->terminal_result = NX_SUCCESS;
    state->limit = 0U;
    state->counted = 0U;
    state->generation++;
    nx_request_settle(&request->base, result, counted);
    return NX_SUCCESS;
}
/** \brief Reject a foreign pointer without reading its storage or domain. */
static nx_result_t lptim_cancel(void* context, nx_lptim_request_t* request) {
    nx_native_lptim_state_t* state = context;
    if (!lptim_owner(state)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed || request == NULL || state->active != request) {
        return NX_ERROR_STATE;
    }
    (void)lptim_withdraw(state, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
/** \brief Expiry has observation priority over an unobserved source event. */
static nx_result_t lptim_service(void* context) {
    nx_native_lptim_state_t* state = context;
    if (!lptim_owner(state)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed) {
        return NX_ERROR_STATE;
    }
    if (state->active == NULL) {
        return NX_SUCCESS;
    }
    if (state->terminal) {
        return lptim_withdraw(state, state->terminal_result);
    }
    nx_time_us_t deadline = state->active->base.deadline;
    if (deadline != NX_DEADLINE_NEVER &&
        nx_deadline_expired(deadline,
                            state->clock.read(state->clock.context))) {
        return lptim_withdraw(state, NX_ERROR_TIMEOUT);
    }
    return state->pending ? lptim_withdraw(state, NX_SUCCESS) : NX_ERROR_BUSY;
}
/** \brief Failed withdrawal retains both the permanent owner and live request.
 */
static nx_result_t lptim_stop(void* context) {
    nx_native_lptim_state_t* state = context;
    if (!lptim_owner(state)) {
        return NX_ERROR_INVALID;
    }
    if (!state->constructed) {
        return NX_ERROR_STATE;
    }
    state->opened = false;
    if (state->active != NULL) {
        return lptim_withdraw(state, NX_ERROR_CANCELLED);
    }
    return state->armed || state->pending || state->terminal ? NX_ERROR_BUSY
                                                             : NX_SUCCESS;
}
const nx_rtc_ops_t nx_native_rtc_ops = {rtc_read,   rtc_set,     rtc_submit,
                                        rtc_cancel, rtc_service, rtc_stop};
const nx_lptim_ops_t nx_native_lptim_ops = {
    lptim_sample, lptim_submit, lptim_cancel, lptim_service, lptim_stop};

/** \brief Model hooks recognize exact Native faces, not a mutable registry. */
static nx_result_t rtc_model_owner(const nx_rtc_port_t* port,
                                   nx_native_rtc_state_t** state) {
    if (!nx_time_span(port, sizeof(*port), _Alignof(nx_rtc_port_t)) ||
        port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops != &nx_native_rtc_ops) {
        return NX_ERROR_UNSUPPORTED;
    }
    *state = port->context;
    if (!rtc_owner(*state)) {
        return NX_ERROR_INVALID;
    }
    return (*state)->constructed ? NX_SUCCESS : NX_ERROR_STATE;
}
/** \brief No state layout is imposed on unrelated substitute providers. */
static nx_result_t lptim_model_owner(const nx_lptim_port_t* port,
                                     nx_native_lptim_state_t** state) {
    if (!nx_time_span(port, sizeof(*port), _Alignof(nx_lptim_port_t)) ||
        port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops != &nx_native_lptim_ops) {
        return NX_ERROR_UNSUPPORTED;
    }
    *state = port->context;
    if (!lptim_owner(*state)) {
        return NX_ERROR_INVALID;
    }
    return (*state)->constructed ? NX_SUCCESS : NX_ERROR_STATE;
}
/** \brief Advance exactly one valid date, keeping weekday independent. */
static void rtc_next_day(nx_rtc_calendar_t* calendar) {
    calendar->weekday = (uint8_t)(calendar->weekday % 7U + 1U);
    if (calendar->day < nx_time_month_days(calendar->year, calendar->month)) {
        calendar->day++;
    } else {
        calendar->day = 1U;
        if (calendar->month < 12U) {
            calendar->month++;
        } else {
            calendar->month = 1U;
            calendar->year = (uint8_t)((calendar->year + 1U) % 100U);
        }
    }
}
/** \brief At most two day intervals are examined; no per-second loop. */
nx_result_t nx_native_rtc_model_advance(const nx_rtc_port_t* port,
                                        uint32_t seconds) {
    nx_native_rtc_state_t* state;
    nx_result_t status = rtc_model_owner(port, &state);
    if (status != NX_SUCCESS) {
        return status;
    }
    if (seconds > 86400U) {
        return NX_ERROR_INVALID;
    }
    if (!state->valid) {
        return NX_ERROR_STATE;
    }
    uint32_t start = (uint32_t)state->calendar.hour * 3600U +
                     (uint32_t)state->calendar.minute * 60U +
                     state->calendar.second;
    uint32_t end = start + seconds;
    nx_rtc_calendar_t next = state->calendar;
    bool carries = end >= 86400U;
    if (carries) {
        rtc_next_day(&next);
    }
    uint32_t within = carries ? end - 86400U : end;
    if (state->armed && !state->pending && seconds != 0U) {
        uint32_t match = (uint32_t)state->alarm.hour * 3600U +
                         (uint32_t)state->alarm.minute * 60U +
                         state->alarm.second;
        uint32_t first_end = carries ? 86399U : end;
        state->pending =
            (state->calendar.day == state->alarm.day && match > start &&
             match <= first_end) ||
            (carries && next.day == state->alarm.day && match <= within);
    }
    next.hour = (uint8_t)(within / 3600U);
    next.minute = (uint8_t)((within % 3600U) / 60U);
    next.second = (uint8_t)(within % 60U);
    state->calendar = next;
    return NX_SUCCESS;
}
/** \brief Removing an external fault never performs the production drain. */
nx_result_t nx_native_rtc_model_hold_withdraw(const nx_rtc_port_t* port,
                                              bool hold) {
    nx_native_rtc_state_t* state;
    nx_result_t status = rtc_model_owner(port, &state);
    if (status == NX_SUCCESS) {
        state->hold_withdraw = hold;
    }
    return status;
}
/** \brief Only source facts change; no active-request read or publication. */
nx_result_t nx_native_lptim_model_advance(const nx_lptim_port_t* port,
                                          uint32_t ticks) {
    nx_native_lptim_state_t* state;
    nx_result_t status = lptim_model_owner(port, &state);
    if (status != NX_SUCCESS) {
        return status;
    }
    if (ticks > 65536U) {
        return NX_ERROR_INVALID;
    }
    if (state->armed) {
        uint32_t remaining = state->limit - state->counted;
        state->counted += ticks < remaining ? ticks : remaining;
        state->pending = state->counted == state->limit;
    }
    return NX_SUCCESS;
}
/** \brief The external source can remain armed until real withdrawal succeeds.
 */
nx_result_t nx_native_lptim_model_hold_withdraw(const nx_lptim_port_t* port,
                                                bool hold) {
    nx_native_lptim_state_t* state;
    nx_result_t status = lptim_model_owner(port, &state);
    if (status == NX_SUCCESS) {
        state->hold_withdraw = hold;
    }
    return status;
}
