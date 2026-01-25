/**
 * \file            native_spi_helpers.h
 * \brief           Native SPI test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_SPI_HELPERS_H
#define NATIVE_SPI_HELPERS_H

#include "hal/interface/nx_spi.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* SPI State Structure for Testing                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI state structure for testing
 *
 * Contains runtime state information that can be queried by tests.
 */
typedef struct native_spi_state_s {
    bool initialized;          /**< Initialization flag */
    bool suspended;            /**< Suspend flag */
    bool busy;                 /**< Busy flag */
    uint32_t max_speed;        /**< Maximum SPI speed in Hz */
    uint8_t mosi_pin;          /**< MOSI pin number */
    uint8_t miso_pin;          /**< MISO pin number */
    uint8_t sck_pin;           /**< SCK pin number */
    uint8_t current_cs_pin;    /**< Current device CS pin */
    uint32_t current_speed;    /**< Current device speed */
    uint8_t current_mode;      /**< Current SPI mode (0-3) */
    uint8_t current_bit_order; /**< Current bit order */
    uint32_t tx_count;         /**< Total bytes transmitted */
    uint32_t rx_count;         /**< Total bytes received */
    uint32_t error_count;      /**< Error count */
    size_t tx_buf_count;       /**< Bytes in TX buffer */
    size_t rx_buf_count;       /**< Bytes in RX buffer */
} native_spi_state_t;

/*---------------------------------------------------------------------------*/
/* SPI Test Helpers                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SPI device state
 * \param[in]       instance: SPI instance ID
 * \param[out]      state: State structure to fill
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_spi_get_state(uint8_t instance, native_spi_state_t* state);

/**
 * \brief           Inject receive data (simulate hardware reception)
 * \param[in]       instance: SPI instance ID
 * \param[in]       data: Data buffer to inject
 * \param[in]       len: Data length
 * \return          NX_OK on success, error code otherwise
 * \note            This simulates data arriving from the hardware into
 *                  the RX buffer, making it available for reading.
 */
nx_status_t native_spi_inject_rx_data(uint8_t instance, const uint8_t* data,
                                      size_t len);

/**
 * \brief           Get transmitted data (capture hardware transmission)
 * \param[in]       instance: SPI instance ID
 * \param[out]      data: Buffer to store captured data
 * \param[in,out]   len: Input: buffer size, Output: actual data length
 * \return          NX_OK on success, error code otherwise
 * \note            This captures data that was transmitted through the
 *                  TX buffer, simulating what would be sent to hardware.
 */
nx_status_t native_spi_get_tx_data(uint8_t instance, uint8_t* data,
                                   size_t* len);

/**
 * \brief           Reset specific SPI instance to initial state
 * \param[in]       instance: SPI instance ID
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_spi_reset(uint8_t instance);

/**
 * \brief           Reset all SPI instances to initial state
 */
void native_spi_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_SPI_HELPERS_H */
