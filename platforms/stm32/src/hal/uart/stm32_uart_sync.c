/**
 * \file            stm32_uart_sync.c
 * \brief           STM32 UART sync interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-03
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART synchronous operations including blocking
 *                  TX/RX with timeout support and circular buffer management.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "stm32_uart_helpers.h"
#include "stm32_uart_types.h"

/*---------------------------------------------------------------------------*/
/* TX Sync Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Synchronous send implementation
 * \details         Direct delegation to HAL_UART_Transmit() (blocking).
 *                  Zero-copy operation - passes data pointer directly to HAL.
 */
static nx_status_t tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                size_t len, uint32_t timeout_ms) {
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, tx_sync);

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
    if (osal_mutex_lock(impl->mutex, timeout_ms) != OSAL_OK) {
        return NX_ERR_TIMEOUT;
    }
#endif

    /* Direct call to STM32 HAL blocking function (zero-copy) */
    HAL_StatusTypeDef hal_status = HAL_UART_Transmit(
        &impl->huart, (uint8_t*)data, (uint16_t)len, timeout_ms);

    /* Map HAL status to Nexus status */
    nx_status_t status = stm32_uart_hal_to_nx_status(hal_status);

    /* Transmission complete */

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Release mutex */
    osal_mutex_unlock(impl->mutex);
#endif

    return status;
}

/*---------------------------------------------------------------------------*/
/* RX Sync Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Synchronous receive implementation
 * \details         Direct delegation to HAL_UART_Receive() (blocking).
 *                  Zero-copy operation - passes data pointer directly to HAL.
 */
static nx_status_t rx_sync_receive(nx_rx_sync_t* self, uint8_t* data,
                                   size_t* len, uint32_t timeout_ms) {
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, rx_sync);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }
    if (!data || !len || *len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Acquire mutex */
    if (osal_mutex_lock(impl->mutex, timeout_ms) != OSAL_OK) {
        return NX_ERR_TIMEOUT;
    }
#endif

    /* Direct call to STM32 HAL blocking function (zero-copy) */
    HAL_StatusTypeDef hal_status =
        HAL_UART_Receive(&impl->huart, data, (uint16_t)*len, timeout_ms);

    /* Map HAL status to Nexus status */
    nx_status_t status = stm32_uart_hal_to_nx_status(hal_status);

    /* Reception complete */
    if (status != NX_OK) {
        *len = 0;
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Release mutex */
    osal_mutex_unlock(impl->mutex);
#endif

    return status;
}

/**
 * \brief           Synchronous receive all implementation
 * \details         Blocks until all requested data is received or timeout.
 *                  Direct delegation to HAL_UART_Receive() with exact length.
 */
static nx_status_t rx_sync_receive_all(nx_rx_sync_t* self, uint8_t* data,
                                       size_t* len, uint32_t timeout_ms) {
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, rx_sync);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }
    if (!data || !len || *len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Acquire mutex */
    if (osal_mutex_lock(impl->mutex, timeout_ms) != OSAL_OK) {
        return NX_ERR_TIMEOUT;
    }
#endif

    size_t requested = *len;

    /* Direct call to STM32 HAL blocking function (zero-copy) */
    /* HAL_UART_Receive blocks until all data received or timeout */
    HAL_StatusTypeDef hal_status =
        HAL_UART_Receive(&impl->huart, data, (uint16_t)requested, timeout_ms);

    /* Map HAL status to Nexus status */
    nx_status_t status = stm32_uart_hal_to_nx_status(hal_status);

    /* Reception complete */
    if (status != NX_OK) {
        *len = 0;
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Release mutex */
    osal_mutex_unlock(impl->mutex);
#endif

    return status;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX sync interface
 */
void uart_init_tx_sync(nx_tx_sync_t* tx_sync) {
    tx_sync->send = tx_sync_send;
}

/**
 * \brief           Initialize RX sync interface
 */
void uart_init_rx_sync(nx_rx_sync_t* rx_sync) {
    rx_sync->receive = rx_sync_receive;
    rx_sync->receive_all = rx_sync_receive_all;
}
