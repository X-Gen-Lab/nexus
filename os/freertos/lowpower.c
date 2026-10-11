/**
 * \file            lowpower.c
 * \brief           Measured tick suppression with bounded catch-up
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/lowpower.h"
#include "FreeRTOS.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/sleep.h"
#include "task.h"
#include <stddef.h>

__attribute__((weak)) const nx_freertos_lowpower_port_t*
nx_freertos_lowpower_port(void) {
    return NULL;
}

#if configUSE_TICKLESS_IDLE != 0
_Static_assert(configTICK_RATE_HZ > 0 && 1000000u % configTICK_RATE_HZ == 0,
               "The tickless clock requires exact microsecond tick periods");

/** \brief Keep all timer effects behind a complete explicit implementation. */
static bool lowpower_port_valid(const nx_freertos_lowpower_port_t* port) {
    return port != NULL && port->now_us != NULL && port->pause_tick != NULL &&
           port->arm_deadline != NULL && port->disarm != NULL &&
           port->resume_tick != NULL;
}
#endif

void nx_freertos_suppress_ticks_and_sleep(uint32_t expected_idle_ticks) {
#if configUSE_TICKLESS_IDLE != 0
    const nx_freertos_lowpower_port_t* port = nx_freertos_lowpower_port();
    if (!lowpower_port_valid(port) || expected_idle_ticks < 2u) {
        return;
    }
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0) {
        return;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask != 0 || masks.basepri != 0 || masks.faultmask != 0) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (eTaskConfirmSleepModeStatus() == eAbortSleep) {
        nx_arch_irq_restore(saved);
        return;
    }
    nx_freertos_tick_snapshot_t snapshot;
    if (port->pause_tick(port->context, &snapshot) != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return;
    }
    const uint32_t period_us = 1000000u / configTICK_RATE_HZ;
    /* A broken clock/phase contract cannot be repaired by inventing a tick
     * epoch. Keep masks held and use the maintained fail-stop instead. */
    configASSERT(snapshot.phase_us < period_us);
    uint64_t idle_us =
        (uint64_t)expected_idle_ticks * period_us - snapshot.phase_us;
    configASSERT(snapshot.time_us <= UINT64_MAX - idle_us);
    uint64_t deadline_us = snapshot.time_us + idle_us;
    if (port->arm_deadline(port->context, deadline_us) == NX_SUCCESS) {
        (void)nx_arch_wait_for_interrupt();
        port->disarm(port->context);
    }
    uint64_t now_us = port->now_us(port->context);
    configASSERT(now_us >= snapshot.time_us);
    uint64_t elapsed_us = now_us - snapshot.time_us;
    configASSERT(elapsed_us <= UINT64_MAX - snapshot.phase_us);
    uint64_t whole_ticks = (elapsed_us + snapshot.phase_us) / period_us;
    configASSERT(whole_ticks <=
                 (uint64_t)expected_idle_ticks + NX_FREERTOS_LATE_TICK_BUDGET);
    uint64_t next_us = (whole_ticks + 1u) * period_us - snapshot.phase_us;
    configASSERT(snapshot.time_us <= UINT64_MAX - next_us);
    /* Stepping stops before the first potentially unblocked task. The final
     * due tick and up to eight late ticks are pended through the real kernel
     * primitive while the scheduler remains suspended, preserving unblock
     * behavior without an unbounded catch-up loop. */
    uint32_t step_ticks = whole_ticks < expected_idle_ticks
                              ? (uint32_t)whole_ticks
                              : expected_idle_ticks - 1u;
    if (step_ticks != 0) {
        vTaskStepTick((TickType_t)step_ticks);
    }
    uint32_t pending_ticks = (uint32_t)(whole_ticks - step_ticks);
    for (uint32_t index = 0; index < pending_ticks; ++index) {
        (void)xTaskIncrementTick();
    }
    port->resume_tick(port->context, snapshot.time_us + next_us);
    nx_arch_irq_restore(saved);
#else
    (void)expected_idle_ticks;
#endif
}
