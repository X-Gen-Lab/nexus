/**
 * \file            usb_hal_time.c
 * \brief           Selected USB HAL delays use the existing Core time domain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"

extern nx_stm32_system_t g_nx_stm32_system;
void HAL_Delay(uint32_t milliseconds);

/** \brief The void official delay cannot report clock failure as success. */
static _Noreturn void clock_failed(void) {
    for (;;) {
        nx_arch_dsb();
    }
}

/** \brief HAL PHY waits neither install SysTick nor create an executor.
 * \note Invalid, backward or persistently frozen Core time parks this caller.
 *       No successful delay or release is reported; external fault/reset
 *       policy owns recovery and all existing cold receipts remain retained.
 */
void HAL_Delay(uint32_t milliseconds) {
    if (milliseconds == 0U)
        return;
    nx_time_us_t previous = nx_time_now_us();
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(previous, (uint64_t)milliseconds * 1000U,
                                 &deadline) != NX_SUCCESS)
        clock_failed();
    /* A live microsecond timer advances long before this finite CPU budget.
     * Preemption does not consume iterations; any real progress replenishes
     * it. RCC/CEN live bits alone cannot prove that CNT still advances. */
    const uint32_t budget = NEXUS_CORE_HZ / 1000U + 1U;
    uint32_t remaining = budget;
    for (;;) {
        nx_time_us_t now = nx_time_now_us();
        if (nx_stm32_system_clock_read(&g_nx_stm32_system) == UINT64_MAX ||
            now < previous)
            clock_failed();
        if (nx_deadline_expired(deadline, now))
            return;
        if (now == previous) {
            if (remaining == 0U)
                clock_failed();
            --remaining;
        } else {
            previous = now;
            remaining = budget;
        }
    }
}
