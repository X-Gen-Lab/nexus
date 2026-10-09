/**
 * \file            stm32_uart_helpers.h
 * \brief           STM32 UART helper functions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_UART_HELPERS_H
#define STM32_UART_HELPERS_H

#include "hal/nx_status.h"
#include "stm32_uart_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize circular buffer
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       data: Buffer data pointer
 * \param[in]       size: Buffer size
 * \param[in]       policy: Overflow policy
 */
void stm32_uart_buffer_init(stm32_uart_buffer_t* buf, uint8_t* data,
                            size_t size, uart_overflow_policy_t policy);

/**
 * \brief           Set buffer watermarks
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       high_percent: High watermark percentage (0-100)
 * \param[in]       low_percent: Low watermark percentage (0-100)
 */
void stm32_uart_buffer_set_watermarks(stm32_uart_buffer_t* buf,
                                      uint8_t high_percent,
                                      uint8_t low_percent);

/**
 * \brief           Get number of bytes in buffer
 * \param[in]       buf: Buffer structure pointer
 * \return          Number of bytes available
 */
static inline size_t
stm32_uart_buffer_get_count(const stm32_uart_buffer_t* buf) {
    return buf ? buf->count : 0;
}

/**
 * \brief           Get free space in buffer
 * \param[in]       buf: Buffer structure pointer
 * \return          Number of bytes free
 */
static inline size_t
stm32_uart_buffer_get_free(const stm32_uart_buffer_t* buf) {
    return buf ? (buf->size - buf->count) : 0;
}

/**
 * \brief           Check if buffer is empty
 * \param[in]       buf: Buffer structure pointer
 * \return          true if empty, false otherwise
 */
static inline bool stm32_uart_buffer_is_empty(const stm32_uart_buffer_t* buf) {
    return buf ? (buf->count == 0) : true;
}

/**
 * \brief           Check if buffer is full
 * \param[in]       buf: Buffer structure pointer
 * \return          true if full, false otherwise
 */
static inline bool stm32_uart_buffer_is_full(const stm32_uart_buffer_t* buf) {
    return buf ? (buf->count >= buf->size) : false;
}

/**
 * \brief           Write data to circular buffer (optimized with memcpy)
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       data: Data to write
 * \param[in]       len: Data length
 * \return          Number of bytes written
 */
size_t stm32_uart_buffer_write(stm32_uart_buffer_t* buf, const uint8_t* data,
                               size_t len);

/**
 * \brief           Write data to circular buffer with interrupt protection
 * \param[in]       buf: Buffer structure pointer
 * \param[in]       data: Data to write
 * \param[in]       len: Data length
 * \return          Number of bytes written
 */
size_t stm32_uart_buffer_write_safe(stm32_uart_buffer_t* buf,
                                    const uint8_t* data, size_t len);

/**
 * \brief           Read data from circular buffer (optimized with memcpy)
 * \param[in]       buf: Buffer structure pointer
 * \param[out]      data: Data buffer
 * \param[in]       len: Maximum bytes to read
 * \return          Number of bytes read
 */
size_t stm32_uart_buffer_read(stm32_uart_buffer_t* buf, uint8_t* data,
                              size_t len);

/**
 * \brief           Read data from circular buffer with interrupt protection
 * \param[in]       buf: Buffer structure pointer
 * \param[out]      data: Data buffer
 * \param[in]       len: Maximum bytes to read
 * \return          Number of bytes read
 */
size_t stm32_uart_buffer_read_safe(stm32_uart_buffer_t* buf, uint8_t* data,
                                   size_t len);

/**
 * \brief           Flush buffer (clear all data)
 * \param[in]       buf: Buffer structure pointer
 */
void stm32_uart_buffer_flush(stm32_uart_buffer_t* buf);

/**
 * \brief           Get buffer statistics
 * \param[in]       buf: Buffer structure pointer
 * \param[out]      peak_usage: Peak buffer usage
 * \param[out]      overflow_count: Overflow event count
 */
void stm32_uart_buffer_get_stats(const stm32_uart_buffer_t* buf,
                                 size_t* peak_usage, uint32_t* overflow_count);

/**
 * \brief           Get UART IRQ number from instance index
 * \param[in]       instance: UART instance index
 * \return          IRQ number or -1 if invalid
 */
IRQn_Type stm32_uart_get_irq_number(uint8_t instance);

/**
 * \brief           Get peripheral type from base address
 * \param[in]       instance: UART/USART/LPUART base address
 * \return          Peripheral type
 */
uart_periph_type_t uart_get_periph_type(USART_TypeDef* instance);

/**
 * \brief           Convert HAL status to Nexus status
 * \param[in]       hal_status: HAL status code
 * \return          Nexus status code
 */
nx_status_t stm32_uart_hal_to_nx_status(HAL_StatusTypeDef hal_status);

/**
 * \brief           Attempt to recover from UART error
 * \param[in]       impl: UART implementation pointer
 * \param[in]       error_code: HAL error code
 * \return          Status code (NX_OK if recovered, error otherwise)
 */
nx_status_t uart_error_recovery(stm32_uart_impl_t* impl, uint32_t error_code);

/*---------------------------------------------------------------------------*/
/* Debug Macros                                                              */
/*---------------------------------------------------------------------------*/

#ifdef NX_CONFIG_STM32_UART_DEBUG
#include <stdio.h>
#define UART_DEBUG(fmt, ...) printf("[UART] " fmt "\n", ##__VA_ARGS__)
#else
#define UART_DEBUG(fmt, ...)
#endif

/*---------------------------------------------------------------------------*/
/* Performance Hints                                                         */
/*---------------------------------------------------------------------------*/

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

#ifdef NX_CONFIG_STM32_UART_TEST_SUPPORT

/**
 * \brief           Inject data into RX buffer (for testing)
 * \param[in]       uart: UART interface pointer
 * \param[in]       data: Data to inject
 * \param[in]       len: Data length
 * \return          Status code
 */
nx_status_t stm32_uart_test_inject_rx(nx_uart_t* uart, const uint8_t* data,
                                      size_t len);

/**
 * \brief           Get buffer state (for testing)
 * \param[in]       uart: UART interface pointer
 * \param[out]      tx_count: TX buffer count
 * \param[out]      rx_count: RX buffer count
 * \return          Status code
 */
nx_status_t stm32_uart_test_get_buffer_state(nx_uart_t* uart, size_t* tx_count,
                                             size_t* rx_count);

/**
 * \brief           Simulate error condition (for testing)
 * \param[in]       uart: UART interface pointer
 * \param[in]       error_code: Error code to simulate
 * \return          Status code
 */
nx_status_t stm32_uart_test_simulate_error(nx_uart_t* uart,
                                           uint32_t error_code);

#endif /* NX_CONFIG_STM32_UART_TEST_SUPPORT */

#ifdef __cplusplus
}
#endif

#endif /* STM32_UART_HELPERS_H */
