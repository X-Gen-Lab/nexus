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
#include "nexus_config.h"
#include "nx_uart_helpers.h"
#include "nx_uart_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_UART_MAX_INSTANCES 4
#define DEVICE_TYPE           NX_UART

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_uart_state_t g_uart_states[NX_UART_MAX_INSTANCES];
static nx_uart_impl_t g_uart_instances[NX_UART_MAX_INSTANCES];

/* Dynamic buffer allocation based on Kconfig */
static uint8_t g_uart0_tx_buffer[NX_CONFIG_UART0_TX_BUFFER_SIZE];
static uint8_t g_uart0_rx_buffer[NX_CONFIG_UART0_RX_BUFFER_SIZE];
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_1
static uint8_t g_uart1_tx_buffer[NX_CONFIG_UART1_TX_BUFFER_SIZE];
static uint8_t g_uart1_rx_buffer[NX_CONFIG_UART1_RX_BUFFER_SIZE];
#endif
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_2
static uint8_t g_uart2_tx_buffer[NX_CONFIG_UART2_TX_BUFFER_SIZE];
static uint8_t g_uart2_rx_buffer[NX_CONFIG_UART2_RX_BUFFER_SIZE];
#endif
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_3
static uint8_t g_uart3_tx_buffer[NX_CONFIG_UART3_TX_BUFFER_SIZE];
static uint8_t g_uart3_rx_buffer[NX_CONFIG_UART3_RX_BUFFER_SIZE];
#endif

/* Buffer pointer table */
static uint8_t* g_uart_tx_buffers[NX_UART_MAX_INSTANCES] = {
    g_uart0_tx_buffer,
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_1
    g_uart1_tx_buffer,
#else
    NULL,
#endif
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_2
    g_uart2_tx_buffer,
#else
    NULL,
#endif
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_3
    g_uart3_tx_buffer,
#else
    NULL,
#endif
};

static uint8_t* g_uart_rx_buffers[NX_UART_MAX_INSTANCES] = {
    g_uart0_rx_buffer,
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_1
    g_uart1_rx_buffer,
#else
    NULL,
#endif
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_2
    g_uart2_rx_buffer,
#else
    NULL,
#endif
#ifdef NX_CONFIG_INSTANCE_NATIVE_UART_3
    g_uart3_rx_buffer,
#else
    NULL,
#endif
};

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

    /* Link to state */
    impl->state = &g_uart_states[index];
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

    if (config == NULL || config->uart_index >= NX_UART_MAX_INSTANCES) {
        return NULL;
    }

    nx_uart_impl_t* impl = &g_uart_instances[config->uart_index];

    /* Initialize instance with platform configuration */
    uart_init_instance(impl, config->uart_index, config);

    /* Initialize lifecycle */
    nx_status_t status = impl->lifecycle.init(&impl->lifecycle);
    if (status != NX_OK) {
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
        .baudrate = CONFIG_UART##index##_BAUDRATE,                             \
        .word_length = CONFIG_UART##index##_DATA_BITS,                         \
        .stop_bits = CONFIG_UART##index##_STOP_BITS,                           \
        .parity = CONFIG_UART##index##_PARITY_VALUE,                           \
        .flow_control = 0,                                                     \
        .tx_buf_size = CONFIG_UART##index##_TX_BUFFER_SIZE,                    \
        .rx_buf_size = CONFIG_UART##index##_RX_BUFFER_SIZE,                    \
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
NX_TRAVERSE_EACH_INSTANCE(NX_UART_DEVICE_REGISTER, DEVICE_TYPE);

/*---------------------------------------------------------------------------*/
/* Legacy Factory Functions (for backward compatibility)                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get UART instance (legacy)
 */
nx_uart_t* nx_uart_native_get(uint8_t index) {
    if (index >= NX_UART_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "UART%d", index);
    return (nx_uart_t*)nx_device_get(name);
}

/**
 * \brief           Get UART instance with configuration (deprecated)
 * \deprecated      Configuration should be done via Kconfig
 */
nx_uart_t* nx_uart_native_get_with_config(uint8_t index,
                                          const nx_uart_config_t* cfg) {
    /* Configuration is now compile-time only, ignore cfg parameter */
    (void)cfg;
    return nx_uart_native_get(index);
}

/**
 * \brief           Reset all UART instances (for testing)
 */
void nx_uart_native_reset_all(void) {
    for (uint8_t i = 0; i < NX_UART_MAX_INSTANCES; i++) {
        nx_uart_impl_t* impl = &g_uart_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_uart_states[i], 0, sizeof(nx_uart_state_t));
    }
}

/**
 * \brief           Inject data into RX buffer (for testing)
 */
nx_status_t nx_uart_native_inject_rx(uint8_t index, const uint8_t* data,
                                     size_t len) {
    if (index >= NX_UART_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_uart_impl_t* impl = &g_uart_instances[index];
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    size_t written = buffer_write(&impl->state->rx_buf, data, len);
    return (written == len) ? NX_OK : NX_ERR_FULL;
}

/**
 * \brief           Get UART device descriptor (for testing)
 */
nx_device_t* nx_uart_native_get_device(uint8_t index) {
    if (index >= NX_UART_MAX_INSTANCES) {
        return NULL;
    }
    return g_uart_instances[index].device;
}
