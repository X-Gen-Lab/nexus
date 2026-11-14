/**
 * \file            consumer.c
 * \brief           Explicit RTC wake and family power through public owner APIs
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Software link reference; no fitted LSE or hardware
 * execution.
 */
#include "nexus/components/clock_domain_owner.h"
#include "nexus/soc/stm32f4_power.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/* This single-thread integration has no external clock/deadline borrowers.
 * Real products must enroll and join every additional user before entry. */
static nx_clock_domain_t domain;
static nx_clock_barrier_t barrier;
static nx_rtc_standby_t standby;
static nx_stm32_stop_receipt_t stopped;
static const nx_stm32_power_options_t options = {.poll_limit = 1000000U,
                                                 .wake_events =
                                                     NX_STM32_WAKE_RTC_TIMER,
                                                 .standby = true};

int main(void) {
    const nx_rtc_port_t* rtc = nx_factory_rtc_ref(NX_RTC_REF_CALENDAR);
    if (rtc == NULL || nx_clock_domain_construct(&domain, 1U) != NX_SUCCESS)
        return 1;
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        return 2;
    nx_stm32_power_wake_status_t wake;
    if (nx_stm32_power_wake_status(&wake) != NX_SUCCESS)
        return 3;
    /* Never turn a retained flag into permission to reset or discard data.
     * F479 reset preparation and the subsequent reset are product decisions. */
    if (wake.rtc_reset_required || wake.wake_pending)
        return 4;
    nx_rtc_wakeup_t timer = {true, NX_RTC_WAKE_DIV16, 1U};
    if (nx_rtc_port_wakeup(rtc, &timer) != NX_SUCCESS ||
        nx_rtc_port_wake_route(rtc, NX_RTC_EVENT_WAKEUP) != NX_SUCCESS)
        return 5;
    if (nx_platform_suspend() != NX_SUCCESS)
        return 6;
    if (nx_rtc_port_standby_prepare(rtc, 0U, 1000000U, &standby) !=
            NX_SUCCESS ||
        nx_clock_domain_close(&domain, &barrier) != NX_SUCCESS)
        return 7;
    nx_result_t result =
        nx_stm32_standby_enter(barrier, &options, rtc, &standby, &stopped);
    /* A real Standby resets. A returning WFI must retain all failed cleanup
     * receipts and storage; this reference never restarts an old deadline. */
    if (result != NX_ERROR_IO || standby.owner != NULL || !stopped.waited)
        return 8;
    if (nx_clock_domain_reopen(barrier) != NX_SUCCESS ||
        nx_platform_resume().primary != NX_SUCCESS)
        return 9;
    if (nx_rtc_port_stop(rtc) != NX_SUCCESS)
        return 10;
    nx_rtc_backup_handoff_t handoff = {0};
    (void)nx_rtc_port_backup_handoff(rtc, &handoff);
    return nx_platform_stop() == NX_SUCCESS ? 0 : 11;
}
