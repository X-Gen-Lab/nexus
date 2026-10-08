#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
/**
 * \file            nx_watchdog_helpers.c
 * \brief           Watchdog helper functions implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements helper functions for watchdog timer simulation
 *                  including system time retrieval, timeout checking, and
 *                  state management.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "nx_watchdog_helpers.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/*---------------------------------------------------------------------------*/
/* System Time Functions                                                     */
/*---------------------------------------------------------------------------*/

/* Production monotonic clock with an explicit link-time injection point for
 * deterministic tests. No production symbol depends on test-only objects. */
NX_WEAK uint64_t nx_native_monotonic_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&counter))
        abort();
    return (uint64_t)(counter.QuadPart/frequency.QuadPart)*1000U +
           (uint64_t)(counter.QuadPart%frequency.QuadPart)*1000U/(uint64_t)frequency.QuadPart;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC,&now)!=0) abort();
    return (uint64_t)now.tv_sec*1000U+(uint64_t)now.tv_nsec/1000000U;
#endif
}
uint64_t watchdog_get_system_time_ms(void) {
    return nx_native_monotonic_time_ms();
}

/*---------------------------------------------------------------------------*/
/* Watchdog Timeout Functions                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Check if watchdog has timed out
 */
bool watchdog_check_timeout(nx_watchdog_state_t* state) {
    if (!state || !state->running) {
        return false;
    }

    uint64_t current_time = watchdog_get_system_time_ms();
    uint64_t elapsed = current_time - state->last_feed_time_ms;

    return elapsed >= state->config.timeout_ms;
}

/*---------------------------------------------------------------------------*/
/* State Management Functions                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset watchdog state for testing
 */
void watchdog_reset_state(nx_watchdog_state_t* state) {
    if (!state) {
        return;
    }

    /* Reset state */
    state->running = false;
    state->last_feed_time_ms = 0;
    state->callback = NULL;
    state->user_data = NULL;
    state->initialized = false;
    state->suspended = false;

    /* Reset statistics */
    memset(&state->stats, 0, sizeof(state->stats));
}
