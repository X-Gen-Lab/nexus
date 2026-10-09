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

#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
#include "hal/interface/nx_uart.h"
#include "nexus_config.h"
#include "nx_uart_helpers.h"
#include "nx_uart_types.h"
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

/* Interface implementations (defined in separate files) */
extern void uart_init_tx_async(nx_tx_async_t* tx_async);
extern void uart_init_rx_async(nx_rx_async_t* rx_async);
extern void uart_init_tx_sync(nx_tx_sync_t* tx_sync);
extern void uart_init_rx_sync(nx_rx_sync_t* rx_sync);
extern void uart_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void uart_init_power(nx_power_t* power);

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

/** Descriptor-owned storage: construction cannot fail halfway through an
 * allocation or retarget an escaped pointer on close/reopen. */
typedef struct {
    nx_device_config_state_t core;
    nx_uart_impl_t impl;
    nx_uart_state_t state;
    uint8_t* tx;
    uint8_t* rx;
    nx_uart_rx_event_t* events;
    size_t tx_size, rx_size, event_capacity;
} native_uart_storage_t;

static nx_status_t nx_uart_construct(const nx_device_t* dev, void** out) {
    if (!out)
        return NX_ERR_NULL_PTR;
    *out = NULL;
    if (!dev || !dev->state || !dev->config)
        return NX_ERR_INVALID_PARAM;
    const nx_uart_platform_config_t* cfg = dev->config;
    native_uart_storage_t* storage =
        NX_CONTAINER_OF(dev->state, native_uart_storage_t, core);
    if (!cfg->baudrate || cfg->word_length < 5 || cfg->word_length > 9 ||
        (cfg->stop_bits != 1 && cfg->stop_bits != 2) || cfg->parity > 2 ||
        cfg->flow_control || !cfg->tx_buf_size || !cfg->rx_buf_size ||
        !storage->tx || !storage->rx || !storage->events ||
        storage->tx_size != cfg->tx_buf_size ||
        storage->rx_size != cfg->rx_buf_size ||
        storage->event_capacity != cfg->rx_buf_size)
        return NX_ERR_INVALID_PARAM;
    nx_uart_impl_t* impl = &storage->impl;
    memset(impl, 0, sizeof(*impl));
    impl->state = &storage->state;
    memset(impl->state, 0, sizeof(*impl->state));
    impl->state->index = cfg->uart_index;
    impl->state->config =
        (nx_uart_config_t){cfg->baudrate, cfg->word_length,  cfg->stop_bits,
                           cfg->parity,   cfg->flow_control, false,
                           false,         cfg->tx_buf_size,  cfg->rx_buf_size};
    buffer_init(&impl->state->tx_buf, storage->tx, cfg->tx_buf_size);
    buffer_init(&impl->state->rx_buf, storage->rx, cfg->rx_buf_size);
    impl->device = (nx_device_t*)dev;
    impl->rx_events = storage->events;
    impl->rx_event_capacity = storage->event_capacity;
    NX_INIT_UART(&impl->base, uart_get_tx_async, uart_get_rx_async,
                 uart_get_tx_sync, uart_get_rx_sync, uart_get_lifecycle,
                 uart_get_power);
    uart_init_tx_async(&impl->tx_async);
    uart_init_rx_async(&impl->rx_async);
    uart_init_tx_sync(&impl->tx_sync);
    uart_init_rx_sync(&impl->rx_sync);
    uart_init_lifecycle(&impl->lifecycle);
    uart_init_power(&impl->power);
    native_uart_init_operations(impl);
    *out = &impl->base;
    return NX_OK;
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
    static uint8_t uart_tx_##index[NX_CONFIG_UART##index##_TX_BUFFER_SIZE];    \
    static uint8_t uart_rx_##index[NX_CONFIG_UART##index##_RX_BUFFER_SIZE];    \
    static nx_uart_rx_event_t uart_events_##index[NX_CONFIG_UART##index##_RX_BUFFER_SIZE];           \
    static native_uart_storage_t uart_storage_##index = {                      \
        .tx = uart_tx_##index, .rx = uart_rx_##index,                                                 \
        .tx_size = sizeof(uart_tx_##index), .rx_size = sizeof(uart_rx_##index),                                    \
        .events = uart_events_##index,                                         \
        .event_capacity = sizeof(uart_events_##index) / sizeof(uart_events_##index[0]),      \
    };                                                                         \
    NX_DEVICE_REGISTER_TYPED(DEVICE_TYPE, index, "UART" #index,               \
        &uart_config_##index, &uart_storage_##index.core,                      \
        NX_DEVICE_CLASS_UART,                      \
        NX_DEVICE_CAP_UART_OPERATIONS | NX_DEVICE_CAP_UART_CANCEL |            \
            NX_DEVICE_CAP_UART_RX_EVENTS, nx_uart_construct, NULL);

/**
 * \brief           Register all enabled UART instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_UART_DEVICE_REGISTER, DEVICE_TYPE)
