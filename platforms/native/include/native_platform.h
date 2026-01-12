/**
 * \file            native_platform.h
 * \brief           Native Platform Header (for host testing)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef NATIVE_PLATFORM_H
#define NATIVE_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/hal_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Simulated GPIO state
 */
typedef struct {
    bool     configured;    /**< Pin configured flag */
    bool     is_output;     /**< Output mode flag */
    bool     level;         /**< Current level */
} native_gpio_pin_t;

/**
 * \brief           Get simulated GPIO state (for testing)
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin
 * \return          Pointer to pin state or NULL
 */
native_gpio_pin_t* native_gpio_get_state(uint8_t port, uint8_t pin);

/**
 * \brief           Reset all simulated GPIO states
 */
void native_gpio_reset_all(void);

/*===========================================================================*/
/* UART Test Helpers                                                          */
/*===========================================================================*/

/**
 * \brief           Simulated UART state (opaque for testing)
 */
typedef struct native_uart_state native_uart_state_t;

/**
 * \brief           Reset all simulated UART states
 */
void native_uart_reset_all(void);

/**
 * \brief           Get simulated UART state (for testing)
 * \param[in]       instance: UART instance
 * \return          Pointer to UART state or NULL
 */
native_uart_state_t* native_uart_get_state(int instance);

/**
 * \brief           Inject data into UART RX buffer (for testing)
 * \param[in]       instance: UART instance
 * \param[in]       data: Data to inject
 * \param[in]       len: Length of data
 * \return          true on success, false on failure
 */
bool native_uart_inject_rx_data(int instance, const uint8_t* data, size_t len);

/**
 * \brief           Get data from UART TX buffer (for testing)
 * \param[in]       instance: UART instance
 * \param[out]      data: Buffer to store data
 * \param[in]       max_len: Maximum length to read
 * \return          Number of bytes read
 */
size_t native_uart_get_tx_data(int instance, uint8_t* data, size_t max_len);

/**
 * \brief           Get actual configured baudrate (for testing)
 * \param[in]       instance: UART instance
 * \return          Actual baudrate or 0 if not initialized
 */
uint32_t native_uart_get_actual_baudrate(int instance);

/*===========================================================================*/
/* SPI Test Helpers                                                           */
/*===========================================================================*/

/**
 * \brief           Simulated SPI state (opaque for testing)
 */
typedef struct native_spi_state native_spi_state_t;

/**
 * \brief           Reset all simulated SPI states
 */
void native_spi_reset_all(void);

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
hal_spi_mode_t native_spi_get_mode(int instance);

/**
 * \brief           Get length of last transfer (for testing)
 * \param[in]       instance: SPI instance
 * \return          Length of last transfer
 */
size_t native_spi_get_last_transfer_len(int instance);

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

/*===========================================================================*/
/* I2C Test Helpers                                                           */
/*===========================================================================*/

/**
 * \brief           Simulated I2C state (opaque for testing)
 */
typedef struct native_i2c_state native_i2c_state_t;

/**
 * \brief           Reset all simulated I2C states
 */
void native_i2c_reset_all(void);

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
                                    uint16_t mem_addr, const uint8_t* data, size_t len);

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
                                   uint16_t mem_addr, uint8_t* data, size_t len);

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

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_PLATFORM_H */
