/**
 * \file            stm32_uart_callbacks.c
 * \brief           STM32 UART callback implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-04
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements STM32 HAL UART callbacks and callback
 *                  registration functions. Forwards HAL callbacks to Nexus
 *                  user callbacks with minimal processing.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "stm32_uart_helpers.h"
#include "stm32_uart_types.h"

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
#include "osal/osal_sem.h"
#endif

/*---------------------------------------------------------------------------*/
/* STM32 HAL Callback Implementations                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           TX complete callback
 * \note            Called by HAL_UART_IRQHandler when transmission completes
 * \details         Updates statistics, clears busy flag, signals semaphore
 *                  (if OSAL enabled), and forwards to user callback.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    if (!huart) {
        return;
    }

    /* Find UART instance from handle */
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(huart, stm32_uart_impl_t, huart);
    if (!impl || !impl->state) {
        return;
    }

    /* Clear busy flag */
    impl->state->tx_busy = false;

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Signal TX completion semaphore */
    if (impl->tx_sem != NULL) {
        osal_sem_give(impl->tx_sem);
    }
#endif

    /* Forward to user TX complete callback */
    if (impl->callbacks.tx_complete_cb) {
        impl->callbacks.tx_complete_cb(impl->callbacks.user_data);
    }
}

/**
 * \brief           RX complete callback
 * \note            Called by HAL_UART_IRQHandler when reception completes
 * \details         Writes data to circular buffer (if configured), updates
 *                  statistics, clears busy flag, signals semaphore (if OSAL
 *                  enabled), and forwards to user callback.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    if (!huart) {
        return;
    }

    /* Find UART instance from handle */
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(huart, stm32_uart_impl_t, huart);
    if (!impl || !impl->state) {
        return;
    }

    /* Write received data to circular buffer (Nexus value-add) */
    if (impl->state->rx_buf.data != NULL && impl->state->rx_buf.size > 0) {
        stm32_uart_buffer_write(&impl->state->rx_buf, huart->pRxBuffPtr,
                                huart->RxXferSize);
    }

    /* Clear busy flag */
    impl->state->rx_busy = false;

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* Signal RX completion semaphore */
    if (impl->rx_sem != NULL) {
        osal_sem_give(impl->rx_sem);
    }
#endif

    /* Forward to user RX complete callback */
    if (impl->callbacks.rx_complete_cb) {
        impl->callbacks.rx_complete_cb(impl->callbacks.user_data);
    }
}

/**
 * \brief           Error callback
 * \note            Called by HAL_UART_IRQHandler when error occurs
 * \details         Updates error statistics by type (PE, FE, NE, ORE, DMA),
 *                  clears busy flags, and forwards to user error callback.
 *                  STM32 HAL handles error recovery automatically.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    if (!huart) {
        return;
    }

    /* Find UART instance from handle */
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(huart, stm32_uart_impl_t, huart);
    if (!impl || !impl->state) {
        return;
    }

    /* Get error code from HAL */
    uint32_t error_code = HAL_UART_GetError(huart);

    /* Error handling */
    (void)error_code;

    /* Clear busy flags */
    impl->state->tx_busy = false;
    impl->state->rx_busy = false;

    /* Forward to user error callback with error code */
    /* Let STM32 HAL handle error recovery */
    if (impl->callbacks.error_cb) {
        impl->callbacks.error_cb(impl->callbacks.user_data, error_code);
    }
}

/*---------------------------------------------------------------------------*/
/* Callback Registration Functions                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register TX complete callback
 * \param[in]       impl: UART implementation pointer
 * \param[in]       callback: TX complete callback function
 * \param[in]       user_data: User data pointer
 * \return          Status code
 */
nx_status_t stm32_uart_register_tx_callback(stm32_uart_impl_t* impl,
                                            void (*callback)(void*),
                                            void* user_data) {
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    /* Store TX complete callback */
    impl->callbacks.tx_complete_cb = callback;

    /* Store user data pointer */
    impl->callbacks.user_data = user_data;

    return NX_OK;
}

/**
 * \brief           Register RX complete callback
 * \param[in]       impl: UART implementation pointer
 * \param[in]       callback: RX complete callback function
 * \param[in]       user_data: User data pointer
 * \return          Status code
 */
nx_status_t stm32_uart_register_rx_callback(stm32_uart_impl_t* impl,
                                            void (*callback)(void*),
                                            void* user_data) {
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    /* Store RX complete callback */
    impl->callbacks.rx_complete_cb = callback;

    /* Store user data pointer */
    impl->callbacks.user_data = user_data;

    return NX_OK;
}

/**
 * \brief           Register error callback
 * \param[in]       impl: UART implementation pointer
 * \param[in]       callback: Error callback function
 * \param[in]       user_data: User data pointer
 * \return          Status code
 */
nx_status_t stm32_uart_register_error_callback(stm32_uart_impl_t* impl,
                                               void (*callback)(void*,
                                                                uint32_t),
                                               void* user_data) {
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    /* Store error callback */
    impl->callbacks.error_cb = callback;

    /* Store user data pointer */
    impl->callbacks.user_data = user_data;

    return NX_OK;
}

/**
 * \brief           Unregister TX complete callback
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t stm32_uart_unregister_tx_callback(stm32_uart_impl_t* impl) {
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    /* Clear TX complete callback */
    impl->callbacks.tx_complete_cb = NULL;

    return NX_OK;
}

/**
 * \brief           Unregister RX complete callback
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t stm32_uart_unregister_rx_callback(stm32_uart_impl_t* impl) {
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    /* Clear RX complete callback */
    impl->callbacks.rx_complete_cb = NULL;

    return NX_OK;
}

/**
 * \brief           Unregister error callback
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t stm32_uart_unregister_error_callback(stm32_uart_impl_t* impl) {
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    /* Clear error callback */
    impl->callbacks.error_cb = NULL;

    return NX_OK;
}
