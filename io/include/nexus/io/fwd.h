/**
 * \file            fwd.h
 * \brief           Lightweight identities for typed I/O faces
 * \author          Nexus Team
 * \note            Include the matching public header before accessing its
 *                  layout, operations, requests or value types. Forward
 *                  identities neither construct nor acquire any device.
 */
#ifndef NEXUS_IO_FWD_H
#define NEXUS_IO_FWD_H

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Opaque identity; complete contracts live in its public header. */
typedef struct nx_adc_group_port nx_adc_group_port_t;
typedef struct nx_adc_port nx_adc_port_t;
typedef struct nx_advanced_pwm nx_advanced_pwm_t;
typedef struct nx_can_fd_port nx_can_fd_port_t;
typedef struct nx_can_port nx_can_port_t;
typedef struct nx_encoder nx_encoder_t;
typedef struct nx_eth_frames_port nx_eth_frames_port_t;
typedef struct nx_eth_port nx_eth_port_t;
typedef struct nx_exti_port nx_exti_port_t;
typedef struct nx_flash_port nx_flash_port_t;
typedef struct nx_gpio_port nx_gpio_port_t;
typedef struct nx_i2c_endpoint nx_i2c_endpoint_t;
typedef struct nx_i2c_long_endpoint nx_i2c_long_endpoint_t;
typedef struct nx_i2c_port nx_i2c_port_t;
typedef struct nx_lptim_port nx_lptim_port_t;
typedef struct nx_mdio_port nx_mdio_port_t;
typedef struct nx_pin_state_port nx_pin_state_port_t;
typedef struct nx_pwm_adc nx_pwm_adc_t;
typedef struct nx_pwm_group nx_pwm_group_t;
typedef struct nx_pwm_port nx_pwm_port_t;
typedef struct nx_rtc_port nx_rtc_port_t;
typedef struct nx_spi_dma_transaction_endpoint
    nx_spi_dma_transaction_endpoint_t;
typedef struct nx_spi_dma_transaction_port nx_spi_dma_transaction_port_t;
typedef struct nx_spi_endpoint nx_spi_endpoint_t;
typedef struct nx_spi_port nx_spi_port_t;
typedef struct nx_spi_transaction_endpoint nx_spi_transaction_endpoint_t;
typedef struct nx_timer_capture nx_timer_capture_t;
typedef struct nx_uart_port nx_uart_port_t;
typedef struct nx_uart_word_port nx_uart_word_port_t;
typedef struct nx_usb_activation_port nx_usb_activation_port_t;
typedef struct nx_usb_device_port nx_usb_device_port_t;
typedef struct nx_usb_endpoint_port nx_usb_endpoint_port_t;
typedef struct nx_watchdog_port nx_watchdog_port_t;

#ifdef __cplusplus
}
#endif

#endif
