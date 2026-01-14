/**
 * \file            hal_uart.h
 * \brief           HAL UART Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include "hal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        HAL_UART UART Hardware Abstraction
 * \brief           UART interface for hardware abstraction
 * \{
 */

/**
 * \brief           UART instance enumeration
 */
typedef enum {
    HAL_UART_0 = 0, /**< UART instance 0 */
    HAL_UART_1,     /**< UART instance 1 */
    HAL_UART_2,     /**< UART instance 2 */
    HAL_UART_3,     /**< UART instance 3 */
    HAL_UART_MAX    /**< Maximum UART count */
} hal_uart_instance_t;

/**
 * \brief           UART word length
 */
typedef enum {
    HAL_UART_WORDLEN_8 = 0, /**< 8 data bits */
    HAL_UART_WORDLEN_9      /**< 9 data bits */
} hal_uart_wordlen_t;

/**
 * \brief           UART stop bits
 */
typedef enum {
    HAL_UART_STOPBITS_1 = 0, /**< 1 stop bit */
    HAL_UART_STOPBITS_2      /**< 2 stop bits */
} hal_uart_stopbits_t;

/**
 * \brief           UART parity
 */
typedef enum {
    HAL_UART_PARITY_NONE = 0, /**< No parity */
    HAL_UART_PARITY_EVEN,     /**< Even parity */
    HAL_UART_PARITY_ODD       /**< Odd parity */
} hal_uart_parity_t;

/**
 * \brief           UART flow control
 */
typedef enum {
    HAL_UART_FLOWCTRL_NONE = 0, /**< No flow control */
    HAL_UART_FLOWCTRL_RTS,      /**< RTS flow control */
    HAL_UART_FLOWCTRL_CTS,      /**< CTS flow control */
    HAL_UART_FLOWCTRL_RTS_CTS   /**< RTS/CTS flow control */
} hal_uart_flowctrl_t;

/**
 * \brief           UART configuration structure
 */
typedef struct {
    uint32_t baudrate;            /**< Baud rate (e.g., 115200) */
    hal_uart_wordlen_t wordlen;   /**< Word length */
    hal_uart_stopbits_t stopbits; /**< Stop bits */
    hal_uart_parity_t parity;     /**< Parity */
    hal_uart_flowctrl_t flowctrl; /**< Flow control */
} hal_uart_config_t;

/**
 * \brief           UART receive callback function type
 * \param[in]       instance: UART instance
 * \param[in]       data: Received data byte
 * \param[in]       context: User context pointer
 */
typedef void (*hal_uart_rx_callback_t)(hal_uart_instance_t instance,
                                       uint8_t data, void* context);

/**
 * \brief           UART transmit complete callback function type
 * \param[in]       instance: UART instance
 * \param[in]       context: User context pointer
 */
typedef void (*hal_uart_tx_callback_t)(hal_uart_instance_t instance,
                                       void* context);

/**
 * \brief           Initialize UART
 * \param[in]       instance: UART instance
 * \param[in]       config: Pointer to configuration structure
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_uart_init(hal_uart_instance_t instance,
                           const hal_uart_config_t* config);

/**
 * \brief           Deinitialize UART
 * \param[in]       instance: UART instance
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_uart_deinit(hal_uart_instance_t instance);

/**
 * \brief           Transmit data (blocking)
 * \param[in]       instance: UART instance
 * \param[in]       data: Pointer to data buffer
 * \param[in]       len: Number of bytes to transmit
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_uart_transmit(hal_uart_instance_t instance,
                               const uint8_t* data, size_t len,
                               uint32_t timeout_ms);

/**
 * \brief           Receive data (blocking)
 * \param[in]       instance: UART instance
 * \param[out]      data: Pointer to data buffer
 * \param[in]       len: Number of bytes to receive
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_uart_receive(hal_uart_instance_t instance, uint8_t* data,
                              size_t len, uint32_t timeout_ms);

/**
 * \brief           Transmit single byte
 * \param[in]       instance: UART instance
 * \param[in]       byte: Byte to transmit
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_uart_putc(hal_uart_instance_t instance, uint8_t byte);

/**
 * \brief           Receive single byte
 * \param[in]       instance: UART instance
 * \param[out]      byte: Pointer to store received byte
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_uart_getc(hal_uart_instance_t instance, uint8_t* byte,
                           uint32_t timeout_ms);

/**
 * \brief           Register receive callback
 * \param[in]       instance: UART instance
 * \param[in]       callback: Callback function
 * \param[in]       context: User context
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_uart_set_rx_callback(hal_uart_instance_t instance,
                                      hal_uart_rx_callback_t callback,
                                      void* context);

/**
 * \brief           Register transmit complete callback
 * \param[in]       instance: UART instance
 * \param[in]       callback: Callback function
 * \param[in]       context: User context
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_uart_set_tx_callback(hal_uart_instance_t instance,
                                      hal_uart_tx_callback_t callback,
                                      void* context);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
