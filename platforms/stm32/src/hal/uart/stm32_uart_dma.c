/**
 * \file            stm32_uart_dma.c
 * \brief           STM32 UART DMA support implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-04
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements DMA initialization, transfer operations, and
 *                  zero-copy DMA support for UART. Delegates to STM32 HAL
 *                  DMA functions.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "stm32_uart_dma.h"
#include "nexus_config.h"
#include "stm32_uart_helpers.h"
#include "stm32_uart_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* DMA Initialization                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DMA for UART
 */
nx_status_t uart_dma_init(stm32_uart_impl_t* impl) {
    if (!impl) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Initialize DMA structure */
    memset(&impl->dma, 0, sizeof(stm32_uart_dma_t));

#ifdef NX_CONFIG_STM32_UART_USE_DMA
    /* Check if DMA is enabled in configuration */
    impl->dma.dma_tx_enabled = impl->state->config.dma_tx_enable;
    impl->dma.dma_rx_enabled = impl->state->config.dma_rx_enable;

    if (!impl->dma.dma_tx_enabled && !impl->dma.dma_rx_enabled) {
        /* DMA not enabled, nothing to do */
        return NX_OK;
    }

    /* Initialize TX DMA if enabled */
    if (impl->dma.dma_tx_enabled) {
#ifdef NX_CONFIG_STM32_UART_DMA_TX_CHANNEL
        impl->dma.hdma_tx.Instance = NX_CONFIG_STM32_UART_DMA_TX_CHANNEL;
        impl->dma.hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        impl->dma.hdma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        impl->dma.hdma_tx.Init.MemInc = DMA_MINC_ENABLE;
        impl->dma.hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        impl->dma.hdma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        impl->dma.hdma_tx.Init.Mode = DMA_NORMAL;
        impl->dma.hdma_tx.Init.Priority = NX_CONFIG_STM32_UART_DMA_TX_PRIORITY;

        /* Link DMA handle to UART handle */
        __HAL_LINKDMA(&impl->huart, hdmatx, impl->dma.hdma_tx);

        /* Initialize DMA */
        if (HAL_DMA_Init(&impl->dma.hdma_tx) != HAL_OK) {
            return NX_ERR_IO;
        }
#else
        /* DMA TX channel not configured */
        impl->dma.dma_tx_enabled = false;
#endif
    }

    /* Initialize RX DMA if enabled */
    if (impl->dma.dma_rx_enabled) {
#ifdef NX_CONFIG_STM32_UART_DMA_RX_CHANNEL
        impl->dma.hdma_rx.Instance = NX_CONFIG_STM32_UART_DMA_RX_CHANNEL;
        impl->dma.hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        impl->dma.hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        impl->dma.hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
        impl->dma.hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        impl->dma.hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        impl->dma.hdma_rx.Init.Mode = DMA_NORMAL;
        impl->dma.hdma_rx.Init.Priority = NX_CONFIG_STM32_UART_DMA_RX_PRIORITY;

        /* Link DMA handle to UART handle */
        __HAL_LINKDMA(&impl->huart, hdmarx, impl->dma.hdma_rx);

        /* Initialize DMA */
        if (HAL_DMA_Init(&impl->dma.hdma_rx) != HAL_OK) {
            return NX_ERR_IO;
        }
#else
        /* DMA RX channel not configured */
        impl->dma.dma_rx_enabled = false;
#endif
    }

#endif /* NX_CONFIG_STM32_UART_USE_DMA */

    return NX_OK;
}

/**
 * \brief           Deinitialize DMA for UART
 */
nx_status_t uart_dma_deinit(stm32_uart_impl_t* impl) {
    if (!impl) {
        return NX_ERR_INVALID_PARAM;
    }

#ifdef NX_CONFIG_STM32_UART_USE_DMA
    /* Deinitialize TX DMA */
    if (impl->dma.dma_tx_enabled) {
        HAL_DMA_DeInit(&impl->dma.hdma_tx);
        impl->dma.dma_tx_enabled = false;
    }

    /* Deinitialize RX DMA */
    if (impl->dma.dma_rx_enabled) {
        HAL_DMA_DeInit(&impl->dma.hdma_rx);
        impl->dma.dma_rx_enabled = false;
    }
#endif

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* DMA Transfer Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Transmit data using DMA
 * \details         Requirement 8.5: Reject operations when suspended.
 */
nx_status_t uart_dma_transmit(stm32_uart_impl_t* impl, const uint8_t* data,
                              size_t len) {
    if (!impl || !data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if suspended */
    if (impl->state && impl->state->suspended) {
        return NX_ERR_NOT_READY;
    }

#ifdef NX_CONFIG_STM32_UART_USE_DMA
    /* Check if DMA TX is enabled */
    if (!impl->dma.dma_tx_enabled) {
        return NX_ERR_NOT_SUPPORTED;
    }

    /* Check if UART is busy */
    if (impl->state->tx_busy) {
        return NX_ERR_BUSY;
    }

    /* Mark TX as busy */
    impl->state->tx_busy = true;

    /* Start DMA transmission */
    HAL_StatusTypeDef hal_status =
        HAL_UART_Transmit_DMA(&impl->huart, (uint8_t*)data, len);

    if (hal_status != HAL_OK) {
        impl->state->tx_busy = false;
        return stm32_uart_hal_to_nx_status(hal_status);
    }

    return NX_OK;
#else
    (void)impl;
    (void)data;
    (void)len;
    return NX_ERR_NOT_SUPPORTED;
#endif
}
/**
 * \brief           Receive data using DMA
 * \details         Requirement 8.5: Reject operations when suspended.
 */
nx_status_t uart_dma_receive(stm32_uart_impl_t* impl, uint8_t* data,
                             size_t len) {
    if (!impl || !data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if suspended */
    if (impl->state && impl->state->suspended) {
        return NX_ERR_NOT_READY;
    }

#ifdef NX_CONFIG_STM32_UART_USE_DMA
    /* Check if DMA RX is enabled */
    if (!impl->dma.dma_rx_enabled) {
        return NX_ERR_NOT_SUPPORTED;
    }

    /* Check if UART is busy */
    if (impl->state->rx_busy) {
        return NX_ERR_BUSY;
    }

    /* Mark RX as busy */
    impl->state->rx_busy = true;

    /* Start DMA reception */
    HAL_StatusTypeDef hal_status =
        HAL_UART_Receive_DMA(&impl->huart, data, len);

    if (hal_status != HAL_OK) {
        impl->state->rx_busy = false;
        return stm32_uart_hal_to_nx_status(hal_status);
    }

    return NX_OK;
#else
    (void)impl;
    (void)data;
    (void)len;
    return NX_ERR_NOT_SUPPORTED;
#endif
}

/*---------------------------------------------------------------------------*/
/* DMA Callback Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           DMA TX complete callback
 * \note            Called by HAL when DMA transmission completes
 */
void HAL_UART_TxCpltCallback_DMA(UART_HandleTypeDef* huart) {
    if (!huart) {
        return;
    }

    /* Find the UART implementation from HAL handle */
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(huart, stm32_uart_impl_t, huart);

    /* Clear TX busy flag */
    impl->state->tx_busy = false;

    /* Call user callback if registered */
    if (impl->callbacks.tx_complete_cb) {
        impl->callbacks.tx_complete_cb(impl->callbacks.user_data);
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Signal TX completion semaphore */
    if (impl->tx_sem) {
        osal_sem_give(impl->tx_sem);
    }
#endif
}

/**
 * \brief           DMA RX complete callback
 * \note            Called by HAL when DMA reception completes
 */
void HAL_UART_RxCpltCallback_DMA(UART_HandleTypeDef* huart) {
    if (!huart) {
        return;
    }

    /* Find the UART implementation from HAL handle */
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(huart, stm32_uart_impl_t, huart);

    /* Clear RX busy flag */
    impl->state->rx_busy = false;

    /* Write received data to circular buffer if configured */
    if (impl->state->rx_buf.data != NULL && impl->state->rx_buf.size > 0) {
        stm32_uart_buffer_write(&impl->state->rx_buf, huart->pRxBuffPtr,
                                huart->RxXferSize);
    }

    /* Call user callback if registered */
    if (impl->callbacks.rx_complete_cb) {
        impl->callbacks.rx_complete_cb(impl->callbacks.user_data);
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Signal RX completion semaphore */
    if (impl->rx_sem) {
        osal_sem_give(impl->rx_sem);
    }
#endif
}

/**
 * \brief           DMA error callback
 * \note            Called by HAL when DMA error occurs
 */
void HAL_UART_ErrorCallback_DMA(UART_HandleTypeDef* huart) {
    if (!huart) {
        return;
    }

    /* Find the UART implementation from HAL handle */
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(huart, stm32_uart_impl_t, huart);

    /* Get error code */
    uint32_t error_code = huart->ErrorCode;

    /* Clear busy flags */
    impl->state->tx_busy = false;
    impl->state->rx_busy = false;

    /* Error handling */
    if (error_code & HAL_UART_ERROR_DMA) {
        /* DMA error - increment DMA error counter */
        /* Note: dma_errors field will be added in task 5.1 */
    }

    /* Call user error callback if registered */
    if (impl->callbacks.error_cb) {
        impl->callbacks.error_cb(impl->callbacks.user_data, error_code);
    }
}

/*---------------------------------------------------------------------------*/
/* Zero-Copy DMA Functions                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Start zero-copy DMA transmission from circular buffer
 * \details         Transmits data directly from circular buffer without copy.
 *                  Requirement 8.5: Reject operations when suspended.
 */
nx_status_t uart_dma_transmit_zerocopy(stm32_uart_impl_t* impl) {
    if (!impl) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if suspended */
    if (impl->state && impl->state->suspended) {
        return NX_ERR_NOT_READY;
    }

#ifdef NX_CONFIG_STM32_UART_USE_DMA
    /* Check if DMA TX is enabled */
    if (!impl->dma.dma_tx_enabled) {
        return NX_ERR_NOT_SUPPORTED;
    }

    /* Check if TX buffer is configured */
    if (!impl->state->tx_buf.data || impl->state->tx_buf.size == 0) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check if UART is busy */
    if (impl->state->tx_busy) {
        return NX_ERR_BUSY;
    }

    /* Check if there's data to transmit */
    size_t available = stm32_uart_buffer_get_count(&impl->state->tx_buf);
    if (available == 0) {
        return NX_OK; /* Nothing to transmit */
    }

    /* Calculate contiguous data length from tail */
    size_t tail = impl->state->tx_buf.tail;
    size_t chunk_size = impl->state->tx_buf.size - tail;
    if (chunk_size > available) {
        chunk_size = available;
    }

    /* Mark TX as busy */
    impl->state->tx_busy = true;

    /* Start DMA transmission directly from buffer */
    HAL_StatusTypeDef hal_status = HAL_UART_Transmit_DMA(
        &impl->huart, &impl->state->tx_buf.data[tail], chunk_size);

    if (hal_status != HAL_OK) {
        impl->state->tx_busy = false;
        return stm32_uart_hal_to_nx_status(hal_status);
    }

    /* Update buffer tail (will be finalized in callback) */
    impl->state->tx_buf.tail = (tail + chunk_size) % impl->state->tx_buf.size;
    impl->state->tx_buf.count -= chunk_size;

    return NX_OK;
#else
    (void)impl;
    return NX_ERR_NOT_SUPPORTED;
#endif
}

/**
 * \brief           Start zero-copy DMA reception to circular buffer
 * \details         Receives data directly to circular buffer without copy.
 *                  Requirement 8.5: Reject operations when suspended.
 */
nx_status_t uart_dma_receive_zerocopy(stm32_uart_impl_t* impl, size_t len) {
    if (!impl || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if suspended */
    if (impl->state && impl->state->suspended) {
        return NX_ERR_NOT_READY;
    }

#ifdef NX_CONFIG_STM32_UART_USE_DMA
    /* Check if DMA RX is enabled */
    if (!impl->dma.dma_rx_enabled) {
        return NX_ERR_NOT_SUPPORTED;
    }

    /* Check if RX buffer is configured */
    if (!impl->state->rx_buf.data || impl->state->rx_buf.size == 0) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check if UART is busy */
    if (impl->state->rx_busy) {
        return NX_ERR_BUSY;
    }

    /* Check if there's space in buffer */
    size_t free_space = stm32_uart_buffer_get_free(&impl->state->rx_buf);
    if (free_space == 0) {
        return NX_ERR_NO_MEMORY;
    }

    /* Limit reception to available space */
    if (len > free_space) {
        len = free_space;
    }

    /* Calculate contiguous space from head */
    size_t head = impl->state->rx_buf.head;
    size_t chunk_size = impl->state->rx_buf.size - head;
    if (chunk_size > len) {
        chunk_size = len;
    }

    /* Mark RX as busy */
    impl->state->rx_busy = true;

    /* Start DMA reception directly to buffer */
    HAL_StatusTypeDef hal_status = HAL_UART_Receive_DMA(
        &impl->huart, &impl->state->rx_buf.data[head], chunk_size);

    if (hal_status != HAL_OK) {
        impl->state->rx_busy = false;
        return stm32_uart_hal_to_nx_status(hal_status);
    }

    /* Update buffer head (will be finalized in callback) */
    impl->state->rx_buf.head = (head + chunk_size) % impl->state->rx_buf.size;
    impl->state->rx_buf.count += chunk_size;

    /* Update peak usage */
    if (impl->state->rx_buf.count > impl->state->rx_buf.peak_usage) {
        impl->state->rx_buf.peak_usage = impl->state->rx_buf.count;
    }

    return NX_OK;
#else
    (void)impl;
    (void)len;
    return NX_ERR_NOT_SUPPORTED;
#endif
}
