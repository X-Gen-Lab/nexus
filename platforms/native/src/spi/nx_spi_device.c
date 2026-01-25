/**
 * \file            nx_spi_device.c
 * \brief           SPI device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SPI device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages SPI instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_spi.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_spi_helpers.h"
#include "nx_spi_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_SPI

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface getters */
static nx_tx_async_t* spi_get_tx_async_handle(nx_spi_bus_t* self,
                                              nx_spi_device_config_t config);
static nx_tx_rx_async_t*
spi_get_tx_rx_async_handle(nx_spi_bus_t* self, nx_spi_device_config_t config,
                           nx_comm_callback_t callback, void* user_data);
static nx_tx_sync_t* spi_get_tx_sync_handle(nx_spi_bus_t* self,
                                            nx_spi_device_config_t config);
static nx_tx_rx_sync_t*
spi_get_tx_rx_sync_handle(nx_spi_bus_t* self, nx_spi_device_config_t config);
static nx_lifecycle_t* spi_get_lifecycle(nx_spi_bus_t* self);
static nx_power_t* spi_get_power(nx_spi_bus_t* self);
static nx_diagnostic_t* spi_get_diagnostic(nx_spi_bus_t* self);

/* Interface implementations (defined in separate files) */
extern void spi_init_tx_async(nx_tx_async_t* tx_async);
extern void spi_init_tx_rx_async(nx_tx_rx_async_t* tx_rx_async);
extern void spi_init_tx_sync(nx_tx_sync_t* tx_sync);
extern void spi_init_tx_rx_sync(nx_tx_rx_sync_t* tx_rx_sync);
extern void spi_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void spi_init_power(nx_power_t* power);
extern void spi_init_diagnostic(nx_diagnostic_t* diagnostic);

/*---------------------------------------------------------------------------*/
/* Base Interface Getters                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get TX async handle
 */
static nx_tx_async_t* spi_get_tx_async_handle(nx_spi_bus_t* self,
                                              nx_spi_device_config_t config) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }

    /* Store device configuration */
    impl->state->current_device.config = config;
    impl->state->current_device.in_use = true;

    return &impl->tx_async;
}

/**
 * \brief           Get TX/RX async handle
 */
static nx_tx_rx_async_t*
spi_get_tx_rx_async_handle(nx_spi_bus_t* self, nx_spi_device_config_t config,
                           nx_comm_callback_t callback, void* user_data) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }

    /* Store device configuration and callback */
    impl->state->current_device.config = config;
    impl->state->current_device.callback = callback;
    impl->state->current_device.user_data = user_data;
    impl->state->current_device.in_use = true;

    return &impl->tx_rx_async;
}

/**
 * \brief           Get TX sync handle
 */
static nx_tx_sync_t* spi_get_tx_sync_handle(nx_spi_bus_t* self,
                                            nx_spi_device_config_t config) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }

    /* Store device configuration */
    impl->state->current_device.config = config;
    impl->state->current_device.in_use = true;

    return &impl->tx_sync;
}

/**
 * \brief           Get TX/RX sync handle
 */
static nx_tx_rx_sync_t*
spi_get_tx_rx_sync_handle(nx_spi_bus_t* self, nx_spi_device_config_t config) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }

    /* Store device configuration */
    impl->state->current_device.config = config;
    impl->state->current_device.in_use = true;

    return &impl->tx_rx_sync;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* spi_get_lifecycle(nx_spi_bus_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* spi_get_power(nx_spi_bus_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Get diagnostic interface
 */
static nx_diagnostic_t* spi_get_diagnostic(nx_spi_bus_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize SPI instance with platform configuration
 */
static void spi_init_instance(nx_spi_impl_t* impl, uint8_t index,
                              const nx_spi_platform_config_t* platform_cfg) {
    /* Initialize base interface */
    impl->base.get_tx_async_handle = spi_get_tx_async_handle;
    impl->base.get_tx_rx_async_handle = spi_get_tx_rx_async_handle;
    impl->base.get_tx_sync_handle = spi_get_tx_sync_handle;
    impl->base.get_tx_rx_sync_handle = spi_get_tx_rx_sync_handle;
    impl->base.get_lifecycle = spi_get_lifecycle;
    impl->base.get_power = spi_get_power;
    impl->base.get_diagnostic = spi_get_diagnostic;

    /* Initialize interfaces (implemented in separate files) */
    spi_init_tx_async(&impl->tx_async);
    spi_init_tx_rx_async(&impl->tx_rx_async);
    spi_init_tx_sync(&impl->tx_sync);
    spi_init_tx_rx_sync(&impl->tx_rx_sync);
    spi_init_lifecycle(&impl->lifecycle);
    spi_init_power(&impl->power);
    spi_init_diagnostic(&impl->diagnostic);

    /* Allocate and initialize state */
    impl->state = (nx_spi_state_t*)nx_mem_alloc(sizeof(nx_spi_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_spi_state_t));

    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->busy = false;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.max_speed = platform_cfg->max_speed;
        impl->state->config.mosi_pin = platform_cfg->mosi_pin;
        impl->state->config.miso_pin = platform_cfg->miso_pin;
        impl->state->config.sck_pin = platform_cfg->sck_pin;
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
    memset(&impl->state->stats, 0, sizeof(nx_spi_stats_t));

    /* Clear device handle */
    memset(&impl->state->current_device, 0, sizeof(nx_spi_device_handle_t));
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_spi_device_init(const nx_device_t* dev) {
    const nx_spi_platform_config_t* config =
        (const nx_spi_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_spi_impl_t* impl = (nx_spi_impl_t*)nx_mem_alloc(sizeof(nx_spi_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_spi_impl_t));

    /* Initialize instance with platform configuration */
    spi_init_instance(impl, config->spi_index, config);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Device is created but not initialized - tests will call init() */
    return &impl->base;
}

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_SPI_CONFIG(index)                                                   \
    static const nx_spi_platform_config_t spi_config_##index = {               \
        .spi_index = index,                                                    \
        .max_speed = NX_CONFIG_SPI##index##_MAX_SPEED,                         \
        .mosi_pin = 1,                                                         \
        .miso_pin = 2,                                                         \
        .sck_pin = 3,                                                          \
        .tx_buf_size = NX_CONFIG_SPI##index##_TX_BUFFER_SIZE,                  \
        .rx_buf_size = NX_CONFIG_SPI##index##_RX_BUFFER_SIZE,                  \
    }

/**
 * \brief           Device registration macro
 */
#define NX_SPI_DEVICE_REGISTER(index)                                          \
    NX_SPI_CONFIG(index);                                                      \
    static nx_device_config_state_t spi_kconfig_state_##index = {              \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "SPI" #index, &spi_config_##index,  \
                       &spi_kconfig_state_##index, nx_spi_device_init);

/**
 * \brief           Register all enabled SPI instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_SPI_DEVICE_REGISTER, DEVICE_TYPE);
