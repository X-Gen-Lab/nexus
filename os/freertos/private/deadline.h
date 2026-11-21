/**
 * \file            deadline.h
 * \brief           One FreeRTOS conversion of absolute monotonic budgets
 * \author          Nexus Team
 */
#ifndef NEXUS_FREERTOS_PRIVATE_DEADLINE_H
#define NEXUS_FREERTOS_PRIVATE_DEADLINE_H
#include "FreeRTOS.h"
#include "nexus/core/time.h"

/** \brief Round remaining time up, reserving the native infinite sentinel. */
static TickType_t nx_freertos_deadline_ticks(nx_time_us_t deadline) {
    nx_time_us_t now = nx_time_now_us();
    if (deadline != NX_DEADLINE_NEVER && now >= deadline) {
        return 0;
    }
    uint64_t remaining =
        deadline == NX_DEADLINE_NEVER ? UINT64_MAX : deadline - now;
    uint64_t seconds = remaining / 1000000U;
    uint64_t maximum = (uint64_t)portMAX_DELAY - 1U;
    if (seconds > maximum / configTICK_RATE_HZ) {
        return (TickType_t)maximum;
    }
    uint64_t ticks = seconds * configTICK_RATE_HZ;
    ticks += ((remaining % 1000000U) * configTICK_RATE_HZ + 999999U) / 1000000U;
    return (TickType_t)(ticks > maximum ? maximum : ticks);
}
#endif /* NEXUS_FREERTOS_PRIVATE_DEADLINE_H */
