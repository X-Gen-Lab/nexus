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

#define MAX_UART_INSTANCES  4

typedef struct {
    bool initialized;
    hal_uart_config_t config;
} uart_state_t;

static uart_state_t uart_state[MAX_UART_INSTANCES];

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_uart_init(hal_uart_instance_t instance,
                           const hal_uart_config_t* config)
{
    if (instance >= MAX_UART_INSTANCES || config == NULL) {
        return HAL_ERR_PARAM;
    }

    uart_state[instance].initialized = true;
    uart_state[instance].config = *config;

    return HAL_OK;
}

hal_status_t hal_uart_deinit(hal_uart_instance_t instance)
{
    if (instance >= MAX_UART_INSTANCES) {
        return HAL_ERR_PARAM;
    }

    uart_state[instance].initialized = false;
    return HAL_OK;
}

hal_status_t hal_uart_transmit(hal_uart_instance_t instance,
                               const uint8_t* data,
                               size_t len,
                               uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (instance >= MAX_UART_INSTANCES || data == NULL) {
        return HAL_ERR_PARAM;
    }

    if (!uart_state[instance].initialized) {
        return HAL_ERR_STATE;
    }

    /* Write to stdout for native platform */
    for (size_t i = 0; i < len; i++) {
        putchar(data[i]);
    }
    fflush(stdout);

    return HAL_OK;
}

hal_status_t hal_uart_receive(hal_uart_instance_t instance,
                              uint8_t* data,
                              size_t len,
                              uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (instance >= MAX_UART_INSTANCES || data == NULL) {
        return HAL_ERR_PARAM;
    }

    if (!uart_state[instance].initialized) {
        return HAL_ERR_STATE;
    }

    /* Read from stdin for native platform */
    for (size_t i = 0; i < len; i++) {
        int c = getchar();
        if (c == EOF) {
            return HAL_ERROR_IO;
        }
        data[i] = (uint8_t)c;
    }

    return HAL_OK;
}

hal_status_t hal_uart_putc(hal_uart_instance_t instance, uint8_t byte)
{
    return hal_uart_transmit(instance, &byte, 1, HAL_WAIT_FOREVER);
}

hal_status_t hal_uart_getc(hal_uart_instance_t instance,
                           uint8_t* byte,
                           uint32_t timeout_ms)
{
    return hal_uart_receive(instance, byte, 1, timeout_ms);
}

hal_status_t hal_uart_set_rx_callback(hal_uart_instance_t instance,
                                      hal_uart_rx_callback_t callback,
                                      void* context)
{
    (void)instance;
    (void)callback;
    (void)context;
    return HAL_ERR_NOT_SUPPORTED;
}

hal_status_t hal_uart_set_tx_callback(hal_uart_instance_t instance,
                                      hal_uart_tx_callback_t callback,
                                      void* context)
{
    (void)instance;
    (void)callback;
    (void)context;
    return HAL_ERR_NOT_SUPPORTED;
}
