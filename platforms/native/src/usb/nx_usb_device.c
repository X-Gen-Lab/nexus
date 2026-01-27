/**
 * \file            nx_usb_device.c
 * \brief           USB device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements USB device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages USB instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_usb.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_usb_helpers.h"
#include "nx_usb_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_USB

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface getters */
static nx_tx_async_t* usb_get_tx_async(nx_usb_t* self);
static nx_rx_async_t* usb_get_rx_async(nx_usb_t* self);
static nx_tx_sync_t* usb_get_tx_sync(nx_usb_t* self);
static nx_rx_sync_t* usb_get_rx_sync(nx_usb_t* self);
static bool usb_is_connected(nx_usb_t* self);
static nx_lifecycle_t* usb_get_lifecycle(nx_usb_t* self);
static nx_power_t* usb_get_power(nx_usb_t* self);

/* Interface implementations (defined in separate files) */
extern void usb_init_tx_async(nx_tx_async_t* tx_async);
extern void usb_init_rx_async(nx_rx_async_t* rx_async);
extern void usb_init_tx_sync(nx_tx_sync_t* tx_sync);
extern void usb_init_rx_sync(nx_rx_sync_t* rx_sync);
extern void usb_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void usb_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Base Interface Getters                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get TX async interface
 */
static nx_tx_async_t* usb_get_tx_async(nx_usb_t* self) {
    nx_usb_impl_t* impl = usb_get_impl(self);
    return impl ? &impl->tx_async : NULL;
}

/**
 * \brief           Get RX async interface
 */
static nx_rx_async_t* usb_get_rx_async(nx_usb_t* self) {
    nx_usb_impl_t* impl = usb_get_impl(self);
    return impl ? &impl->rx_async : NULL;
}

/**
 * \brief           Get TX sync interface
 */
static nx_tx_sync_t* usb_get_tx_sync(nx_usb_t* self) {
    nx_usb_impl_t* impl = usb_get_impl(self);
    return impl ? &impl->tx_sync : NULL;
}

/**
 * \brief           Get RX sync interface
 */
static nx_rx_sync_t* usb_get_rx_sync(nx_usb_t* self) {
    nx_usb_impl_t* impl = usb_get_impl(self);
    return impl ? &impl->rx_sync : NULL;
}

/**
 * \brief           Check USB connection status
 */
static bool usb_is_connected(nx_usb_t* self) {
    nx_usb_impl_t* impl = usb_get_impl(self);
    nx_usb_state_t* state = usb_get_state(impl);
    return (state != NULL && state->initialized) ? state->connected : false;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* usb_get_lifecycle(nx_usb_t* self) {
    nx_usb_impl_t* impl = usb_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* usb_get_power(nx_usb_t* self) {
    nx_usb_impl_t* impl = usb_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize USB instance with platform configuration
 */
static void usb_init_instance(nx_usb_impl_t* impl, uint8_t index,
                              const nx_usb_platform_config_t* platform_cfg) {
    /* Initialize base interface */
    NX_INIT_USB(&impl->base, usb_get_tx_async, usb_get_rx_async,
                usb_get_tx_sync, usb_get_rx_sync, usb_is_connected,
                usb_get_lifecycle, usb_get_power);

    /* Initialize interfaces (implemented in separate files) */
    usb_init_tx_async(&impl->tx_async);
    usb_init_rx_async(&impl->rx_async);
    usb_init_tx_sync(&impl->tx_sync);
    usb_init_rx_sync(&impl->rx_sync);
    usb_init_lifecycle(&impl->lifecycle);
    usb_init_power(&impl->power);

    /* Allocate and initialize state */
    impl->state = (nx_usb_state_t*)nx_mem_alloc(sizeof(nx_usb_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_usb_state_t));

    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->connected = false;
    impl->state->tx_busy = false;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.num_endpoints = platform_cfg->num_endpoints;
        impl->state->config.tx_buf_size = platform_cfg->tx_buf_size;
        impl->state->config.rx_buf_size = platform_cfg->rx_buf_size;
    }

    /* Allocate TX and RX buffer data */
    impl->state->tx_buf.data =
        (uint8_t*)nx_mem_alloc(impl->state->config.tx_buf_size);
    impl->state->rx_buf.data =
        (uint8_t*)nx_mem_alloc(impl->state->config.rx_buf_size);

    if (!impl->state->tx_buf.data || !impl->state->rx_buf.data) {
        if (impl->state->tx_buf.data) {
            nx_mem_free(impl->state->tx_buf.data);
        }
        if (impl->state->rx_buf.data) {
            nx_mem_free(impl->state->rx_buf.data);
        }
        nx_mem_free(impl->state);
        impl->state = NULL;
        return;
    }

    impl->state->tx_buf.size = impl->state->config.tx_buf_size;
    impl->state->rx_buf.size = impl->state->config.rx_buf_size;

    /* Initialize endpoints */
    for (uint8_t i = 0; i < NX_USB_MAX_ENDPOINTS; i++) {
        endpoint_init(&impl->state->endpoints[i]);
    }
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_usb_device_init(const nx_device_t* dev) {
    const nx_usb_platform_config_t* config =
        (const nx_usb_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_usb_impl_t* impl = (nx_usb_impl_t*)nx_mem_alloc(sizeof(nx_usb_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_usb_impl_t));

    /* Initialize instance with platform configuration */
    usb_init_instance(impl, config->usb_index, config);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Store device pointer */
    impl->device = (nx_device_t*)dev;

    /* Device is created but not initialized - tests will call init() */
    return &impl->base;
}

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_USB_CONFIG(index)                                                   \
    static const nx_usb_platform_config_t usb_config_##index = {               \
        .usb_index = index,                                                    \
        .num_endpoints = NX_CONFIG_USB##index##_NUM_ENDPOINTS,                 \
        .tx_buf_size = NX_CONFIG_USB##index##_TX_BUFFER_SIZE,                  \
        .rx_buf_size = NX_CONFIG_USB##index##_RX_BUFFER_SIZE,                  \
    }

/**
 * \brief           Device registration macro
 */
#define NX_USB_DEVICE_REGISTER(index)                                          \
    NX_USB_CONFIG(index);                                                      \
    static nx_device_config_state_t usb_kconfig_state_##index = {              \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "USB" #index, &usb_config_##index,  \
                       &usb_kconfig_state_##index, nx_usb_device_init);

/**
 * \brief           Register all enabled USB instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_USB_DEVICE_REGISTER, DEVICE_TYPE)
