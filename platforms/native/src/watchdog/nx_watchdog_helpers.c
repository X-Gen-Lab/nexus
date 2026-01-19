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
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/*---------------------------------------------------------------------------*/
/* System Time Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get current system time in milliseconds
 */
uint64_t watchdog_get_system_time_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;

    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    /* Convert 100-nanosecond intervals to milliseconds */
    return uli.QuadPart / 10000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
#endif
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
