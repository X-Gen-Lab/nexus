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
typedef struct nx_audio_port nx_audio_port_t;
typedef struct nx_timer_pulse_port nx_timer_pulse_port_t;
typedef struct nx_timer_sequence_port nx_timer_sequence_port_t;
typedef struct nx_capture_port nx_capture_port_t;
typedef struct nx_adc_port nx_adc_port_t;
typedef struct nx_advanced_pwm nx_advanced_pwm_t;
typedef struct nx_block_port nx_block_port_t;
typedef struct nx_can_fd_port nx_can_fd_port_t;
typedef struct nx_can_port nx_can_port_t;
typedef struct nx_dac_port nx_dac_port_t;
typedef struct nx_dac_tx_port nx_dac_tx_port_t;
typedef struct nx_crc_port nx_crc_port_t;
typedef struct nx_encoder nx_encoder_t;
typedef struct nx_entropy_port nx_entropy_port_t;
typedef struct nx_eth_frames_port nx_eth_frames_port_t;
typedef struct nx_eth_port nx_eth_port_t;
typedef struct nx_exti_port nx_exti_port_t;
typedef struct nx_external_bus_port nx_external_bus_port_t;
typedef struct nx_flash_async_port nx_flash_async_port_t;
typedef struct nx_nand_port nx_nand_port_t;
typedef struct nx_flash_port nx_flash_port_t;
typedef struct nx_gpio_port nx_gpio_port_t;
typedef struct nx_i2c_endpoint nx_i2c_endpoint_t;
typedef struct nx_i2c_long_endpoint nx_i2c_long_endpoint_t;
typedef struct nx_i2c_port nx_i2c_port_t;
typedef struct nx_i2c_target_port nx_i2c_target_port_t;
typedef struct nx_lptim_port nx_lptim_port_t;
typedef struct nx_mdio_port nx_mdio_port_t;
typedef struct nx_memory_window_port nx_memory_window_port_t;
typedef struct nx_parallel16_endpoint nx_parallel16_endpoint_t;
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
typedef struct nx_usb_host_port nx_usb_host_port_t;
typedef struct nx_usb_otg_port nx_usb_otg_port_t;
typedef struct nx_watchdog_port nx_watchdog_port_t;

#ifdef __cplusplus
}
#endif

#endif
