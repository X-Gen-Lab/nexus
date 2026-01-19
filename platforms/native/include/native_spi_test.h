/**
 * \file            native_spi_test.h
 * \brief           Native SPI Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_SPI_TEST_H
#define NATIVE_SPI_TEST_H

#include "hal/interface/nx_spi.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulated SPI state (opaque for testing)
 */
typedef struct native_spi_state native_spi_state_t;

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all simulated SPI states
 */
void native_spi_reset_all(void);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get simulated SPI state (for testing)
 * \param[in]       instance: SPI instance
 * \return          Pointer to SPI state or NULL
 */
native_spi_state_t* native_spi_get_state(int instance);

/**
 * \brief           Check if SPI instance is initialized
 * \param[in]       instance: SPI instance
 * \return          true if initialized, false otherwise
 */
bool native_spi_is_initialized(int instance);

/**
 * \brief           Get CS pin state (for testing)
 * \param[in]       instance: SPI instance
 * \return          true if CS is active (asserted/low), false otherwise
 */
bool native_spi_get_cs_state(int instance);

/**
 * \brief           Get configured SPI mode (for testing)
 * \param[in]       instance: SPI instance
 * \return          SPI mode (0-3)
 */
nx_spi_mode_t native_spi_get_mode(int instance);

/**
 * \brief           Get length of last transfer (for testing)
 * \param[in]       instance: SPI instance
 * \return          Length of last transfer
 */
size_t native_spi_get_last_transfer_len(int instance);

/*---------------------------------------------------------------------------*/
/* SPI-Specific Test Helpers                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Inject data into SPI RX buffer (for testing)
 * \param[in]       instance: SPI instance
 * \param[in]       data: Data to inject
 * \param[in]       len: Length of data
 * \return          true on success, false on failure
 */
bool native_spi_inject_rx_data(int instance, const uint8_t* data, size_t len);

/**
 * \brief           Get data from SPI TX buffer (for testing)
 * \param[in]       instance: SPI instance
 * \param[out]      data: Buffer to store data
 * \param[in]       max_len: Maximum length to read
 * \return          Number of bytes read
 */
size_t native_spi_get_tx_data(int instance, uint8_t* data, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_SPI_TEST_H */
