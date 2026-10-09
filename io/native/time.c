/**
 * \file            time.c
 *
 * \brief           Real monotonic host time and explicit deterministic
 *                  domains.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "nexus/io/native/model.h"
#include <stdlib.h>
#include <time.h>

static uint64_t s_manual_time;
static bool s_manual;

/** \brief Read host monotonic time; clock failure is never invented success. */
nx_time_us_t nx_time_now_us(void) {
    if (__atomic_load_n(&s_manual, __ATOMIC_ACQUIRE)) {
        return __atomic_load_n(&s_manual_time, __ATOMIC_ACQUIRE);
    }
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        abort();
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000) +
           (uint64_t)value.tv_nsec / 1000u;
}

/** \brief Switch domains only under the fixture's global quiescence contract.
 */
nx_result_t nx_native_clock_configure(bool enabled, nx_time_us_t now) {
    if (now == UINT64_MAX) {
        return NX_ERROR_INVALID;
    }
    __atomic_store_n(&s_manual_time, now, __ATOMIC_RELEASE);
    __atomic_store_n(&s_manual, enabled, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}

/** \brief Advance deterministic time without wrapping or inventing IRQ
 * progress. */
nx_result_t nx_native_clock_advance(uint64_t duration_us) {
    if (!__atomic_load_n(&s_manual, __ATOMIC_ACQUIRE)) {
        return NX_ERROR_STATE;
    }
    uint64_t current = __atomic_load_n(&s_manual_time, __ATOMIC_ACQUIRE);
    do {
        if (duration_us >= UINT64_MAX - current) {
            return NX_ERROR_EXHAUSTED;
        }
    } while (!__atomic_compare_exchange_n(&s_manual_time, &current,
                                          current + duration_us, false,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    return NX_SUCCESS;
}
