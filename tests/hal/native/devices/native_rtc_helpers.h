/**
 * \file            native_rtc_helpers.h
 * \brief           Native RTC test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_RTC_HELPERS_H
#define NATIVE_RTC_HELPERS_H

#include "hal/interface/nx_rtc.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* RTC Test Helpers                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get RTC device state
 * \param[in]       index: RTC index
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_rtc_get_state(uint8_t index, bool* initialized,
                                 bool* suspended);

/**
 * \brief           Advance RTC time for testing
 * \param[in]       index: RTC index
 * \param[in]       seconds: Seconds to advance
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_rtc_advance_time(uint8_t index, uint32_t seconds);

/**
 * \brief           Check and trigger RTC alarm if needed
 * \param[in]       index: RTC index
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_rtc_check_alarm(uint8_t index);

/**
 * \brief           Reset all RTC instances to initial state
 */
void native_rtc_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_RTC_HELPERS_H */
