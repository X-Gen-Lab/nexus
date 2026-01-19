/**
 * \file            nx_rtc_helpers.c
 * \brief           RTC helper functions implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements RTC helper functions including date/time
 *                  validation, timestamp conversion, and alarm checking.
 */

#include "nx_rtc_helpers.h"
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/*---------------------------------------------------------------------------*/
/* Date/Time Validation                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Check if year is a leap year
 */
static bool is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * \brief           Get days in month
 */
static uint8_t days_in_month(uint8_t month, uint16_t year) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};

    if (month < 1 || month > 12) {
        return 0;
    }

    uint8_t days_count = days[month - 1];
    if (month == 2 && is_leap_year(year)) {
        days_count = 29;
    }

    return days_count;
}

/**
 * \brief           Validate date/time structure
 */
nx_status_t rtc_validate_datetime(const nx_datetime_t* dt) {
    if (dt == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* Validate year (2000-2099) */
    if (dt->year < 2000 || dt->year > 2099) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Validate month (1-12) */
    if (dt->month < 1 || dt->month > 12) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Validate day (1-31, depends on month) */
    uint8_t max_days = days_in_month(dt->month, dt->year);
    if (dt->day < 1 || dt->day > max_days) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Validate hour (0-23) */
    if (dt->hour > 23) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Validate minute (0-59) */
    if (dt->minute > 59) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Validate second (0-59) */
    if (dt->second > 59) {
        return NX_ERR_INVALID_PARAM;
    }

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Timestamp Conversion                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert date/time to Unix timestamp
 */
uint32_t rtc_datetime_to_timestamp(const nx_datetime_t* dt) {
    if (dt == NULL) {
        return 0;
    }

    struct tm timeinfo = {0};
    timeinfo.tm_year = dt->year - 1900; /* Years since 1900 */
    timeinfo.tm_mon = dt->month - 1;    /* Months since January (0-11) */
    timeinfo.tm_mday = dt->day;
    timeinfo.tm_hour = dt->hour;
    timeinfo.tm_min = dt->minute;
    timeinfo.tm_sec = dt->second;
    timeinfo.tm_isdst = -1; /* Let mktime determine DST */

    time_t timestamp = mktime(&timeinfo);
    return (uint32_t)timestamp;
}

/**
 * \brief           Convert Unix timestamp to date/time
 */
void rtc_timestamp_to_datetime(uint32_t timestamp, nx_datetime_t* dt) {
    if (dt == NULL) {
        return;
    }

    time_t time = (time_t)timestamp;
    struct tm* timeinfo = localtime(&time);

    if (timeinfo != NULL) {
        dt->year = (uint16_t)(timeinfo->tm_year + 1900);
        dt->month = (uint8_t)(timeinfo->tm_mon + 1);
        dt->day = (uint8_t)timeinfo->tm_mday;
        dt->hour = (uint8_t)timeinfo->tm_hour;
        dt->minute = (uint8_t)timeinfo->tm_min;
        dt->second = (uint8_t)timeinfo->tm_sec;
    } else {
        memset(dt, 0, sizeof(nx_datetime_t));
    }
}

/*---------------------------------------------------------------------------*/
/* Date/Time Comparison                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Compare two date/time structures
 */
int rtc_compare_datetime(const nx_datetime_t* dt1, const nx_datetime_t* dt2) {
    if (dt1 == NULL || dt2 == NULL) {
        return 0;
    }

    /* Compare year */
    if (dt1->year != dt2->year) {
        return (int)dt1->year - (int)dt2->year;
    }

    /* Compare month */
    if (dt1->month != dt2->month) {
        return (int)dt1->month - (int)dt2->month;
    }

    /* Compare day */
    if (dt1->day != dt2->day) {
        return (int)dt1->day - (int)dt2->day;
    }

    /* Compare hour */
    if (dt1->hour != dt2->hour) {
        return (int)dt1->hour - (int)dt2->hour;
    }

    /* Compare minute */
    if (dt1->minute != dt2->minute) {
        return (int)dt1->minute - (int)dt2->minute;
    }

    /* Compare second */
    if (dt1->second != dt2->second) {
        return (int)dt1->second - (int)dt2->second;
    }

    return 0;
}

/*---------------------------------------------------------------------------*/
/* System Time                                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get current system time in milliseconds
 */
uint64_t rtc_get_system_time_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    /* Convert to 64-bit value */
    uint64_t time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

    /* Convert from 100-nanosecond intervals to milliseconds */
    /* Also adjust from Windows epoch (1601) to Unix epoch (1970) */
    time = (time / 10000) - 11644473600000ULL;

    return time;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
#endif
}

/*---------------------------------------------------------------------------*/
/* Alarm Checking                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Check if alarm should trigger
 */
void rtc_check_alarm(nx_rtc_state_t* state) {
    if (state == NULL || !state->alarm.enabled) {
        return;
    }

    /* Compare current time with alarm time */
    if (rtc_compare_datetime(&state->current_time, &state->alarm.alarm_time) >=
        0) {
        /* Alarm triggered */
        if (state->alarm.callback != NULL) {
            state->alarm.callback(state->alarm.user_data);
            state->stats.alarm_trigger_count++;
        }

        /* Disable alarm after triggering (one-shot) */
        state->alarm.enabled = false;
    }
}

/*---------------------------------------------------------------------------*/
/* Test Support                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset RTC state for testing
 */
void rtc_reset_state(nx_rtc_state_t* state) {
    if (state == NULL) {
        return;
    }

    state->initialized = false;
    state->suspended = false;
    memset(&state->current_time, 0, sizeof(nx_datetime_t));
    memset(&state->alarm, 0, sizeof(nx_rtc_alarm_t));
    memset(&state->stats, 0, sizeof(nx_rtc_stats_t));
    state->start_timestamp_ms = 0;
}
