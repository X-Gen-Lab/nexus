/**
 * \file            native_rtc_test.h
 * \brief           Native RTC Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_RTC_TEST_H
#define NATIVE_RTC_TEST_H

#include "hal/interface/nx_rtc.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get RTC instance by index
 * \param[in]       index: RTC instance index (0-3)
 * \return          RTC interface pointer or NULL
 */
nx_rtc_t* nx_rtc_native_get(uint8_t index);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all RTC instances
 * \note            This function is for testing purposes only
 */
void nx_rtc_native_reset_all(void);

/**
 * \brief           Reset RTC instance
 * \param[in]       index: RTC instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_rtc_native_reset(uint8_t index);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get RTC state
 * \param[in]       index: RTC instance index
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_rtc_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended);

/*---------------------------------------------------------------------------*/
/* RTC-Specific Test Helpers                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get RTC device descriptor
 * \param[in]       index: RTC instance index
 * \return          Device descriptor pointer or NULL
 * \note            This function is for testing purposes only
 */
struct nx_device_s* nx_rtc_native_get_device(uint8_t index);

/**
 * \brief           Simulate time passage
 * \param[in]       index: RTC instance index
 * \param[in]       seconds: Number of seconds to advance
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_rtc_native_advance_time(uint8_t index, uint32_t seconds);

/**
 * \brief           Trigger alarm check manually
 * \param[in]       index: RTC instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_rtc_native_check_alarm(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_RTC_TEST_H */
