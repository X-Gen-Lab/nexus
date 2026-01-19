/**
 * \file            native_option_bytes_test.h
 * \brief           Native Option Bytes Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_OPTION_BYTES_TEST_H
#define NATIVE_OPTION_BYTES_TEST_H

#include "hal/interface/nx_option_bytes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Option Bytes instance by index
 * \param[in]       index: Instance index
 * \return          Option Bytes interface pointer, NULL on failure
 */
nx_option_bytes_t* nx_option_bytes_native_get(uint8_t index);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all Option Bytes instances
 */
void nx_option_bytes_native_reset_all(void);

/**
 * \brief           Reset Option Bytes instance
 * \param[in]       index: Instance index
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_option_bytes_native_reset(uint8_t index);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Option Bytes state
 * \param[in]       index: Instance index
 * \param[out]      initialized: Initialization status (optional)
 * \param[out]      suspended: Suspend status (optional)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_option_bytes_native_get_state(uint8_t index, bool* initialized,
                                             bool* suspended);

/*---------------------------------------------------------------------------*/
/* Option Bytes-Specific Test Helpers                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set write protection status
 * \param[in]       index: Instance index
 * \param[in]       protected: Write protection status
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_option_bytes_native_set_write_protection(uint8_t index,
                                                        bool protected);

/**
 * \brief           Get write protection status
 * \param[in]       index: Instance index
 * \param[out]      protected: Write protection status
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_option_bytes_native_get_write_protection(uint8_t index,
                                                        bool* protected);

/**
 * \brief           Check if there are pending changes
 * \param[in]       index: Instance index
 * \param[out]      has_pending: Pending changes flag
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_option_bytes_native_has_pending_changes(uint8_t index,
                                                       bool* has_pending);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_OPTION_BYTES_TEST_H */
