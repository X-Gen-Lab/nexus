/**
 * \file            raw_alarm.c
 * \brief           Exercise the private F103RB RTC EXTI17 compare session.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/rtc/alarm.h"
#include "nexus/core/time.h"
#include "nexus_bindings.h"

static nx_stm32f1_rtc_alarm_t alarm;

void RTC_Alarm_IRQHandler(void);
void RTC_Alarm_IRQHandler(void) {
    nx_stm32f1_rtc_alarm_irq(&alarm);
}

static nx_result_t arm_on_counter_event(void) {
    const nx_time_us_t until =
        nx_deadline_after(nx_time_now_us(), UINT32_C(2000000));
    do {
        uint32_t now;
        nx_result_t result = nx_stm32f1_rtc_ticks(&alarm.backup, &now);
        if (result != NX_SUCCESS)
            return result;
        result = nx_stm32f1_rtc_alarm_arm(&alarm, now + 8U);
        if (result != NX_ERROR_EMPTY)
            return result;
    } while (!nx_deadline_expired(until, nx_time_now_us()));
    return NX_ERROR_TIMEOUT;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;

    nx_result_t result = nx_stm32f1_rtc_alarm_prepare(&alarm, 7U);
    if (result == NX_SUCCESS)
        result = arm_on_counter_event();
    if (result == NX_SUCCESS) {
        const nx_time_us_t until =
            nx_deadline_after(nx_time_now_us(), UINT32_C(10000000));
        do {
            result = nx_stm32f1_rtc_alarm_take(&alarm);
        } while (result == NX_ERROR_EMPTY &&
                 !nx_deadline_expired(until, nx_time_now_us()));
    }
    const nx_result_t closed = nx_stm32f1_rtc_alarm_close(&alarm);
    const nx_result_t stopped = nx_platform_stop();
    return result == NX_SUCCESS && closed == NX_SUCCESS && stopped == NX_SUCCESS
               ? 0
               : 2;
}
