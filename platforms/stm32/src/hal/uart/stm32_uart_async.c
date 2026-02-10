/**
 * \file            stm32_uart_async.c
 * \brief           STM32 UART async interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-03
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART asynchronous operations with
 * interrupt-driven data transfer and circular buffer management.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "stm32_uart_dma.h"
#include "stm32_uart_helpers.h"
#include "stm32_uart_types.h"

/*---------------------------------------------------------------------------*/
/* TX Async Interface Implementation                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Asynchronous send implementation
 * \details         Direct delegation to HAL_UART_Transmit_IT() or
 *                  HAL_UART_Transmit_DMA(). Zero-copy operation.
 */
static nx_status_t tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len) {
    stm32_uart_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_uart_impl_t, tx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }
    if (!data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Acquire mutex */
    if (osal_mutex_lock(impl->mutex, OSAL_WAIT_FOREVER) != OSAL_OK) {
        return NX_ERR_TIMEOUT;
    }
#endif

    HAL_StatusTypeDef hal_status;

#ifdef NX_CONFIG_STM32_UART_USE_DMA
    /* Use DMA if enabled (zero-copy) */
    if (impl->dma.dma_tx_enabled) {
        hal_status =
            HAL_UART_Transmit_DMA(&impl->huart, (uint8_t*)data, (uint16_t)len);
    } else
#endif
    {
        /* Use interrupt mode (zero-copy) */
        hal_status =
            HAL_UART_Transmit_IT(&impl->huart, (uint8_t*)data, (uint16_t)len);
    }

    /* Map HAL status to Nexus status */
    nx_status_t status = stm32_uart_hal_to_nx_status(hal_status);

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Release mutex */
    osal_mutex_unlock(impl->mutex);
#endif

    return status;
}

/**
 * \brief           Get TX async state
 * \details         Checks huart.gState for BUSY_TX status.
 */
static nx_status_t tx_async_get_state(nx_tx_async_t* self) {
    stm32_uart_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_uart_impl_t, tx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check HAL UART state for TX busy */
    HAL_UART_StateTypeDef state = HAL_UART_GetState(&impl->huart);

    /* Return NX_ERR_BUSY if transmitting, NX_OK if idle */
    if (state == HAL_UART_STATE_BUSY_TX || state == HAL_UART_STATE_BUSY_TX_RX) {
        return NX_ERR_BUSY;
    }

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* RX Async Interface Implementation                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Asynchronous receive implementation
 * \details         Reads from circular buffer populated by HAL RX callback.
 *                  Returns NX_ERR_NO_DATA when buffer empty.
 */
static nx_status_t rx_async_receive(nx_rx_async_t* self, uint8_t* data,
                                    size_t* len) {
    stm32_uart_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_uart_impl_t, rx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }
    if (!data || !len) {
        return NX_ERR_INVALID_PARAM;
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Acquire mutex */
    if (osal_mutex_lock(impl->mutex, OSAL_WAIT_FOREVER) != OSAL_OK) {
        return NX_ERR_TIMEOUT;
    }
#endif

    /* Read from circular buffer (Nexus value-add) */
    size_t read_count =
        stm32_uart_buffer_read_safe(&impl->state->rx_buf, data, *len);
    *len = read_count;

    /* Data received */
    (void)read_count;

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Release mutex */
    osal_mutex_unlock(impl->mutex);
#endif

    return (read_count > 0) ? NX_OK : NX_ERR_NO_DATA;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX async interface
 */
void uart_init_tx_async(nx_tx_async_t* tx_async) {
    tx_async->send = tx_async_send;
    tx_async->get_state = tx_async_get_state;
}

/**
 * \brief           Initialize RX async interface
 */
void uart_init_rx_async(nx_rx_async_t* rx_async) {
    rx_async->receive = rx_async_receive;
}
