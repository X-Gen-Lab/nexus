/**
 * \file            stm32_uart_lifecycle.c
 * \brief           STM32 UART lifecycle interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-03
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART lifecycle operations including init,
 *                  deinit, suspend, resume, and state query functions.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "stm32_uart_dma.h"
#include "stm32_uart_helpers.h"
#include "stm32_uart_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Implementation                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize UART device
 * \details         Populates UART_InitTypeDef from Nexus configuration,
 *                  calls HAL_UART_Init(), initializes DMA if enabled,
 *                  initializes circular buffers if configured, and creates
 *                  OSAL objects if enabled.
 */
static nx_status_t uart_lifecycle_init(nx_lifecycle_t* self) {
    stm32_uart_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Populate UART_InitTypeDef from Nexus configuration */
    impl->huart.Init.BaudRate = impl->state->config.baudrate;
    impl->huart.Init.WordLength = impl->state->config.word_length;
    impl->huart.Init.StopBits = impl->state->config.stop_bits;
    impl->huart.Init.Parity = impl->state->config.parity;
    impl->huart.Init.Mode = impl->state->config.mode;
    impl->huart.Init.HwFlowCtl = impl->state->config.hw_flow_ctl;

    /* Initialize ST HAL UART */
    HAL_StatusTypeDef hal_status = HAL_UART_Init(&impl->huart);
    if (hal_status != HAL_OK) {
        return stm32_uart_hal_to_nx_status(hal_status);
    }

    /* Initialize DMA if enabled */
    if (impl->state->config.dma_tx_enable ||
        impl->state->config.dma_rx_enable) {
        nx_status_t dma_status = uart_dma_init(impl);
        if (dma_status != NX_OK) {
            HAL_UART_DeInit(&impl->huart);
            return dma_status;
        }
    }

    /* Initialize TX circular buffer if configured */
    if (impl->state->tx_buf.data != NULL && impl->state->tx_buf.size > 0) {
        memset(impl->state->tx_buf.data, 0, impl->state->tx_buf.size);
        impl->state->tx_buf.head = 0;
        impl->state->tx_buf.tail = 0;
        impl->state->tx_buf.count = 0;
        impl->state->tx_buf.peak_usage = 0;
        impl->state->tx_buf.overflow_count = 0;
        impl->state->tx_buf.high_water_flag = false;
        impl->state->tx_buf.low_water_flag = true;
    }

    /* Initialize RX circular buffer if configured */
    if (impl->state->rx_buf.data != NULL && impl->state->rx_buf.size > 0) {
        memset(impl->state->rx_buf.data, 0, impl->state->rx_buf.size);
        impl->state->rx_buf.head = 0;
        impl->state->rx_buf.tail = 0;
        impl->state->rx_buf.count = 0;
        impl->state->rx_buf.peak_usage = 0;
        impl->state->rx_buf.overflow_count = 0;
        impl->state->rx_buf.high_water_flag = false;
        impl->state->rx_buf.low_water_flag = true;
    }

    /* Set state flags */
    impl->state->initialized = true;
    impl->state->suspended = false;
    impl->state->tx_busy = false;
    impl->state->rx_busy = false;

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Create OSAL mutex for thread safety */
    osal_status_t osal_status = osal_mutex_create(&impl->mutex);
    if (osal_status != OSAL_OK) {
        /* Cleanup on failure */
        if (impl->state->config.dma_tx_enable ||
            impl->state->config.dma_rx_enable) {
            uart_dma_deinit(impl);
        }
        HAL_UART_DeInit(&impl->huart);
        impl->state->initialized = false;
        return NX_ERR_NO_MEMORY;
    }

    /* Create TX semaphore for sync operations (binary semaphore, initial=0) */
    osal_status = osal_sem_create_binary(0, &impl->tx_sem);
    if (osal_status != OSAL_OK) {
        osal_mutex_delete(impl->mutex);
        if (impl->state->config.dma_tx_enable ||
            impl->state->config.dma_rx_enable) {
            uart_dma_deinit(impl);
        }
        HAL_UART_DeInit(&impl->huart);
        impl->state->initialized = false;
        return NX_ERR_NO_MEMORY;
    }

    /* Create RX semaphore for sync operations (binary semaphore, initial=0) */
    osal_status = osal_sem_create_binary(0, &impl->rx_sem);
    if (osal_status != OSAL_OK) {
        osal_sem_delete(impl->tx_sem);
        osal_mutex_delete(impl->mutex);
        if (impl->state->config.dma_tx_enable ||
            impl->state->config.dma_rx_enable) {
            uart_dma_deinit(impl);
        }
        HAL_UART_DeInit(&impl->huart);
        impl->state->initialized = false;
        return NX_ERR_NO_MEMORY;
    }
#endif

    return NX_OK;
}

/**
 * \brief           Deinitialize UART device
 * \details         Calls HAL_UART_DeInit(), deinitializes DMA if enabled,
 *                  frees circular buffers, and destroys OSAL objects if
 * enabled.
 */
static nx_status_t uart_lifecycle_deinit(nx_lifecycle_t* self) {
    stm32_uart_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Delete OSAL objects if enabled */
    if (impl->rx_sem != NULL) {
        osal_sem_delete(impl->rx_sem);
        impl->rx_sem = NULL;
    }
    if (impl->tx_sem != NULL) {
        osal_sem_delete(impl->tx_sem);
        impl->tx_sem = NULL;
    }
    if (impl->mutex != NULL) {
        osal_mutex_delete(impl->mutex);
        impl->mutex = NULL;
    }
#endif

    /* Deinitialize DMA if enabled */
    if (impl->state->config.dma_tx_enable ||
        impl->state->config.dma_rx_enable) {
        uart_dma_deinit(impl);
    }

    /* Deinitialize ST HAL UART */
    HAL_UART_DeInit(&impl->huart);

    /* Clear TX buffer if allocated */
    if (impl->state->tx_buf.data != NULL) {
        memset(impl->state->tx_buf.data, 0, impl->state->tx_buf.size);
        impl->state->tx_buf.head = 0;
        impl->state->tx_buf.tail = 0;
        impl->state->tx_buf.count = 0;
        impl->state->tx_buf.peak_usage = 0;
        impl->state->tx_buf.overflow_count = 0;
        impl->state->tx_buf.high_water_flag = false;
        impl->state->tx_buf.low_water_flag = true;
    }

    /* Clear RX buffer if allocated */
    if (impl->state->rx_buf.data != NULL) {
        memset(impl->state->rx_buf.data, 0, impl->state->rx_buf.size);
        impl->state->rx_buf.head = 0;
        impl->state->rx_buf.tail = 0;
        impl->state->rx_buf.count = 0;
        impl->state->rx_buf.peak_usage = 0;
        impl->state->rx_buf.overflow_count = 0;
        impl->state->rx_buf.high_water_flag = false;
        impl->state->rx_buf.low_water_flag = true;
    }

    /* Clear state flags */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->tx_busy = false;
    impl->state->rx_busy = false;

    return NX_OK;
}

/**
 * \brief           Suspend UART device
 * \details         Disables UART peripheral and saves current state.
 *                  Requirement 8.1: Disable UART peripheral and save state.
 *                  Requirement 8.3: Support LPUART low-power mode.
 */
static nx_status_t uart_lifecycle_suspend(nx_lifecycle_t* self) {
    stm32_uart_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if already suspended */
    if (impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check if busy - cannot suspend during active transfer */
    if (impl->state->tx_busy || impl->state->rx_busy) {
        return NX_ERR_BUSY;
    }

    /* Detect peripheral type for LPUART-specific handling */
    uart_periph_type_t periph_type = uart_get_periph_type(impl->huart.Instance);

    /* Disable UART peripheral */
    __HAL_UART_DISABLE(&impl->huart);

    /* For LPUART, additional low-power mode configuration */
    if (periph_type == UART_PERIPH_LPUART) {
        /* LPUART can remain operational in low-power modes */
        /* The peripheral is already disabled above, which is sufficient */
        /* When resumed, LPUART will automatically support low-power operation
         */
    }

    /* Set suspend flag */
    impl->state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume UART device
 * \details         Restores UART peripheral and configuration.
 *                  Requirement 8.2: Restore UART peripheral and configuration.
 */
static nx_status_t uart_lifecycle_resume(nx_lifecycle_t* self) {
    stm32_uart_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if not suspended */
    if (!impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Enable UART peripheral */
    __HAL_UART_ENABLE(&impl->huart);

    /* Clear suspend flag */
    impl->state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get device state
 */
static nx_device_state_t uart_lifecycle_get_state(nx_lifecycle_t* self) {
    stm32_uart_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state) {
        return NX_DEV_STATE_ERROR;
    }
    if (!impl->state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }
    if (impl->state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }

    return NX_DEV_STATE_RUNNING;
}

/**
 * \brief           Reconfigure UART at runtime
 * \details         Calls HAL_UART_DeInit() then HAL_UART_Init() with new
 *                  settings. Preserves state where possible (buffers, stats).
 */
NX_UNUSED static nx_status_t
uart_lifecycle_reconfigure(stm32_uart_impl_t* impl,
                           const stm32_uart_config_t* new_config) {
    /* Parameter validation */
    if (!impl || !impl->state || !new_config) {
        return NX_ERR_INVALID_PARAM;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if busy */
    if (impl->state->tx_busy || impl->state->rx_busy) {
        return NX_ERR_BUSY;
    }

    /* Save current configuration */
    stm32_uart_config_t old_config = impl->state->config;

    /* Update configuration */
    impl->state->config = *new_config;

    /* Update UART_InitTypeDef */
    impl->huart.Init.BaudRate = new_config->baudrate;
    impl->huart.Init.WordLength = new_config->word_length;
    impl->huart.Init.StopBits = new_config->stop_bits;
    impl->huart.Init.Parity = new_config->parity;
    impl->huart.Init.Mode = new_config->mode;
    impl->huart.Init.HwFlowCtl = new_config->hw_flow_ctl;

    /* Deinitialize DMA if enabled */
    if (old_config.dma_tx_enable || old_config.dma_rx_enable) {
        uart_dma_deinit(impl);
    }

    /* Deinitialize ST HAL UART */
    HAL_UART_DeInit(&impl->huart);

    /* Reinitialize ST HAL UART with new settings */
    HAL_StatusTypeDef hal_status = HAL_UART_Init(&impl->huart);
    if (hal_status != HAL_OK) {
        /* Restore old configuration on failure */
        impl->state->config = old_config;
        impl->huart.Init.BaudRate = old_config.baudrate;
        impl->huart.Init.WordLength = old_config.word_length;
        impl->huart.Init.StopBits = old_config.stop_bits;
        impl->huart.Init.Parity = old_config.parity;
        impl->huart.Init.Mode = old_config.mode;
        impl->huart.Init.HwFlowCtl = old_config.hw_flow_ctl;
        HAL_UART_Init(&impl->huart);

        /* Restore DMA if it was enabled */
        if (old_config.dma_tx_enable || old_config.dma_rx_enable) {
            uart_dma_init(impl);
        }

        return stm32_uart_hal_to_nx_status(hal_status);
    }

    /* Reinitialize DMA if enabled in new configuration */
    if (new_config->dma_tx_enable || new_config->dma_rx_enable) {
        nx_status_t dma_status = uart_dma_init(impl);
        if (dma_status != NX_OK) {
            /* Restore old configuration on DMA init failure */
            impl->state->config = old_config;
            HAL_UART_DeInit(&impl->huart);
            impl->huart.Init.BaudRate = old_config.baudrate;
            impl->huart.Init.WordLength = old_config.word_length;
            impl->huart.Init.StopBits = old_config.stop_bits;
            impl->huart.Init.Parity = old_config.parity;
            impl->huart.Init.Mode = old_config.mode;
            impl->huart.Init.HwFlowCtl = old_config.hw_flow_ctl;
            HAL_UART_Init(&impl->huart);

            /* Restore DMA if it was enabled */
            if (old_config.dma_tx_enable || old_config.dma_rx_enable) {
                uart_dma_init(impl);
            }

            return dma_status;
        }
    }

    /* Preserve buffers and statistics - they remain unchanged */
    /* Note: Buffer sizes cannot be changed at runtime */

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface
 */
void uart_init_lifecycle(nx_lifecycle_t* lifecycle) {
    lifecycle->init = uart_lifecycle_init;
    lifecycle->deinit = uart_lifecycle_deinit;
    lifecycle->suspend = uart_lifecycle_suspend;
    lifecycle->resume = uart_lifecycle_resume;
    lifecycle->get_state = uart_lifecycle_get_state;
}
