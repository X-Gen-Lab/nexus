/**
 * \file            native_flash_helpers.h
 * \brief           Native Flash test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_FLASH_HELPERS_H
#define NATIVE_FLASH_HELPERS_H

#include "hal/interface/nx_flash.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Flash Test Helpers                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Flash device state
 * \param[in]       index: Flash index
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_flash_get_state(uint8_t index, bool* initialized,
                                   bool* suspended);

/**
 * \brief           Get Flash lock status
 * \param[in]       index: Flash index
 * \param[out]      locked: Lock status
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_flash_get_lock_status(uint8_t index, bool* locked);

/**
 * \brief           Check if Flash region is erased
 * \param[in]       index: Flash index
 * \param[in]       address: Start address
 * \param[in]       length: Length to check
 * \return          true if erased, false otherwise
 */
bool native_flash_is_erased(uint8_t index, uint32_t address, uint32_t length);

/**
 * \brief           Reset all Flash instances to initial state
 * \note            This is useful for test isolation
 */
void native_flash_reset_all(void);

/**
 * \brief           Set unique backing file for Flash instance
 * \param[in]       index: Flash index
 * \param[in]       filename: Unique filename for persistence
 * \return          NX_OK on success, error code otherwise
 * \note            Use this to avoid file conflicts in parallel tests
 */
nx_status_t native_flash_set_backing_file(uint8_t index, const char* filename);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_FLASH_HELPERS_H */
