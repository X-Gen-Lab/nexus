/**
 * \file            posix_idle.c
 * \brief           Cooperative idle for the real FreeRTOS POSIX host fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "FreeRTOS.h"
#include <errno.h>
#include <stdlib.h>
#include <time.h>

void vApplicationIdleHook(void);

/**
 * \brief           Release the host CPU while the kernel has no ready work.
 * \note            This host-only sleep leaves the POSIX port's tick signal
 *                  enabled. It also provides a runtime interception point for
 *                  sanitizers that defer asynchronous signals while executing
 *                  ordinary code. Normal and sanitized fixtures use the same
 *                  hook; no kernel instrumentation or checks are disabled.
 *                  This does not model MCU idle power or interrupt latency.
 */
void vApplicationIdleHook(void) {
    const struct timespec interval = {.tv_sec = 0, .tv_nsec = 1000000L};
    if (nanosleep(&interval, NULL) != 0 && errno != EINTR) {
        abort();
    }
}
