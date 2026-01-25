/**
 * \file            nx_factory.h
 * \brief           Device factory interface (thin wrappers)
 * \author          Nexus Team
 *
 * \details         This file provides thin wrapper functions around
 *                  nx_device for convenient device access.
 *                  All functions are static inline for zero overhead.
 */

#ifndef NX_FACTORY_H
#define NX_FACTORY_H

#include "hal/base/nx_device.h"
#include "hal/interface/nx_adc.h"
#include "hal/interface/nx_can.h"
#include "hal/interface/nx_crc.h"
#include "hal/interface/nx_dac.h"
#include "hal/interface/nx_flash.h"
#include "hal/interface/nx_gpio.h"
#include "hal/interface/nx_i2c.h"
#include "hal/interface/nx_option_bytes.h"
#include "hal/interface/nx_rtc.h"
#include "hal/interface/nx_sdio.h"
#include "hal/interface/nx_spi.h"
#include "hal/interface/nx_timer.h"
#include "hal/interface/nx_uart.h"
#include "hal/interface/nx_usb.h"
#include "hal/interface/nx_watchdog.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* GPIO Factory Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get GPIO device
 * \param[in]       port: GPIO port character ('A', 'B', etc.)
 * \param[in]       pin: GPIO pin number
 * \return          GPIO interface pointer, NULL on failure
 */
static inline nx_gpio_t* nx_factory_gpio(char port, uint8_t pin) {
    char name[16];
    snprintf(name, sizeof(name), "GPIO%c%d", port, pin);
    return (nx_gpio_t*)nx_device_get(name);
}

/**
 * \brief           Get GPIO read interface
 * \param[in]       port: GPIO port character ('A', 'B', etc.)
 * \param[in]       pin: GPIO pin number
 * \return          GPIO read interface pointer, NULL on failure
 */
static inline nx_gpio_read_t* nx_factory_gpio_read(char port, uint8_t pin) {
    char name[16];
    snprintf(name, sizeof(name), "GPIO%c%d", port, pin);
    return (nx_gpio_read_t*)nx_device_get(name);
}

/**
 * \brief           Get GPIO write interface
 * \param[in]       port: GPIO port character ('A', 'B', etc.)
 * \param[in]       pin: GPIO pin number
 * \return          GPIO write interface pointer, NULL on failure
 */
static inline nx_gpio_write_t* nx_factory_gpio_write(char port, uint8_t pin) {
    char name[16];
    snprintf(name, sizeof(name), "GPIO%c%d", port, pin);
    return (nx_gpio_write_t*)nx_device_get(name);
}

/**
 * \brief           Get GPIO read-write interface
 * \param[in]       port: GPIO port character ('A', 'B', etc.)
 * \param[in]       pin: GPIO pin number
 * \return          GPIO read-write interface pointer, NULL on failure
 */
static inline nx_gpio_read_write_t* nx_factory_gpio_read_write(char port,
                                                               uint8_t pin) {
    char name[16];
    snprintf(name, sizeof(name), "GPIO%c%d", port, pin);
    return (nx_gpio_read_write_t*)nx_device_get(name);
}

/**
 * \brief           Release GPIO device (no-op)
 * \param[in]       gpio: GPIO interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_gpio_release(nx_gpio_t* gpio) {
    (void)gpio; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* UART Factory Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get UART device
 * \param[in]       index: UART index
 * \return          UART interface pointer, NULL on failure
 */
static inline nx_uart_t* nx_factory_uart(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "UART%d", index);
    return (nx_uart_t*)nx_device_get(name);
}

/**
 * \brief           Release UART device (no-op)
 * \param[in]       uart: UART interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_uart_release(nx_uart_t* uart) {
    (void)uart; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* SPI Factory Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SPI device
 * \param[in]       index: SPI index
 * \return          SPI interface pointer, NULL on failure
 */
static inline nx_spi_t* nx_factory_spi(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "SPI%d", index);
    return (nx_spi_t*)nx_device_get(name);
}

/**
 * \brief           Release SPI device (no-op)
 * \param[in]       spi: SPI interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_spi_release(nx_spi_t* spi) {
    (void)spi; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* I2C Factory Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get I2C device
 * \param[in]       index: I2C index
 * \return          I2C interface pointer, NULL on failure
 */
static inline nx_i2c_t* nx_factory_i2c(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "I2C%d", index);
    return (nx_i2c_t*)nx_device_get(name);
}

/**
 * \brief           Release I2C device (no-op)
 * \param[in]       i2c: I2C interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_i2c_release(nx_i2c_t* i2c) {
    (void)i2c; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* Timer Factory Functions                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Timer base device
 * \param[in]       index: Timer index
 * \return          Timer base interface pointer, NULL on failure
 */
static inline nx_timer_base_t* nx_factory_timer(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "TIMER%d", index);
    return (nx_timer_base_t*)nx_device_get(name);
}

/**
 * \brief           Get Timer PWM device
 * \param[in]       index: Timer index
 * \return          Timer PWM interface pointer, NULL on failure
 */
static inline nx_timer_pwm_t* nx_factory_timer_pwm(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "TIMER%d", index);
    return (nx_timer_pwm_t*)nx_device_get(name);
}

/**
 * \brief           Get Timer encoder device
 * \param[in]       index: Timer index
 * \return          Timer encoder interface pointer, NULL on failure
 */
static inline nx_timer_encoder_t* nx_factory_timer_encoder(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "TIMER%d", index);
    return (nx_timer_encoder_t*)nx_device_get(name);
}

/**
 * \brief           Release Timer base device (no-op)
 * \param[in]       timer: Timer base interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_timer_release(nx_timer_base_t* timer) {
    (void)timer; /* Unused */
}

/**
 * \brief           Release Timer PWM device (no-op)
 * \param[in]       timer: Timer PWM interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_timer_pwm_release(nx_timer_pwm_t* timer) {
    (void)timer; /* Unused */
}

/**
 * \brief           Release Timer encoder device (no-op)
 * \param[in]       timer: Timer encoder interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_timer_encoder_release(nx_timer_encoder_t* timer) {
    (void)timer; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* ADC Factory Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC device
 * \param[in]       index: ADC index
 * \return          ADC interface pointer, NULL on failure
 */
static inline nx_adc_t* nx_factory_adc(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "ADC%d", index);
    return (nx_adc_t*)nx_device_get(name);
}

/**
 * \brief           Release ADC device (no-op)
 * \param[in]       adc: ADC interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_adc_release(nx_adc_t* adc) {
    (void)adc; /* Unused */
}

/**
 * \brief           Get ADC buffer device
 * \param[in]       index: ADC index
 * \param[in]       buffer_size: Buffer size in samples
 * \return          ADC buffer interface pointer, NULL on failure
 */
static inline nx_adc_buffer_t* nx_factory_adc_buffer(uint8_t index,
                                                     size_t buffer_size) {
    char name[16];
    snprintf(name, sizeof(name), "ADC%d_BUF", index);
    (void)buffer_size; /* Buffer size handled by device configuration */
    return (nx_adc_buffer_t*)nx_device_get(name);
}

/**
 * \brief           Release ADC buffer device (no-op)
 * \param[in]       adc_buf: ADC buffer interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_adc_buffer_release(nx_adc_buffer_t* adc_buf) {
    (void)adc_buf; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* Internal Flash Factory Functions                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get internal flash device
 * \param[in]       index: Flash index
 * \return          Internal flash interface pointer, NULL on failure
 */
static inline nx_internal_flash_t* nx_factory_flash(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "FLASH%d", index);
    return (nx_internal_flash_t*)nx_device_get(name);
}

/**
 * \brief           Release internal flash device (no-op)
 * \param[in]       flash: Internal flash interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_flash_release(nx_internal_flash_t* flash) {
    (void)flash; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* CAN Factory Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get CAN bus device
 * \param[in]       index: CAN index
 * \return          CAN bus interface pointer, NULL on failure
 */
static inline nx_can_bus_t* nx_factory_can(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "CAN%d", index);
    return (nx_can_bus_t*)nx_device_get(name);
}

/**
 * \brief           Release CAN bus device (no-op)
 * \param[in]       can: CAN bus interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_can_release(nx_can_bus_t* can) {
    (void)can; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* USB Factory Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get USB device
 * \return          USB interface pointer, NULL on failure
 */
static inline nx_usb_t* nx_factory_usb(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "USB%d", index);
    return (nx_usb_t*)nx_device_get(name);
}

/**
 * \brief           Release USB device (no-op)
 * \param[in]       usb: USB interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_usb_release(nx_usb_t* usb) {
    (void)usb; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* RTC Factory Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get RTC device
 * \param[in]       index: RTC index
 * \return          RTC interface pointer, NULL on failure
 */
static inline nx_rtc_t* nx_factory_rtc(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "RTC%d", index);
    return (nx_rtc_t*)nx_device_get(name);
}

/**
 * \brief           Release RTC device (no-op)
 * \param[in]       rtc: RTC interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_rtc_release(nx_rtc_t* rtc) {
    (void)rtc; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* Watchdog Factory Functions                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get watchdog device
 * \param[in]       index: Watchdog index
 * \return          Watchdog interface pointer, NULL on failure
 */
static inline nx_watchdog_t* nx_factory_watchdog(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "WATCHDOG%d", index);
    return (nx_watchdog_t*)nx_device_get(name);
}

/**
 * \brief           Release watchdog device (no-op)
 * \param[in]       watchdog: Watchdog interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_watchdog_release(nx_watchdog_t* watchdog) {
    (void)watchdog; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* DAC Factory Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC device
 * \param[in]       index: DAC index
 * \return          DAC interface pointer, NULL on failure
 */
static inline nx_dac_t* nx_factory_dac(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "DAC%d", index);
    return (nx_dac_t*)nx_device_get(name);
}

/**
 * \brief           Release DAC device (no-op)
 * \param[in]       dac: DAC interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_dac_release(nx_dac_t* dac) {
    (void)dac; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* SDIO Factory Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SDIO device
 * \param[in]       index: SDIO index
 * \return          SDIO interface pointer, NULL on failure
 */
static inline nx_sdio_t* nx_factory_sdio(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "SDIO%d", index);
    return (nx_sdio_t*)nx_device_get(name);
}

/**
 * \brief           Release SDIO device (no-op)
 * \param[in]       sdio: SDIO interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_sdio_release(nx_sdio_t* sdio) {
    (void)sdio; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* CRC Factory Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get CRC device
 * \param[in]       index: CRC index
 * \return          CRC interface pointer, NULL on failure
 */
static inline nx_crc_t* nx_factory_crc(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "CRC%d", index);
    return (nx_crc_t*)nx_device_get(name);
}

/**
 * \brief           Release CRC device (no-op)
 * \param[in]       crc: CRC interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void nx_factory_crc_release(nx_crc_t* crc) {
    (void)crc; /* Unused */
}

/*---------------------------------------------------------------------------*/
/* Option Bytes Factory Functions                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get option bytes device
 * \param[in]       index: Option bytes index
 * \return          Option bytes interface pointer, NULL on failure
 */
static inline nx_option_bytes_t* nx_factory_option_bytes(uint8_t index) {
    char name[16];
    snprintf(name, sizeof(name), "OPTBYTES%d", index);
    return (nx_option_bytes_t*)nx_device_get(name);
}

/**
 * \brief           Release option bytes device (no-op)
 * \param[in]       option_bytes: Option bytes interface pointer
 * \note            No reference counting, this is a no-op
 */
static inline void
nx_factory_option_bytes_release(nx_option_bytes_t* option_bytes) {
    (void)option_bytes; /* Unused */
}

#ifdef __cplusplus
}
#endif

#endif /* NX_FACTORY_H */
