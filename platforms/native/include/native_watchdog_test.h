/**
 * \file            native_watchdog_test.h
 * \brief           Native Watchdog Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_WATCHDOG_TEST_H
#define NATIVE_WATCHDOG_TEST_H

#include "hal/interface/nx_watchdog.h"
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
 * \brief           Get Watchdog instance by index
 * \param[in]       index: Watchdog instance index (0-3)
 * \return          Watchdog interface pointer or NULL
 */
nx_watchdog_t* nx_watchdog_native_get(uint8_t index);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all Watchdog instances
 * \note            This function is for testing purposes only
 */
void nx_watchdog_native_reset_all(void);

/**
 * \brief           Reset Watchdog instance
 * \param[in]       index: Watchdog instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_watchdog_native_reset(uint8_t index);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Watchdog state
 * \param[in]       index: Watchdog instance index
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_watchdog_native_get_state(uint8_t index, bool* initialized,
                                         bool* suspended);

/*---------------------------------------------------------------------------*/
/* Watchdog-Specific Test Helpers                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Watchdog device descriptor
 * \param[in]       index: Watchdog instance index
 * \return          Device descriptor pointer or NULL
 * \note            This function is for testing purposes only
 */
struct nx_device_s* nx_watchdog_native_get_device(uint8_t index);

/**
 * \brief           Check if watchdog has timed out
 * \param[in]       index: Watchdog instance index
 * \return          true if timed out, false otherwise
 * \note            This function is for testing purposes only
 */
bool nx_watchdog_native_has_timed_out(uint8_t index);

/**
 * \brief           Simulate time passage
 * \param[in]       index: Watchdog instance index
 * \param[in]       milliseconds: Number of milliseconds to advance
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_watchdog_native_advance_time(uint8_t index,
                                            uint32_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_WATCHDOG_TEST_H */
