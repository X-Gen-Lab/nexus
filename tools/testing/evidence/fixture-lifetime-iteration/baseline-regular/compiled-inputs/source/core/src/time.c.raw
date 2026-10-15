/**
 * \file            time.c
 *
 * \brief           Deadline arithmetic and bounded hardware counter extension.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"

/** \brief Compare a finite deadline without unsigned subtraction ambiguity. */
bool nx_deadline_expired(nx_time_us_t deadline, nx_time_us_t now) {
    return deadline != NX_DEADLINE_NEVER && now >= deadline;
}

/** \brief Saturate overflowing duration arithmetic to the explicit sentinel. */
nx_time_us_t nx_deadline_after(nx_time_us_t now, uint64_t duration_us) {
    if (duration_us >= UINT64_MAX - now) {
        return NX_DEADLINE_NEVER;
    }
    return now + duration_us;
}

/** \brief Establish one stable-frequency counter domain. */
nx_result_t nx_clock32_initialize(nx_clock32_t* clock, uint32_t counter,
                                  uint32_t frequency_hz) {
    if (clock == NULL || frequency_hz == 0) {
        return NX_ERROR_INVALID;
    }
    clock->ticks = 0;
    clock->previous = counter;
    clock->frequency_hz = frequency_hz;
    clock->initialized = true;
    return NX_SUCCESS;
}

/** \brief Extend one wrap and convert without overflowing an intermediate. */
nx_result_t nx_clock32_observe(nx_clock32_t* clock, uint32_t counter,
                               nx_time_us_t* time_us) {
    if (clock == NULL || time_us == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!clock->initialized || clock->frequency_hz == 0) {
        return NX_ERROR_STATE;
    }
    uint32_t delta = counter - clock->previous;
    if (clock->ticks > UINT64_MAX - delta) {
        return NX_ERROR_EXHAUSTED;
    }
    uint64_t ticks = clock->ticks + delta;
    uint64_t seconds = ticks / clock->frequency_hz;
    uint64_t remainder = ticks % clock->frequency_hz;
    uint64_t fraction = remainder * UINT64_C(1000000) / clock->frequency_hz;
    if (seconds > (UINT64_MAX - 1 - fraction) / UINT64_C(1000000)) {
        return NX_ERROR_EXHAUSTED;
    }
    *time_us = seconds * UINT64_C(1000000) + fraction;
    clock->ticks = ticks;
    clock->previous = counter;
    return NX_SUCCESS;
}
