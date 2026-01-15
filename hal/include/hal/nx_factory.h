/**
 * \file            nx_factory.h
 * \brief           Device factory interface
 * \author          Nexus Team
 */

#ifndef NX_FACTORY_H
#define NX_FACTORY_H

#include "hal/base/nx_device.h"
#include "hal/interface/nx_adc.h"
#include "hal/interface/nx_gpio.h"
#include "hal/interface/nx_i2c.h"
#include "hal/interface/nx_spi.h"
#include "hal/interface/nx_timer.h"
#include "hal/interface/nx_uart.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Device information structure
 */
typedef struct nx_device_info_s {
    const char* name;        /**< Device name */
    const char* type;        /**< Device type */
    nx_device_state_t state; /**< Device state */
    uint8_t ref_count;       /**< Reference count */
} nx_device_info_t;

/* ========== GPIO Factory Functions ========== */

/**
 * \brief           Get GPIO device
 * \param[in]       port: GPIO port number
 * \param[in]       pin: GPIO pin number
 * \return          GPIO interface pointer, NULL on failure
 */
nx_gpio_t* nx_factory_gpio(uint8_t port, uint8_t pin);

/**
 * \brief           Get GPIO device with configuration
 * \param[in]       port: GPIO port number
 * \param[in]       pin: GPIO pin number
 * \param[in]       cfg: GPIO configuration
 * \return          GPIO interface pointer, NULL on failure
 */
nx_gpio_t* nx_factory_gpio_with_config(uint8_t port, uint8_t pin,
                                       const nx_gpio_config_t* cfg);

/**
 * \brief           Release GPIO device
 * \param[in]       gpio: GPIO interface pointer
 */
void nx_factory_gpio_release(nx_gpio_t* gpio);

/* ========== UART Factory Functions ========== */

/**
 * \brief           Get UART device
 * \param[in]       index: UART index
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t* nx_factory_uart(uint8_t index);

/**
 * \brief           Get UART device with configuration
 * \param[in]       index: UART index
 * \param[in]       cfg: UART configuration
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t* nx_factory_uart_with_config(uint8_t index,
                                       const nx_uart_config_t* cfg);

/**
 * \brief           Release UART device
 * \param[in]       uart: UART interface pointer
 */
void nx_factory_uart_release(nx_uart_t* uart);

/* ========== SPI Factory Functions ========== */

/**
 * \brief           Get SPI device
 * \param[in]       index: SPI index
 * \return          SPI interface pointer, NULL on failure
 */
nx_spi_t* nx_factory_spi(uint8_t index);

/**
 * \brief           Get SPI device with configuration
 * \param[in]       index: SPI index
 * \param[in]       cfg: SPI configuration
 * \return          SPI interface pointer, NULL on failure
 */
nx_spi_t* nx_factory_spi_with_config(uint8_t index, const nx_spi_config_t* cfg);

/**
 * \brief           Release SPI device
 * \param[in]       spi: SPI interface pointer
 */
void nx_factory_spi_release(nx_spi_t* spi);

/* ========== I2C Factory Functions ========== */

/**
 * \brief           Get I2C device
 * \param[in]       index: I2C index
 * \return          I2C interface pointer, NULL on failure
 */
nx_i2c_t* nx_factory_i2c(uint8_t index);

/**
 * \brief           Get I2C device with configuration
 * \param[in]       index: I2C index
 * \param[in]       cfg: I2C configuration
 * \return          I2C interface pointer, NULL on failure
 */
nx_i2c_t* nx_factory_i2c_with_config(uint8_t index, const nx_i2c_config_t* cfg);

/**
 * \brief           Release I2C device
 * \param[in]       i2c: I2C interface pointer
 */
void nx_factory_i2c_release(nx_i2c_t* i2c);

/* ========== Timer Factory Functions ========== */

/**
 * \brief           Get Timer device
 * \param[in]       index: Timer index
 * \return          Timer interface pointer, NULL on failure
 */
nx_timer_t* nx_factory_timer(uint8_t index);

/**
 * \brief           Get Timer device with configuration
 * \param[in]       index: Timer index
 * \param[in]       cfg: Timer configuration
 * \return          Timer interface pointer, NULL on failure
 */
nx_timer_t* nx_factory_timer_with_config(uint8_t index,
                                         const nx_timer_config_t* cfg);

/**
 * \brief           Release Timer device
 * \param[in]       timer: Timer interface pointer
 */
void nx_factory_timer_release(nx_timer_t* timer);

/* ========== ADC Factory Functions ========== */

/**
 * \brief           Get ADC device
 * \param[in]       index: ADC index
 * \return          ADC interface pointer, NULL on failure
 */
nx_adc_t* nx_factory_adc(uint8_t index);

/**
 * \brief           Get ADC device with configuration
 * \param[in]       index: ADC index
 * \param[in]       cfg: ADC configuration
 * \return          ADC interface pointer, NULL on failure
 */
nx_adc_t* nx_factory_adc_with_config(uint8_t index, const nx_adc_config_t* cfg);

/**
 * \brief           Release ADC device
 * \param[in]       adc: ADC interface pointer
 */
void nx_factory_adc_release(nx_adc_t* adc);

/* ========== Device Enumeration ========== */

/**
 * \brief           Enumerate all devices
 * \param[out]      list: Device information list buffer
 * \param[in]       max_count: Maximum number of devices to enumerate
 * \return          Number of devices enumerated
 */
size_t nx_factory_enumerate(nx_device_info_t* list, size_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* NX_FACTORY_H */
