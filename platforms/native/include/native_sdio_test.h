/**
 * \file            native_sdio_test.h
 * \brief           Native SDIO Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_SDIO_TEST_H
#define NATIVE_SDIO_TEST_H

#include "hal/interface/nx_sdio.h"
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
 * \brief           Get SDIO instance by index
 * \param[in]       index: SDIO instance index (0-3)
 * \return          SDIO interface pointer or NULL
 */
nx_sdio_t* nx_sdio_native_get(uint8_t index);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all SDIO instances
 * \note            This function is for testing purposes only
 */
void nx_sdio_native_reset_all(void);

/**
 * \brief           Reset SDIO instance
 * \param[in]       index: SDIO instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_sdio_native_reset(uint8_t index);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SDIO state
 * \param[in]       index: SDIO instance index
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_sdio_native_get_state(uint8_t index, bool* initialized,
                                     bool* suspended);

/*---------------------------------------------------------------------------*/
/* SDIO-Specific Test Helpers                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SDIO device descriptor
 * \param[in]       index: SDIO instance index
 * \return          Device descriptor pointer or NULL
 * \note            This function is for testing purposes only
 */
struct nx_device_s* nx_sdio_native_get_device(uint8_t index);

/**
 * \brief           Set card present state
 * \param[in]       index: SDIO instance index
 * \param[in]       present: Card present state
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_sdio_native_set_card_present(uint8_t index, bool present);

/**
 * \brief           Get card present state
 * \param[in]       index: SDIO instance index
 * \return          true if card is present, false otherwise
 * \note            This function is for testing purposes only
 */
bool nx_sdio_native_is_card_present(uint8_t index);

/**
 * \brief           Get block data for verification
 * \param[in]       index: SDIO instance index
 * \param[in]       block: Block number
 * \param[out]      data: Buffer to store block data
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_sdio_native_get_block_data(uint8_t index, uint32_t block,
                                          uint8_t* data);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_SDIO_TEST_H */
