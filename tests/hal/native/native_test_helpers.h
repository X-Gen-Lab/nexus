/**
 * \file            native_test_helpers.h
 * \brief           Native platform test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_TEST_HELPERS_H
#define NATIVE_TEST_HELPERS_H

#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Device Access Helpers                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get device state information
 * \param[in]       device_name: Device name (e.g., "UART0", "SPI1")
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 * \note            This is a generic helper that works with any device
 */
nx_status_t native_get_device_state(const char* device_name, bool* initialized,
                                    bool* suspended);

/*---------------------------------------------------------------------------*/
/* Test Utilities                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all native platform state
 * \note            Use with caution - resets all devices
 */
void native_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_TEST_HELPERS_H */
