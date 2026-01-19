/**
 * \file            native_i2c_test.h
 * \brief           Native I2C Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_I2C_TEST_H
#define NATIVE_I2C_TEST_H

#include "hal/interface/nx_i2c.h"
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
 * \brief           Simulated I2C state (opaque for testing)
 */
typedef struct native_i2c_state native_i2c_state_t;

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get I2C instance (factory function)
 * \param[in]       index: I2C index (0-2)
 * \return          I2C interface pointer, NULL on failure
 */
nx_i2c_t* nx_i2c_native_get(uint8_t index);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all simulated I2C states
 */
void native_i2c_reset_all(void);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get simulated I2C state (for testing)
 * \param[in]       instance: I2C instance
 * \return          Pointer to I2C state or NULL
 */
native_i2c_state_t* native_i2c_get_state(int instance);

/**
 * \brief           Check if I2C instance is initialized
 * \param[in]       instance: I2C instance
 * \return          true if initialized, false otherwise
 */
bool native_i2c_is_initialized(int instance);

/**
 * \brief           Get actual configured I2C speed (for testing)
 * \param[in]       instance: I2C instance
 * \return          Actual speed in Hz or 0 if not initialized
 */
uint32_t native_i2c_get_actual_speed(int instance);

/**
 * \brief           Get last device address used (for testing)
 * \param[in]       instance: I2C instance
 * \return          Last device address or 0
 */
uint16_t native_i2c_get_last_dev_addr(int instance);

/**
 * \brief           Get last memory address used (for testing)
 * \param[in]       instance: I2C instance
 * \return          Last memory address or 0
 */
uint16_t native_i2c_get_last_mem_addr(int instance);

/*---------------------------------------------------------------------------*/
/* I2C-Specific Test Helpers                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Add simulated I2C device (for testing)
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address
 * \param[in]       ready: Device ready flag
 * \return          true on success, false on failure
 */
bool native_i2c_add_device(int instance, uint16_t dev_addr, bool ready);

/**
 * \brief           Set device ready state (for testing)
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address
 * \param[in]       ready: Device ready flag
 * \return          true on success, false on failure
 */
bool native_i2c_set_device_ready(int instance, uint16_t dev_addr, bool ready);

/**
 * \brief           Write data to device memory (for testing)
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address
 * \param[in]       mem_addr: Memory address
 * \param[in]       data: Data to write
 * \param[in]       len: Length of data
 * \return          true on success, false on failure
 */
bool native_i2c_write_device_memory(int instance, uint16_t dev_addr,
                                    uint16_t mem_addr, const uint8_t* data,
                                    size_t len);

/**
 * \brief           Read data from device memory (for testing)
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address
 * \param[in]       mem_addr: Memory address
 * \param[out]      data: Buffer to store data
 * \param[in]       len: Length of data
 * \return          true on success, false on failure
 */
bool native_i2c_read_device_memory(int instance, uint16_t dev_addr,
                                   uint16_t mem_addr, uint8_t* data,
                                   size_t len);

/**
 * \brief           Get data from I2C TX buffer (for testing)
 * \param[in]       instance: I2C instance
 * \param[out]      data: Buffer to store data
 * \param[in]       max_len: Maximum length to read
 * \return          Number of bytes read
 */
size_t native_i2c_get_last_tx_data(int instance, uint8_t* data, size_t max_len);

/**
 * \brief           Get data from I2C RX buffer (for testing)
 * \param[in]       instance: I2C instance
 * \param[out]      data: Buffer to store data
 * \param[in]       max_len: Maximum length to read
 * \return          Number of bytes read
 */
size_t native_i2c_get_last_rx_data(int instance, uint8_t* data, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_I2C_TEST_H */
