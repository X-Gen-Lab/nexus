/**
 * \file            nx_i2c_device.c
 * \brief           I2C device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements I2C device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages I2C instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_i2c.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_i2c_helpers.h"
#include "nx_i2c_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_I2C

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface getters */
static nx_tx_async_t* i2c_get_tx_async_handle(nx_i2c_bus_t* self,
                                              uint8_t dev_addr);
static nx_tx_rx_async_t* i2c_get_tx_rx_async_handle(nx_i2c_bus_t* self,
                                                    uint8_t dev_addr,
                                                    nx_comm_callback_t callback,
                                                    void* user_data);
static nx_tx_sync_t* i2c_get_tx_sync_handle(nx_i2c_bus_t* self,
                                            uint8_t dev_addr);
static nx_tx_rx_sync_t* i2c_get_tx_rx_sync_handle(nx_i2c_bus_t* self,
                                                  uint8_t dev_addr);
static nx_lifecycle_t* i2c_get_lifecycle(nx_i2c_bus_t* self);
static nx_power_t* i2c_get_power(nx_i2c_bus_t* self);
static nx_diagnostic_t* i2c_get_diagnostic(nx_i2c_bus_t* self);

/* Interface implementations (defined in separate files) */
extern void i2c_init_tx_async(nx_tx_async_t* tx_async);
extern void i2c_init_tx_rx_async(nx_tx_rx_async_t* tx_rx_async);
extern void i2c_init_tx_sync(nx_tx_sync_t* tx_sync);
extern void i2c_init_tx_rx_sync(nx_tx_rx_sync_t* tx_rx_sync);
extern void i2c_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void i2c_init_power(nx_power_t* power);
extern void i2c_init_diagnostic(nx_diagnostic_t* diagnostic);

/*---------------------------------------------------------------------------*/
/* Base Interface Getters                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get TX async handle
 */
static nx_tx_async_t* i2c_get_tx_async_handle(nx_i2c_bus_t* self,
                                              uint8_t dev_addr) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }

    /* Store device address */
    impl->state->current_device.dev_addr = dev_addr;
    impl->state->current_device.in_use = true;

    return &impl->tx_async;
}

/**
 * \brief           Get TX/RX async handle
 */
static nx_tx_rx_async_t* i2c_get_tx_rx_async_handle(nx_i2c_bus_t* self,
                                                    uint8_t dev_addr,
                                                    nx_comm_callback_t callback,
                                                    void* user_data) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }

    /* Store device address and callback */
    impl->state->current_device.dev_addr = dev_addr;
    impl->state->current_device.callback = callback;
    impl->state->current_device.user_data = user_data;
    impl->state->current_device.in_use = true;

    return &impl->tx_rx_async;
}

/**
 * \brief           Get TX sync handle
 */
static nx_tx_sync_t* i2c_get_tx_sync_handle(nx_i2c_bus_t* self,
                                            uint8_t dev_addr) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }

    /* Store device address */
    impl->state->current_device.dev_addr = dev_addr;
    impl->state->current_device.in_use = true;

    return &impl->tx_sync;
}

/**
 * \brief           Get TX/RX sync handle
 */
static nx_tx_rx_sync_t* i2c_get_tx_rx_sync_handle(nx_i2c_bus_t* self,
                                                  uint8_t dev_addr) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }

    /* Store device address */
    impl->state->current_device.dev_addr = dev_addr;
    impl->state->current_device.in_use = true;

    return &impl->tx_rx_sync;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* i2c_get_lifecycle(nx_i2c_bus_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* i2c_get_power(nx_i2c_bus_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Get diagnostic interface
 */
static nx_diagnostic_t* i2c_get_diagnostic(nx_i2c_bus_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize I2C instance with platform configuration
 */
static void i2c_init_instance(nx_i2c_impl_t* impl, uint8_t index,
                              const nx_i2c_platform_config_t* platform_cfg) {
    /* Initialize base interface */
    impl->base.get_tx_async_handle = i2c_get_tx_async_handle;
    impl->base.get_tx_rx_async_handle = i2c_get_tx_rx_async_handle;
    impl->base.get_tx_sync_handle = i2c_get_tx_sync_handle;
    impl->base.get_tx_rx_sync_handle = i2c_get_tx_rx_sync_handle;
    impl->base.get_lifecycle = i2c_get_lifecycle;
    impl->base.get_power = i2c_get_power;
    impl->base.get_diagnostic = i2c_get_diagnostic;

    /* Initialize interfaces (implemented in separate files) */
    i2c_init_tx_async(&impl->tx_async);
    i2c_init_tx_rx_async(&impl->tx_rx_async);
    i2c_init_tx_sync(&impl->tx_sync);
    i2c_init_tx_rx_sync(&impl->tx_rx_sync);
    i2c_init_lifecycle(&impl->lifecycle);
    i2c_init_power(&impl->power);
    i2c_init_diagnostic(&impl->diagnostic);

    /* Allocate and initialize state */
    impl->state = (nx_i2c_state_t*)nx_mem_alloc(sizeof(nx_i2c_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_i2c_state_t));

    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->busy = false;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.speed = platform_cfg->speed;
        impl->state->config.scl_pin = platform_cfg->scl_pin;
        impl->state->config.sda_pin = platform_cfg->sda_pin;
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
    memset(&impl->state->stats, 0, sizeof(nx_i2c_stats_t));

    /* Clear device handle */
    memset(&impl->state->current_device, 0, sizeof(nx_i2c_device_handle_t));
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_i2c_device_init(const nx_device_t* dev) {
    const nx_i2c_platform_config_t* config =
        (const nx_i2c_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_i2c_impl_t* impl = (nx_i2c_impl_t*)nx_mem_alloc(sizeof(nx_i2c_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_i2c_impl_t));

    /* Initialize instance with platform configuration */
    i2c_init_instance(impl, config->i2c_index, config);

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
#define NX_I2C_CONFIG(index)                                                   \
    static const nx_i2c_platform_config_t i2c_config_##index = {               \
        .i2c_index = index,                                                    \
        .speed = NX_CONFIG_I2C##index##_SPEED,                                 \
        .scl_pin = 0,                                                          \
        .sda_pin = 1,                                                          \
        .tx_buf_size = NX_CONFIG_I2C##index##_TX_BUFFER_SIZE,                  \
        .rx_buf_size = NX_CONFIG_I2C##index##_RX_BUFFER_SIZE,                  \
    }

/**
 * \brief           Device registration macro
 */
#define NX_I2C_DEVICE_REGISTER(index)                                          \
    NX_I2C_CONFIG(index);                                                      \
    static nx_device_config_state_t i2c_kconfig_state_##index = {              \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "I2C" #index, &i2c_config_##index,  \
                       &i2c_kconfig_state_##index, nx_i2c_device_init);

/**
 * \brief           Register all enabled I2C instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_I2C_DEVICE_REGISTER, DEVICE_TYPE);
