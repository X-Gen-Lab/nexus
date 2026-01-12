/**
 * \file            hal_uart_native.c
 * \brief           Native Platform UART HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/hal_uart.h"
#include <stdio.h>

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

#define MAX_UART_PORTS  3

typedef struct {
    bool initialized;
    hal_uart_config_t config;
} uart_state_t;

static uart_state_t uart_state[MAX_UART_PORTS];

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_uart_init(hal_uart_port_t port, const hal_uart_config_t* config)
{
    if (port >= MAX_UART_PORTS || config == NULL) {
        return HAL_ERR_PARAM;
    }

    uart_state[port].initialized = true;
    uart_state[port].config = *config;

    return HAL_OK;
}

hal_status_t hal_uart_deinit(hal_uart_port_t port)
{
    if (port >= MAX_UART_PORTS) {
        return HAL_ERR_PARAM;
    }

    uart_state[port].initialized = false;
    return HAL_OK;
}

hal_status_t hal_uart_write(hal_uart_port_t port,
                            const uint8_t* data,
                            size_t len,
                            uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (port >= MAX_UART_PORTS || data == NULL) {
        return HAL_ERR_PARAM;
    }

    if (!uart_state[port].initialized) {
        return HAL_ERR_STATE;
    }

    /* Write to stdout for native platform */
    for (size_t i = 0; i < len; i++) {
        putchar(data[i]);
    }
    fflush(stdout);

    return HAL_OK;
}

hal_status_t hal_uart_read(hal_uart_port_t port,
                           uint8_t* data,
                           size_t len,
                           size_t* actual_len,
                           uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (port >= MAX_UART_PORTS || data == NULL) {
        return HAL_ERR_PARAM;
    }

    if (!uart_state[port].initialized) {
        return HAL_ERR_STATE;
    }

    /* Read from stdin for native platform */
    size_t count = 0;
    for (size_t i = 0; i < len; i++) {
        int c = getchar();
        if (c == EOF) {
            break;
        }
        data[i] = (uint8_t)c;
        count++;
    }

    if (actual_len != NULL) {
        *actual_len = count;
    }

    return HAL_OK;
}

hal_status_t hal_uart_write_byte(hal_uart_port_t port, uint8_t byte)
{
    return hal_uart_write(port, &byte, 1, HAL_WAIT_FOREVER);
}

hal_status_t hal_uart_read_byte(hal_uart_port_t port,
                                uint8_t* byte,
                                uint32_t timeout_ms)
{
    return hal_uart_read(port, byte, 1, NULL, timeout_ms);
}

hal_status_t hal_uart_set_callback(hal_uart_port_t port,
                                   hal_uart_rx_callback_t rx_cb,
                                   hal_uart_tx_callback_t tx_cb,
                                   void* rx_ctx,
                                   void* tx_ctx)
{
    (void)port;
    (void)rx_cb;
    (void)tx_cb;
    (void)rx_ctx;
    (void)tx_ctx;
    return HAL_ERR_NOT_SUPPORTED;
}

hal_status_t hal_uart_flush(hal_uart_port_t port)
{
    if (port >= MAX_UART_PORTS) {
        return HAL_ERR_PARAM;
    }

    fflush(stdout);
    return HAL_OK;
}
