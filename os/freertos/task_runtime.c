/**
 * \file            task_runtime.c
 * \brief           Explicit FreeRTOS bootstrap and task execution budgets
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "nexus/arch/arch.h"
#include "nexus/arch/atomic.h"
#include "nexus/os/task.h"
#include "private/context.h"
#include "private/deadline.h"

static uint32_t s_kernel_entered;

/** \brief The caller explicitly enters one cold kernel, never Platform init. */
nx_result_t nx_os_run(void (*startup)(void*), void* context) {
    if (startup == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || !nx_arch_is_privileged() ||
        nx_arch_irq_is_masked() ||
        xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return NX_ERROR_CONTEXT;
    }
#if defined(__arm__) || defined(__thumb__)
    uint32_t control;
    __asm__ volatile("mrs %0, control" : "=r"(control));
    if ((control & 7U) != 0U) {
        return NX_ERROR_CONTEXT;
    }
#endif
    uint32_t expected = 0U;
    if (!nx_atomic_u32_compare_exchange_acq_rel(&s_kernel_entered, &expected,
                                                1U)) {
        return NX_ERROR_INVALID;
    }
    startup(context);
    vTaskStartScheduler();
    /* Unexpected kernel return does not establish task/stack quiescence. */
    nx_freertos_assert_failed(__FILE__, (unsigned)__LINE__);
    for (;;) {
    }
}

/** \brief Physical consumers retain the existing Core clock epoch. */
nx_time_us_t nx_os_now_us(void) {
    return nx_time_now_us();
}

/** \brief Sleep against one deadline rather than restarting relative ticks. */
nx_result_t nx_sleep_until(nx_time_us_t deadline) {
    if (!nx_freertos_task_context_allowed() ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    for (;;) {
        TickType_t ticks = nx_freertos_deadline_ticks(deadline);
        if (ticks == 0U) {
            return NX_SUCCESS;
        }
        vTaskDelay(ticks);
    }
}

/** \brief Yield is a scheduling hint, with no lower-priority progress promise.
 */
nx_result_t nx_task_yield(void) {
    if (!nx_freertos_task_context_allowed() ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    taskYIELD();
    return NX_SUCCESS;
}
