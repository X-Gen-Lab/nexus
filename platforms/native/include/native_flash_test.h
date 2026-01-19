/**
 * \file            native_flash_test.h
 * \brief           Native Flash Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_FLASH_TEST_H
#define NATIVE_FLASH_TEST_H

#include "hal/interface/nx_flash.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Flash instance by index
 * \param[in]       index: Flash instance index (0-3)
 * \return          Flash interface pointer, NULL if invalid
 */
nx_internal_flash_t* nx_flash_native_get(uint8_t index);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all Flash instances
 */
void nx_flash_native_reset_all(void);

/**
 * \brief           Reset specific Flash instance
 * \param[in]       index: Flash instance index
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_flash_native_reset(uint8_t index);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Flash state
 * \param[in]       index: Flash instance index
 * \param[out]      initialized: Initialization status (can be NULL)
 * \param[out]      suspended: Suspend status (can be NULL)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_flash_native_get_state(uint8_t index, bool* initialized,
                                      bool* suspended);

/*---------------------------------------------------------------------------*/
/* Flash-Specific Test Helpers                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set backing file path for flash persistence
 * \param[in]       index: Flash instance index
 * \param[in]       path: File path for persistence
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_flash_native_set_backing_file(uint8_t index, const char* path);

/**
 * \brief           Get backing file path
 * \param[in]       index: Flash instance index
 * \param[out]      path: Buffer to store file path
 * \param[in]       path_len: Buffer length
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_flash_native_get_backing_file(uint8_t index, char* path,
                                             size_t path_len);

/**
 * \brief           Check if flash address range is erased
 * \param[in]       index: Flash instance index
 * \param[in]       addr: Start address
 * \param[in]       len: Length in bytes
 * \return          true if erased, false otherwise
 */
bool nx_flash_native_is_erased(uint8_t index, uint32_t addr, size_t len);

/**
 * \brief           Get flash lock status
 * \param[in]       index: Flash instance index
 * \param[out]      locked: Lock status
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_flash_native_get_lock_status(uint8_t index, bool* locked);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_FLASH_TEST_H */
