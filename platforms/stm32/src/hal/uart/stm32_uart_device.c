/**
 * \file            stm32_uart_device.c
 * \brief           STM32 UART device registration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-03
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART device registration using Kconfig-driven
 *                  configuration with dynamic memory allocation.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_uart.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "stm32_uart_helpers.h"
#include "stm32_uart_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE STM32_UART

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface getters */
static nx_tx_async_t* uart_get_tx_async(nx_uart_t* self);
static nx_rx_async_t* uart_get_rx_async(nx_uart_t* self);
static nx_tx_sync_t* uart_get_tx_sync(nx_uart_t* self);
static nx_rx_sync_t* uart_get_rx_sync(nx_uart_t* self);
static nx_lifecycle_t* uart_get_lifecycle(nx_uart_t* self);
static nx_power_t* uart_get_power(nx_uart_t* self);

/* Interface implementations (defined in separate files) */
extern void uart_init_tx_async(nx_tx_async_t* tx_async);
extern void uart_init_rx_async(nx_rx_async_t* rx_async);
extern void uart_init_tx_sync(nx_tx_sync_t* tx_sync);
extern void uart_init_rx_sync(nx_rx_sync_t* rx_sync);
extern void uart_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void uart_init_power(nx_power_t* power);

/* ISR management */
extern nx_status_t uart_register_isr(stm32_uart_impl_t* impl);
extern nx_status_t uart_unregister_isr(stm32_uart_impl_t* impl);

/*---------------------------------------------------------------------------*/
/* Base Interface Getters                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get TX async interface
 */
static nx_tx_async_t* uart_get_tx_async(nx_uart_t* self) {
    stm32_uart_impl_t* impl =
        self ? NX_CONTAINER_OF(self, stm32_uart_impl_t, base) : NULL;
    return impl ? &impl->tx_async : NULL;
}

/**
 * \brief           Get RX async interface
 */
static nx_rx_async_t* uart_get_rx_async(nx_uart_t* self) {
    stm32_uart_impl_t* impl =
        self ? NX_CONTAINER_OF(self, stm32_uart_impl_t, base) : NULL;
    return impl ? &impl->rx_async : NULL;
}

/**
 * \brief           Get TX sync interface
 */
static nx_tx_sync_t* uart_get_tx_sync(nx_uart_t* self) {
    stm32_uart_impl_t* impl =
        self ? NX_CONTAINER_OF(self, stm32_uart_impl_t, base) : NULL;
    return impl ? &impl->tx_sync : NULL;
}

/**
 * \brief           Get RX sync interface
 */
static nx_rx_sync_t* uart_get_rx_sync(nx_uart_t* self) {
    stm32_uart_impl_t* impl =
        self ? NX_CONTAINER_OF(self, stm32_uart_impl_t, base) : NULL;
    return impl ? &impl->rx_sync : NULL;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* uart_get_lifecycle(nx_uart_t* self) {
    stm32_uart_impl_t* impl =
        self ? NX_CONTAINER_OF(self, stm32_uart_impl_t, base) : NULL;
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* uart_get_power(nx_uart_t* self) {
    stm32_uart_impl_t* impl =
        self ? NX_CONTAINER_OF(self, stm32_uart_impl_t, base) : NULL;
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize UART instance with platform configuration
 */
static void
uart_init_instance(stm32_uart_impl_t* impl, uint8_t index,
                   const stm32_uart_platform_config_t* platform_cfg) {
    /* Initialize base interface using NX_INIT_UART macro */
    NX_INIT_UART(&impl->base, uart_get_tx_async, uart_get_rx_async,
                 uart_get_tx_sync, uart_get_rx_sync, uart_get_lifecycle,
                 uart_get_power);

    /* Initialize interfaces (implemented in separate files) */
    uart_init_tx_async(&impl->tx_async);
    uart_init_rx_async(&impl->rx_async);
    uart_init_tx_sync(&impl->tx_sync);
    uart_init_rx_sync(&impl->rx_sync);
    uart_init_lifecycle(&impl->lifecycle);
    uart_init_power(&impl->power);

    /* Allocate and initialize state */
    impl->state = (stm32_uart_state_t*)nx_mem_alloc(sizeof(stm32_uart_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(stm32_uart_state_t));

    impl->state->instance = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->tx_busy = false;
    impl->state->rx_busy = false;

    /* Configure ST HAL UART handle */
    impl->huart.Instance = platform_cfg->usart_base;
    impl->huart.Init.BaudRate = platform_cfg->baudrate;
    impl->huart.Init.WordLength = platform_cfg->word_length;
    impl->huart.Init.StopBits = platform_cfg->stop_bits;
    impl->huart.Init.Parity = platform_cfg->parity;
    impl->huart.Init.Mode = platform_cfg->mode;
    impl->huart.Init.HwFlowCtl = platform_cfg->hw_flow_ctl;
    impl->huart.Init.OverSampling = platform_cfg->over_sampling;

    /* Set configuration from Kconfig */
    impl->state->config.baudrate = platform_cfg->baudrate;
    impl->state->config.word_length = platform_cfg->word_length;
    impl->state->config.stop_bits = platform_cfg->stop_bits;
    impl->state->config.parity = platform_cfg->parity;
    impl->state->config.mode = platform_cfg->mode;
    impl->state->config.hw_flow_ctl = platform_cfg->hw_flow_ctl;
    impl->state->config.dma_tx_enable = platform_cfg->use_dma;
    impl->state->config.dma_rx_enable = platform_cfg->use_dma;
    impl->state->config.tx_buf_size = platform_cfg->tx_buf_size;
    impl->state->config.rx_buf_size = platform_cfg->rx_buf_size;

    /* Get overflow policy from Kconfig */
    uart_overflow_policy_t overflow_policy =
        (uart_overflow_policy_t)NX_CONFIG_STM32_UART_BUFFER_OVERFLOW_POLICY;

    /* Allocate and initialize TX buffer if size > 0 */
    if (impl->state->config.tx_buf_size > 0) {
        uint8_t* tx_data =
            (uint8_t*)nx_mem_alloc(impl->state->config.tx_buf_size);
        if (!tx_data) {
            nx_mem_free(impl->state);
            impl->state = NULL;
            return;
        }
        stm32_uart_buffer_init(&impl->state->tx_buf, tx_data,
                               impl->state->config.tx_buf_size,
                               overflow_policy);
    } else {
        /* Buffer disabled - set to NULL */
        stm32_uart_buffer_init(&impl->state->tx_buf, NULL, 0, overflow_policy);
    }

    /* Allocate and initialize RX buffer if size > 0 */
    if (impl->state->config.rx_buf_size > 0) {
        uint8_t* rx_data =
            (uint8_t*)nx_mem_alloc(impl->state->config.rx_buf_size);
        if (!rx_data) {
            /* Cleanup TX buffer if allocated */
            if (impl->state->tx_buf.data) {
                nx_mem_free(impl->state->tx_buf.data);
            }
            nx_mem_free(impl->state);
            impl->state = NULL;
            return;
        }
        stm32_uart_buffer_init(&impl->state->rx_buf, rx_data,
                               impl->state->config.rx_buf_size,
                               overflow_policy);
    } else {
        /* Buffer disabled - set to NULL */
        stm32_uart_buffer_init(&impl->state->rx_buf, NULL, 0, overflow_policy);
    }


    /* Clear callbacks */
    memset(&impl->callbacks, 0, sizeof(stm32_uart_callback_t));

#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    /* OSAL objects will be created in lifecycle_init */
    impl->mutex = NULL;
    impl->tx_sem = NULL;
    impl->rx_sem = NULL;
#endif
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
NX_UNUSED static void* stm32_uart_device_init(const nx_device_t* dev) {
    const stm32_uart_platform_config_t* config =
        (const stm32_uart_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    stm32_uart_impl_t* impl =
        (stm32_uart_impl_t*)nx_mem_alloc(sizeof(stm32_uart_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(stm32_uart_impl_t));

    /* Initialize instance with platform configuration */
    uart_init_instance(impl, config->uart_index, config);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Store device reference */
    impl->device = (nx_device_t*)dev;

    /* Device is created but not initialized - user will call lifecycle init()
     */
    return &impl->base;
}

/*---------------------------------------------------------------------------*/
/* UART Configuration Macro                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Define UART configuration structure
 * \param[in]       idx: UART instance index (0-5)
 */
#define STM32_UART_CONFIG(idx)                                                 \
    static const stm32_uart_platform_config_t uart_config_##idx = {            \
        .uart_index = idx,                                                     \
        .usart_base = USART1,                                                  \
        .baudrate = NX_CONFIG_STM32_UART##idx##_BAUDRATE,                      \
        .word_length = UART_WORDLENGTH_8B,                                     \
        .stop_bits = UART_STOPBITS_1,                                          \
        .parity = UART_PARITY_NONE,                                            \
        .mode = UART_MODE_TX_RX,                                               \
        .hw_flow_ctl = UART_HWCONTROL_NONE,                                    \
        .over_sampling = UART_OVERSAMPLING_16,                                 \
        .use_osal = false,                                                     \
        .use_dma = false,                                                      \
        .tx_buf_size = NX_CONFIG_STM32_UART##idx##_TX_BUFFER_SIZE,             \
        .rx_buf_size = NX_CONFIG_STM32_UART##idx##_RX_BUFFER_SIZE,             \
    }

/*---------------------------------------------------------------------------*/
/* UART Device Registration Macro                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register a UART device
 * \param[in]       idx: UART instance index (0-5)
 */
#define STM32_UART_DEVICE_REGISTER(idx)                                        \
    STM32_UART_CONFIG(idx);                                                    \
    static nx_device_config_state_t uart_state_##idx = {                       \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
        .api = NULL,                                                           \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, idx, "UART" #idx, &uart_config_##idx,      \
                       &uart_state_##idx, stm32_uart_device_init)

/*---------------------------------------------------------------------------*/
/* Instance Traversal                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register all enabled UART instances
 * \details         Expands NX_DEFINE_INSTANCE_STM32_UART macro from
 *                  nexus_config.h. Calls STM32_UART_DEVICE_REGISTER for
 *                  each enabled instance.
 */
NX_TRAVERSE_EACH_INSTANCE(STM32_UART_DEVICE_REGISTER, DEVICE_TYPE);
