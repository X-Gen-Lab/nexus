/**
 * \file            nx_rtc_helpers.h
 * \brief           RTC helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_RTC_HELPERS_H
#define NX_RTC_HELPERS_H

#include "hal/interface/nx_rtc.h"
#include "hal/nx_status.h"
#include "nx_rtc_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get RTC implementation from base interface
 * \param[in]       self: RTC interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_rtc_impl_t* rtc_get_impl(nx_rtc_t* self) {
    return self ? (nx_rtc_impl_t*)self : NULL;
}

/**
 * \brief           Validate date/time structure
 * \param[in]       dt: Date/time structure to validate
 * \return          NX_OK if valid, error code otherwise
 */
nx_status_t rtc_validate_datetime(const nx_datetime_t* dt);

/**
 * \brief           Convert date/time to Unix timestamp
 * \param[in]       dt: Date/time structure
 * \return          Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
 */
uint32_t rtc_datetime_to_timestamp(const nx_datetime_t* dt);

/**
 * \brief           Convert Unix timestamp to date/time
 * \param[in]       timestamp: Unix timestamp
 * \param[out]      dt: Date/time structure to fill
 */
void rtc_timestamp_to_datetime(uint32_t timestamp, nx_datetime_t* dt);

/**
 * \brief           Compare two date/time structures
 * \param[in]       dt1: First date/time
 * \param[in]       dt2: Second date/time
 * \return          0 if equal, <0 if dt1 < dt2, >0 if dt1 > dt2
 */
int rtc_compare_datetime(const nx_datetime_t* dt1, const nx_datetime_t* dt2);

/**
 * \brief           Get current system time in milliseconds
 * \return          Current time in milliseconds
 */
uint64_t rtc_get_system_time_ms(void);

/**
 * \brief           Check if alarm should trigger
 * \param[in]       state: RTC state pointer
 */
void rtc_check_alarm(nx_rtc_state_t* state);

/**
 * \brief           Reset RTC state for testing
 * \param[in]       state: RTC state pointer
 * \note            This function is for testing purposes only
 */
void rtc_reset_state(nx_rtc_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NX_RTC_HELPERS_H */
