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

#include "hal/hal_adc.h"
#include "hal/hal_spi.h"
#include "hal/hal_timer.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Simulated GPIO state
 */
typedef struct {
    bool configured; /**< Pin configured flag */
    bool is_output;  /**< Output mode flag */
    bool level;      /**< Current level */
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

/*===========================================================================*/
/* Timer Test Helpers                                                         */
/*===========================================================================*/

/**
 * \brief           Simulated Timer state (opaque for testing)
 */
typedef struct native_timer_state native_timer_state_t;

/**
 * \brief           Reset all simulated Timer states
 */
void native_timer_reset_all(void);

/**
 * \brief           Get simulated Timer state (for testing)
 * \param[in]       instance: Timer instance
 * \return          Pointer to Timer state or NULL
 */
native_timer_state_t* native_timer_get_state(int instance);

/**
 * \brief           Check if Timer instance is initialized
 * \param[in]       instance: Timer instance
 * \return          true if initialized, false otherwise
 */
bool native_timer_is_initialized(int instance);

/**
 * \brief           Check if Timer instance is running
 * \param[in]       instance: Timer instance
 * \return          true if running, false otherwise
 */
bool native_timer_is_running(int instance);

/**
 * \brief           Get configured period in microseconds (for testing)
 * \param[in]       instance: Timer instance
 * \return          Period in microseconds or 0 if not initialized
 */
uint32_t native_timer_get_period_us(int instance);

/**
 * \brief           Get configured timer mode (for testing)
 * \param[in]       instance: Timer instance
 * \return          Timer mode
 */
hal_timer_mode_t native_timer_get_mode(int instance);

/**
 * \brief           Get callback invocation count (for testing)
 * \param[in]       instance: Timer instance
 * \return          Number of times callback was invoked
 */
uint32_t native_timer_get_callback_count(int instance);

/**
 * \brief           Simulate timer period elapsed (for testing)
 * \param[in]       instance: Timer instance
 * \return          true if callback was invoked, false otherwise
 *
 * This function simulates the timer period expiring and invokes
 * the registered callback. For ONESHOT mode, the timer stops.
 * For PERIODIC mode, the timer continues running.
 */
bool native_timer_simulate_period_elapsed(int instance);

/*===========================================================================*/
/* PWM Test Helpers                                                           */
/*===========================================================================*/

/**
 * \brief           Check if PWM channel is initialized
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          true if initialized, false otherwise
 */
bool native_pwm_is_initialized(int instance, int channel);

/**
 * \brief           Check if PWM channel is running
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          true if running, false otherwise
 */
bool native_pwm_is_running(int instance, int channel);

/**
 * \brief           Get PWM frequency (for testing)
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          PWM frequency in Hz or 0 if not initialized
 */
uint32_t native_pwm_get_frequency(int instance, int channel);

/**
 * \brief           Get PWM duty cycle (for testing)
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          Duty cycle (0-10000) or 0 if not initialized
 */
uint16_t native_pwm_get_duty_cycle(int instance, int channel);

/*===========================================================================*/
/* ADC Test Helpers                                                           */
/*===========================================================================*/

/**
 * \brief           Simulated ADC state (opaque for testing)
 */
typedef struct native_adc_state native_adc_state_t;

/**
 * \brief           Reset all simulated ADC states
 */
void native_adc_reset_all(void);

/**
 * \brief           Get simulated ADC state (for testing)
 * \param[in]       instance: ADC instance
 * \return          Pointer to ADC state or NULL
 */
native_adc_state_t* native_adc_get_state(int instance);

/**
 * \brief           Check if ADC instance is initialized
 * \param[in]       instance: ADC instance
 * \return          true if initialized, false otherwise
 */
bool native_adc_is_initialized(int instance);

/**
 * \brief           Get configured ADC resolution (for testing)
 * \param[in]       instance: ADC instance
 * \return          ADC resolution
 */
hal_adc_resolution_t native_adc_get_resolution(int instance);

/**
 * \brief           Set simulated ADC value for a channel (for testing)
 * \param[in]       instance: ADC instance
 * \param[in]       channel: ADC channel
 * \param[in]       value: Value to set
 * \return          true on success, false on failure
 */
bool native_adc_set_simulated_value(int instance, uint8_t channel,
                                    uint16_t value);

/**
 * \brief           Get simulated ADC value for a channel (for testing)
 * \param[in]       instance: ADC instance
 * \param[in]       channel: ADC channel
 * \return          Simulated value
 */
uint16_t native_adc_get_simulated_value(int instance, uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_PLATFORM_H */
