/**
 * \file            stm32_uart_callbacks.h
 * \brief           STM32 UART callback registration header
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_UART_CALLBACKS_H
#define STM32_UART_CALLBACKS_H

#include "hal/nx_status.h"
#include "stm32_uart_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
                                            void* user_data);

/**
 * \brief           Register RX complete callback
 * \param[in]       impl: UART implementation pointer
 * \param[in]       callback: RX complete callback function
 * \param[in]       user_data: User data pointer
 * \return          Status code
 */
nx_status_t stm32_uart_register_rx_callback(stm32_uart_impl_t* impl,
                                            void (*callback)(void*),
                                            void* user_data);

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
                                               void* user_data);

/**
 * \brief           Unregister TX complete callback
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t stm32_uart_unregister_tx_callback(stm32_uart_impl_t* impl);

/**
 * \brief           Unregister RX complete callback
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t stm32_uart_unregister_rx_callback(stm32_uart_impl_t* impl);

/**
 * \brief           Unregister error callback
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t stm32_uart_unregister_error_callback(stm32_uart_impl_t* impl);

#ifdef __cplusplus
}
#endif

#endif /* STM32_UART_CALLBACKS_H */
