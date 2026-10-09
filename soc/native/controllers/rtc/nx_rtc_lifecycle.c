/**
 * \file            nx_rtc_lifecycle.c
 * \brief           RTC lifecycle implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements lifecycle management (init, deinit, suspend,
 *                  resume, get_state) for RTC peripheral.
 */

#include "nx_rtc_helpers.h"
#include "nx_rtc_types.h"

/*---------------------------------------------------------------------------*/
/* Lifecycle Implementation                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize RTC device
 */
static nx_status_t rtc_lifecycle_init(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_rtc_impl_t* impl =
        (nx_rtc_impl_t*)((uint8_t*)self - offsetof(nx_rtc_impl_t, lifecycle));
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Initialize start timestamp for time simulation */
    state->start_timestamp_ms = rtc_get_system_time_ms();

    /* Initialize current time to Unix epoch */
    state->current_time.year = 1970;
    state->current_time.month = 1;
    state->current_time.day = 1;
    state->current_time.hour = 0;
    state->current_time.minute = 0;
    state->current_time.second = 0;

    /* Clear alarm */
    state->alarm.enabled = false;
    state->alarm.callback = NULL;
    state->alarm.user_data = NULL;

    /* Clear statistics */
    state->stats.set_time_count = 0;
    state->stats.get_time_count = 0;
    state->stats.set_alarm_count = 0;
    state->stats.alarm_trigger_count = 0;

    /* Mark as initialized and running */
    state->initialized = true;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize RTC device
 */
static nx_status_t rtc_lifecycle_deinit(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_rtc_impl_t* impl =
        (nx_rtc_impl_t*)((uint8_t*)self - offsetof(nx_rtc_impl_t, lifecycle));
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Disable alarm */
    state->alarm.enabled = false;
    state->alarm.callback = NULL;

    /* Mark as uninitialized */
    state->initialized = false;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Suspend RTC device
 */
static nx_status_t rtc_lifecycle_suspend(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_rtc_impl_t* impl =
        (nx_rtc_impl_t*)((uint8_t*)self - offsetof(nx_rtc_impl_t, lifecycle));
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as suspended (preserve RTC state) */
    state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume RTC device
 */
static nx_status_t rtc_lifecycle_resume(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_rtc_impl_t* impl =
        (nx_rtc_impl_t*)((uint8_t*)self - offsetof(nx_rtc_impl_t, lifecycle));
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as running (restore RTC state) */
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get RTC device state
 */
static nx_device_state_t rtc_lifecycle_get_state(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    nx_rtc_impl_t* impl =
        (nx_rtc_impl_t*)((uint8_t*)self - offsetof(nx_rtc_impl_t, lifecycle));
    nx_rtc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    if (!state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    if (state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }

    return NX_DEV_STATE_RUNNING;
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Initialization                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface
 */
void rtc_init_lifecycle(nx_lifecycle_t* lifecycle) {
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->init = rtc_lifecycle_init;
    lifecycle->deinit = rtc_lifecycle_deinit;
    lifecycle->suspend = rtc_lifecycle_suspend;
    lifecycle->resume = rtc_lifecycle_resume;
    lifecycle->get_state = rtc_lifecycle_get_state;
}
