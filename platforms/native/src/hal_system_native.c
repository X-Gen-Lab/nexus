/**
 * \file            hal_system_native.c
 * \brief           Native Platform System HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

/* Enable POSIX features for clock_gettime and usleep */
#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE
#endif

#include "hal/hal_def.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

static uint32_t start_time_ms = 0;

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get current time in milliseconds
 * \return          Current time in ms
 */
static uint32_t get_time_ms(void) {
#ifdef _WIN32
    return GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_system_init(void) {
    start_time_ms = get_time_ms();
    return HAL_OK;
}

uint32_t hal_get_tick(void) {
    return get_time_ms() - start_time_ms;
}

void hal_delay_ms(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void hal_delay_us(uint32_t us) {
#ifdef _WIN32
    /* Windows doesn't have microsecond sleep, use busy wait */
    uint32_t start = get_time_ms();
    uint32_t target_ms = us / 1000;
    if (target_ms == 0) {
        target_ms = 1;
    }
    while ((get_time_ms() - start) < target_ms) {
        /* Busy wait */
    }
#else
    usleep(us);
#endif
}

void hal_system_reset(void) {
    /* Cannot reset on native platform */
}

uint32_t hal_enter_critical(void) {
    /* No-op on native platform */
    return 0;
}

void hal_exit_critical(uint32_t state) {
    (void)state;
    /* No-op on native platform */
}
