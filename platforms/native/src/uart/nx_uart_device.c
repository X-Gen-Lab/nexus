/**
 * \file            nx_uart_device.c
 * \brief           UART device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages UART instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_uart.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_uart_helpers.h"
#include "nx_uart_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_UART

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
static nx_diagnostic_t* uart_get_diagnostic(nx_uart_t* self);

/* Interface implementations (defined in separate files) */
extern void uart_init_tx_async(nx_tx_async_t* tx_async);
extern void uart_init_rx_async(nx_rx_async_t* rx_async);
extern void uart_init_tx_sync(nx_tx_sync_t* tx_sync);
extern void uart_init_rx_sync(nx_rx_sync_t* rx_sync);
extern void uart_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void uart_init_power(nx_power_t* power);
extern void uart_init_diagnostic(nx_diagnostic_t* diagnostic);

/*---------------------------------------------------------------------------*/
/* Base Interface Getters                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get TX async interface
 */
static nx_tx_async_t* uart_get_tx_async(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->tx_async : NULL;
}

/**
 * \brief           Get RX async interface
 */
static nx_rx_async_t* uart_get_rx_async(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->rx_async : NULL;
}

/**
 * \brief           Get TX sync interface
 */
static nx_tx_sync_t* uart_get_tx_sync(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->tx_sync : NULL;
}

/**
 * \brief           Get RX sync interface
 */
static nx_rx_sync_t* uart_get_rx_sync(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->rx_sync : NULL;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* uart_get_lifecycle(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* uart_get_power(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Get diagnostic interface
 */
static nx_diagnostic_t* uart_get_diagnostic(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize UART instance with platform configuration
 */
static void uart_init_instance(nx_uart_impl_t* impl, uint8_t index,
                               const nx_uart_platform_config_t* platform_cfg) {
    /* Initialize base interface */
    impl->base.get_tx_async = uart_get_tx_async;
    impl->base.get_rx_async = uart_get_rx_async;
    impl->base.get_tx_sync = uart_get_tx_sync;
    impl->base.get_rx_sync = uart_get_rx_sync;
    impl->base.get_lifecycle = uart_get_lifecycle;
    impl->base.get_power = uart_get_power;
    impl->base.get_diagnostic = uart_get_diagnostic;

    /* Initialize interfaces (implemented in separate files) */
    uart_init_tx_async(&impl->tx_async);
    uart_init_rx_async(&impl->rx_async);
    uart_init_tx_sync(&impl->tx_sync);
    uart_init_rx_sync(&impl->rx_sync);
    uart_init_lifecycle(&impl->lifecycle);
    uart_init_power(&impl->power);
    uart_init_diagnostic(&impl->diagnostic);

    /* Allocate and initialize state */
    impl->state = (nx_uart_state_t*)nx_mem_alloc(sizeof(nx_uart_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_uart_state_t));

    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->tx_busy = false;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.baudrate = platform_cfg->baudrate;
        impl->state->config.word_length = platform_cfg->word_length;
        impl->state->config.stop_bits = platform_cfg->stop_bits;
        impl->state->config.parity = platform_cfg->parity;
        impl->state->config.flow_control = platform_cfg->flow_control;
        impl->state->config.dma_tx_enable = false;
        impl->state->config.dma_rx_enable = false;
        impl->state->config.tx_buf_size = platform_cfg->tx_buf_size;
        impl->state->config.rx_buf_size = platform_cfg->rx_buf_size;

        /* Allocate buffers dynamically */
        impl->state->tx_buf.data =
            (uint8_t*)nx_mem_alloc(platform_cfg->tx_buf_size);
        impl->state->tx_buf.size = platform_cfg->tx_buf_size;
        impl->state->tx_buf.head = 0;
        impl->state->tx_buf.tail = 0;
        impl->state->tx_buf.count = 0;

        impl->state->rx_buf.data =
            (uint8_t*)nx_mem_alloc(platform_cfg->rx_buf_size);
        impl->state->rx_buf.size = platform_cfg->rx_buf_size;
        impl->state->rx_buf.head = 0;
        impl->state->rx_buf.tail = 0;
        impl->state->rx_buf.count = 0;
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_uart_stats_t));
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_uart_device_init(const nx_device_t* dev) {
    const nx_uart_platform_config_t* config =
        (const nx_uart_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_uart_impl_t* impl =
        (nx_uart_impl_t*)nx_mem_alloc(sizeof(nx_uart_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_uart_impl_t));

    /* Initialize instance with platform configuration */
    uart_init_instance(impl, config->uart_index, config);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Initialize lifecycle */
    nx_status_t status = impl->lifecycle.init(&impl->lifecycle);
    if (status != NX_OK) {
        if (impl->state) {
            if (impl->state->tx_buf.data) {
                nx_mem_free(impl->state->tx_buf.data);
            }
            if (impl->state->rx_buf.data) {
                nx_mem_free(impl->state->rx_buf.data);
            }
            nx_mem_free(impl->state);
        }
        nx_mem_free(impl);
        return NULL;
    }

    return &impl->base;
}

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_UART_CONFIG(index)                                                  \
    static const nx_uart_platform_config_t uart_config_##index = {             \
        .uart_index = index,                                                   \
        .baudrate = NX_CONFIG_UART##index##_BAUDRATE,                          \
        .word_length = NX_CONFIG_UART##index##_DATA_BITS,                      \
        .stop_bits = NX_CONFIG_UART##index##_STOP_BITS,                        \
        .parity = NX_CONFIG_UART##index##_PARITY_VALUE,                        \
        .flow_control = 0,                                                     \
        .tx_buf_size = NX_CONFIG_UART##index##_TX_BUFFER_SIZE,                 \
        .rx_buf_size = NX_CONFIG_UART##index##_RX_BUFFER_SIZE,                 \
    }

/**
 * \brief           Device registration macro
 */
#define NX_UART_DEVICE_REGISTER(index)                                         \
    NX_UART_CONFIG(index);                                                     \
    static nx_device_config_state_t uart_kconfig_state_##index = {             \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "UART" #index,                      \
                       &uart_config_##index, &uart_kconfig_state_##index,      \
                       nx_uart_device_init)

/* Register all enabled UART instances */
#ifndef _MSC_VER
NX_TRAVERSE_EACH_INSTANCE(NX_UART_DEVICE_REGISTER, DEVICE_TYPE);
#else
/* MSVC: Temporarily disabled due to macro compatibility issues */
#pragma message(                                                               \
    "UART device registration disabled on MSVC - TODO: Fix NX_DEVICE_REGISTER macro")
#endif
