/**
 * \file            native_i2c_helpers.h
 * \brief           Native I2C test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_I2C_HELPERS_H
#define NATIVE_I2C_HELPERS_H

#include "hal/interface/nx_i2c.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* I2C State Structure for Testing                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C state structure for testing
 *
 * Contains runtime state information that can be queried by tests.
 */
typedef struct native_i2c_state_s {
    bool initialized;         /**< Initialization flag */
    bool suspended;           /**< Suspend flag */
    bool busy;                /**< Busy flag */
    uint32_t speed;           /**< I2C speed in Hz */
    uint8_t scl_pin;          /**< SCL pin number */
    uint8_t sda_pin;          /**< SDA pin number */
    uint8_t current_dev_addr; /**< Current device address */
    uint32_t tx_count;        /**< Total bytes transmitted */
    uint32_t rx_count;        /**< Total bytes received */
    uint32_t nack_count;      /**< NACK count */
    uint32_t bus_error_count; /**< Bus error count */
    size_t tx_buf_count;      /**< Bytes in TX buffer */
    size_t rx_buf_count;      /**< Bytes in RX buffer */
} native_i2c_state_t;

/*---------------------------------------------------------------------------*/
/* I2C Test Helpers                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get I2C device state
 * \param[in]       instance: I2C instance ID
 * \param[out]      state: State structure to fill
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_i2c_get_state(uint8_t instance, native_i2c_state_t* state);

/**
 * \brief           Inject receive data (simulate hardware reception)
 * \param[in]       instance: I2C instance ID
 * \param[in]       data: Data buffer to inject
 * \param[in]       len: Data length
 * \return          NX_OK on success, error code otherwise
 * \note            This simulates data arriving from the hardware into
 *                  the RX buffer, making it available for reading.
 */
nx_status_t native_i2c_inject_rx_data(uint8_t instance, const uint8_t* data,
                                      size_t len);

/**
 * \brief           Get transmitted data (capture hardware transmission)
 * \param[in]       instance: I2C instance ID
 * \param[out]      data: Buffer to store captured data
 * \param[in,out]   len: Input: buffer size, Output: actual data length
 * \return          NX_OK on success, error code otherwise
 * \note            This captures data that was transmitted through the
 *                  TX buffer, simulating what would be sent to hardware.
 */
nx_status_t native_i2c_get_tx_data(uint8_t instance, uint8_t* data,
                                   size_t* len);

/**
 * \brief           Reset specific I2C instance to initial state
 * \param[in]       instance: I2C instance ID
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_i2c_reset(uint8_t instance);

/**
 * \brief           Reset all I2C instances to initial state
 */
void native_i2c_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_I2C_HELPERS_H */
