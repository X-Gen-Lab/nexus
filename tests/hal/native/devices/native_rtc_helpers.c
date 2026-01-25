/**
 * \file            native_rtc_helpers.c
 * \brief           Native RTC test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_rtc_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/rtc/nx_rtc_types.h"

#include <string.h>

/* External function to advance simulated time */
extern void nx_advance_time_ms(uint32_t ms);
extern void nx_reset_time(void);

/*---------------------------------------------------------------------------*/
/* Internal Helper                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get RTC implementation structure
 */
static nx_rtc_impl_t* get_rtc_impl(uint8_t index) {
    nx_rtc_t* rtc = nx_factory_rtc(index);
    if (rtc == NULL) {
        return NULL;
    }
    return (nx_rtc_impl_t*)rtc;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get RTC device state
 */
nx_status_t native_rtc_get_state(uint8_t index, bool* initialized,
                                 bool* suspended) {
    nx_rtc_impl_t* impl = get_rtc_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    if (initialized != NULL) {
        *initialized = impl->state->initialized;
    }

    if (suspended != NULL) {
        *suspended = impl->state->suspended;
    }

    return NX_OK;
}

/**
 * \brief           Advance RTC time
 */
nx_status_t native_rtc_advance_time(uint8_t index, uint32_t seconds) {
    nx_rtc_impl_t* impl = get_rtc_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Advance simulated time */
    nx_advance_time_ms(seconds * 1000);

    /* Update RTC time */
    impl->state->current_time.second += (uint8_t)seconds;
    while (impl->state->current_time.second >= 60) {
        impl->state->current_time.second -= 60;
        impl->state->current_time.minute++;
        if (impl->state->current_time.minute >= 60) {
            impl->state->current_time.minute = 0;
            impl->state->current_time.hour++;
            if (impl->state->current_time.hour >= 24) {
                impl->state->current_time.hour = 0;
                impl->state->current_time.day++;
            }
        }
    }

    return NX_OK;
}

/**
 * \brief           Check and trigger alarm
 * \details         Implements one-shot alarm behavior - alarm is automatically
 *                  disabled after triggering once.
 */
nx_status_t native_rtc_check_alarm(uint8_t index) {
    nx_rtc_impl_t* impl = get_rtc_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if alarm is enabled and should trigger */
    if (impl->state->alarm.enabled && impl->state->alarm.callback != NULL) {
        nx_datetime_t* current = &impl->state->current_time;
        nx_datetime_t* alarm = &impl->state->alarm.alarm_time;

        if (current->hour == alarm->hour && current->minute == alarm->minute &&
            current->second == alarm->second) {
            /* Trigger alarm callback */
            impl->state->alarm.callback(impl->state->alarm.user_data);
            impl->state->stats.alarm_trigger_count++;

            /* Disable alarm after triggering (one-shot behavior) */
            impl->state->alarm.enabled = false;
        }
    }

    return NX_OK;
}

/**
 * \brief           Reset all RTC instances
 */
void native_rtc_reset_all(void) {
    for (uint8_t i = 0; i < 4; i++) {
        nx_rtc_impl_t* impl = get_rtc_impl(i);
        if (impl == NULL || impl->state == NULL) {
            continue;
        }

        impl->state->initialized = false;
        impl->state->suspended = false;
        memset(&impl->state->current_time, 0, sizeof(nx_datetime_t));
        memset(&impl->state->alarm, 0, sizeof(nx_rtc_alarm_t));
        memset(&impl->state->stats, 0, sizeof(nx_rtc_stats_t));
    }

    nx_reset_time();
}
