/**
 * \file            rtc_standby.c
 * \brief           Public C11 compile/link consumer for RTC Standby receipts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/rtc_aux.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
static nx_rtc_standby_t receipt;
static volatile bool exercise;
static volatile nx_result_t observed;
/** \brief Link-only contract; Power owns actual suspend/mask/Standby entry. */
int main(void) {
    for (;;) {
        if (!exercise)
            continue;
        exercise = false;
        (void)nx_platform_start();
        const nx_rtc_port_t* rtc = nx_factory_rtc(NX_RTC_ID_CALENDAR);
        observed = nx_rtc_port_standby_prepare(rtc, 0U, 1000000U, &receipt);
        observed = nx_rtc_port_standby_validate(rtc, &receipt);
        observed = nx_rtc_port_standby_commit(rtc, &receipt);
        observed = nx_rtc_port_standby_restore(rtc, &receipt);
        observed = nx_platform_stop();
    }
}
