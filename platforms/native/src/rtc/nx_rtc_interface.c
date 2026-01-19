/**
 * \file            nx_rtc_interface.c
 * \brief           RTC interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements RTC interface functions (set_timestamp,
 *                  get_timestamp, set_datetime, get_datetime, set_alarm,
 *                  get_lifecycle, get_power).
 */

#include "nx_rtc_helpers.h"
#include "nx_rtc_types.h"

/*---------------------------------------------------------------------------*/
/* RTC Interface Implementation                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set Unix timestamp
 */
static nx_status_t rtc_set_timestamp(nx_rtc_t* self, uint32_t timestamp) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_rtc_impl_t* impl = rtc_get_impl(self);
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Convert timestamp to datetime */
    rtc_timestamp_to_datetime(timestamp, &state->current_time);

    /* Update start timestamp for simulation */
    state->start_timestamp_ms = rtc_get_system_time_ms();

    /* Update statistics */
    state->stats.set_time_count++;

    /* Check if alarm should trigger */
    rtc_check_alarm(state);

    return NX_OK;
}

/**
 * \brief           Get Unix timestamp
 */
static uint32_t rtc_get_timestamp(nx_rtc_t* self) {
    if (self == NULL) {
        return 0;
    }

    nx_rtc_impl_t* impl = rtc_get_impl(self);
    nx_rtc_state_t* state = impl->state;

    if (state == NULL || !state->initialized) {
        return 0;
    }

    /* Calculate elapsed time since last set */
    uint64_t current_ms = rtc_get_system_time_ms();
    uint64_t elapsed_ms = current_ms - state->start_timestamp_ms;
    uint32_t elapsed_sec = (uint32_t)(elapsed_ms / 1000);

    /* Get base timestamp and add elapsed time */
    uint32_t base_timestamp = rtc_datetime_to_timestamp(&state->current_time);
    uint32_t current_timestamp = base_timestamp + elapsed_sec;

    /* Update statistics */
    state->stats.get_time_count++;

    return current_timestamp;
}

/**
 * \brief           Set date and time
 */
static nx_status_t rtc_set_datetime(nx_rtc_t* self, const nx_datetime_t* dt) {
    if (self == NULL || dt == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_rtc_impl_t* impl = rtc_get_impl(self);
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Validate datetime */
    nx_status_t status = rtc_validate_datetime(dt);
    if (status != NX_OK) {
        return status;
    }

    /* Set current time */
    state->current_time = *dt;

    /* Update start timestamp for simulation */
    state->start_timestamp_ms = rtc_get_system_time_ms();

    /* Update statistics */
    state->stats.set_time_count++;

    /* Check if alarm should trigger */
    rtc_check_alarm(state);

    return NX_OK;
}

/**
 * \brief           Get date and time
 */
static nx_status_t rtc_get_datetime(nx_rtc_t* self, nx_datetime_t* dt) {
    if (self == NULL || dt == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_rtc_impl_t* impl = rtc_get_impl(self);
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Calculate elapsed time since last set */
    uint64_t current_ms = rtc_get_system_time_ms();
    uint64_t elapsed_ms = current_ms - state->start_timestamp_ms;
    uint32_t elapsed_sec = (uint32_t)(elapsed_ms / 1000);

    /* Get base timestamp and add elapsed time */
    uint32_t base_timestamp = rtc_datetime_to_timestamp(&state->current_time);
    uint32_t current_timestamp = base_timestamp + elapsed_sec;

    /* Convert back to datetime */
    rtc_timestamp_to_datetime(current_timestamp, dt);

    /* Update statistics */
    state->stats.get_time_count++;

    /* Check if alarm should trigger */
    rtc_check_alarm(state);

    return NX_OK;
}

/**
 * \brief           Set alarm with callback
 */
static nx_status_t rtc_set_alarm(nx_rtc_t* self, const nx_datetime_t* alarm,
                                 nx_rtc_alarm_callback_t callback,
                                 void* user_data) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_rtc_impl_t* impl = rtc_get_impl(self);
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if alarm functionality is enabled */
    if (!state->config.enable_alarm) {
        return NX_ERR_NOT_SUPPORTED;
    }

    /* If callback is NULL, disable alarm */
    if (callback == NULL) {
        state->alarm.enabled = false;
        state->alarm.callback = NULL;
        state->alarm.user_data = NULL;
        return NX_OK;
    }

    /* Validate alarm datetime */
    if (alarm != NULL) {
        nx_status_t status = rtc_validate_datetime(alarm);
        if (status != NX_OK) {
            return status;
        }
    } else {
        return NX_ERR_NULL_PTR;
    }

    /* Set alarm */
    state->alarm.enabled = true;
    state->alarm.alarm_time = *alarm;
    state->alarm.callback = callback;
    state->alarm.user_data = user_data;

    /* Update statistics */
    state->stats.set_alarm_count++;

    /* Check if alarm should trigger immediately */
    rtc_check_alarm(state);

    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* rtc_get_lifecycle(nx_rtc_t* self) {
    if (self == NULL) {
        return NULL;
    }

    nx_rtc_impl_t* impl = rtc_get_impl(self);
    return &impl->lifecycle;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* rtc_get_power(nx_rtc_t* self) {
    if (self == NULL) {
        return NULL;
    }

    nx_rtc_impl_t* impl = rtc_get_impl(self);
    return &impl->power;
}

/*---------------------------------------------------------------------------*/
/* RTC Interface Initialization                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize RTC interface
 */
void rtc_init_interface(nx_rtc_t* rtc) {
    if (rtc == NULL) {
        return;
    }

    rtc->set_timestamp = rtc_set_timestamp;
    rtc->get_timestamp = rtc_get_timestamp;
    rtc->set_datetime = rtc_set_datetime;
    rtc->get_datetime = rtc_get_datetime;
    rtc->set_alarm = rtc_set_alarm;
    rtc->get_lifecycle = rtc_get_lifecycle;
    rtc->get_power = rtc_get_power;
}
