/**
 * \file            main.c
 * \brief           Cold link consumer, not an executable STOP demonstration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/clock_domain_owner.h"
#include "nexus/os/lowpower.h"
#include "nexus/soc/stm32f4_freertos_stop.h"
#include "nexus_factory_strong.h"

static nx_stm32_freertos_stop_t stop_owner;
static nx_clock_domain_t clock_domain;
static nx_stm32_freertos_stop_config_t stop_config;

nx_stm32_freertos_stop_t* nx_stm32_freertos_stop_context(void) {
    return &stop_owner;
}

int main(void) {
    const nx_rtc_port_t* rtc = nx_factory_rtc_ref(NX_RTC_REF_CALENDAR);
    if (rtc == NULL)
        return 1;
    /* Zero expected Idle cannot pause a tick or enter STOP. Keeping the real
     * selected entry linked checks the strong hook and kernel ABI together. */
    nx_freertos_suppress_ticks_and_sleep(0U);
    stop_config.rtc = rtc;
    stop_config.poll_limit = 16U;
    stop_config.max_elapsed_us = 200000U;
    stop_config.drift_ppm = 100U;
    if (nx_clock_domain_construct(&clock_domain, 1U) != NX_SUCCESS ||
        nx_clock_domain_close(&clock_domain, &stop_config.barrier) !=
            NX_SUCCESS)
        return 2;
    /* No live suspended RTC has been handed over: a cold admission must fail.
     * This is compile/link coverage, without scheduler or oscillator claims. */
    return nx_stm32_freertos_stop_arm(&stop_owner, &stop_config) == NX_SUCCESS;
}
