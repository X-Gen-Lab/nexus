/** Strong vendor callbacks. No user callback, wait or OSAL call in IRQ. */
#include "stm32_uart_runtime.h"

/* Only handles connected by this controller may be converted to impl. */
extern stm32_uart_impl_t* stm32_uart_from_handle(UART_HandleTypeDef* handle);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* handle) {
    stm32_uart_impl_t* impl = stm32_uart_from_handle(handle);
    if (impl) stm32_uart_tx_completed(impl);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* handle) {
    stm32_uart_impl_t* impl = stm32_uart_from_handle(handle);
    if (!impl || !impl->state->initialized || impl->state->suspended || impl->faulted || impl->closing) return;
    stm32_uart_rx_event(impl, true, impl->rx_byte, NX_OK, 0);
    nx_status_t status = stm32_uart_arm_rx(impl);
    if (status != NX_OK) stm32_uart_rx_event(impl, false, 0, status, 0);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef* handle) {
    stm32_uart_impl_t* impl = stm32_uart_from_handle(handle);
    if (!impl || !impl->state->initialized || impl->faulted || impl->closing) return;
    uint32_t error = HAL_UART_GetError(handle);
    nx_status_t status = NX_ERR_IO;
    if (error & HAL_UART_ERROR_ORE) status = NX_ERR_OVERRUN;
    else if (error & HAL_UART_ERROR_PE) status = NX_ERR_PARITY;
    else if (error & HAL_UART_ERROR_FE) status = NX_ERR_FRAMING;
    else if (error & HAL_UART_ERROR_NE) status = NX_ERR_NOISE;
    stm32_uart_rx_event(impl, false, 0, status, error);
    /* F4 HAL terminates reception on overrun; restart only once READY.
     * Parity/framing/noise are nonblocking and HAL keeps its current RX. */
    if (handle->RxState == HAL_UART_STATE_READY && !impl->state->suspended) {
        (void)stm32_uart_arm_rx(impl);
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
