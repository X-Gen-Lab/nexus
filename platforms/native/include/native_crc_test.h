/**
 * \file            native_crc_test.h
 * \brief           Native CRC Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_CRC_TEST_H
#define NATIVE_CRC_TEST_H

#include "hal/interface/nx_crc.h"
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
 * \brief           Get CRC instance by index
 * \param[in]       index: CRC instance index (0-3)
 * \return          CRC interface pointer or NULL
 */
nx_crc_t* nx_crc_native_get(uint8_t index);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all CRC instances
 * \note            This function is for testing purposes only
 */
void nx_crc_native_reset_all(void);

/**
 * \brief           Reset CRC instance
 * \param[in]       index: CRC instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_crc_native_reset(uint8_t index);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get CRC state
 * \param[in]       index: CRC instance index
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_crc_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended);

/*---------------------------------------------------------------------------*/
/* CRC-Specific Test Helpers                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get CRC device descriptor
 * \param[in]       index: CRC instance index
 * \return          Device descriptor pointer or NULL
 * \note            This function is for testing purposes only
 */
struct nx_device_s* nx_crc_native_get_device(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_CRC_TEST_H */
