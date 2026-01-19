/**
 * \file            native_watchdog_helpers.h
 * \brief           Native Watchdog test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_WATCHDOG_HELPERS_H
#define NATIVE_WATCHDOG_HELPERS_H

#include "hal/interface/nx_watchdog.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Watchdog Test Helpers                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Watchdog device state
 * \param[in]       index: Watchdog index
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_watchdog_get_state(uint8_t index, bool* initialized,
                                      bool* suspended);

/**
 * \brief           Check if watchdog has timed out
 * \param[in]       index: Watchdog index
 * \return          true if timed out, false otherwise
 */
bool native_watchdog_has_timed_out(uint8_t index);

/**
 * \brief           Simulate time passage for testing
 * \param[in]       index: Watchdog index
 * \param[in]       milliseconds: Time to advance in milliseconds
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_watchdog_advance_time(uint8_t index, uint32_t milliseconds);

/*---------------------------------------------------------------------------*/
/* Usage Example                                                             */
/*---------------------------------------------------------------------------*/

/*
 * // Get device using nx_factory
 * nx_watchdog_t* wdt = nx_factory_watchdog(0);
 * ASSERT_NE(wdt, nullptr);
 *
 * // Initialize
 * nx_watchdog_config_t config = { .timeout_ms = 1000 };
 * ASSERT_EQ(wdt->init(wdt, &config), NX_OK);
 *
 * // Use test helper to simulate time
 * native_watchdog_advance_time(0, 500);
 * ASSERT_FALSE(native_watchdog_has_timed_out(0));
 *
 * native_watchdog_advance_time(0, 600);
 * ASSERT_TRUE(native_watchdog_has_timed_out(0));
 */

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_WATCHDOG_HELPERS_H */
