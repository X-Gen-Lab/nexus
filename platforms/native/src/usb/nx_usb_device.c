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
#include "nexus_config.h"
#include "nx_usb_helpers.h"
#include "nx_usb_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_USB_MAX_INSTANCES 2
#define DEVICE_TYPE          NX_USB

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_usb_state_t g_usb_states[NX_USB_MAX_INSTANCES];
static nx_usb_impl_t g_usb_instances[NX_USB_MAX_INSTANCES];

/* Static buffer allocation - simplified for native platform */
#ifndef NX_CONFIG_USB_TX_BUFFER_SIZE
#define NX_CONFIG_USB_TX_BUFFER_SIZE 1024
#endif

#ifndef NX_CONFIG_USB_RX_BUFFER_SIZE
#define NX_CONFIG_USB_RX_BUFFER_SIZE 1024
#endif

static uint8_t g_usb0_tx_buffer[NX_CONFIG_USB_TX_BUFFER_SIZE];
static uint8_t g_usb0_rx_buffer[NX_CONFIG_USB_RX_BUFFER_SIZE];
static uint8_t g_usb1_tx_buffer[NX_CONFIG_USB_TX_BUFFER_SIZE];
static uint8_t g_usb1_rx_buffer[NX_CONFIG_USB_RX_BUFFER_SIZE];

/* Buffer pointer table */
uint8_t* g_usb_tx_buffers[NX_USB_MAX_INSTANCES] = {
    g_usb0_tx_buffer,
    g_usb1_tx_buffer,
};

uint8_t* g_usb_rx_buffers[NX_USB_MAX_INSTANCES] = {
    g_usb0_rx_buffer,
    g_usb1_rx_buffer,
};

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

    /* Link to state */
    impl->state = &g_usb_states[index];
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

    /* Initialize endpoints */
    for (uint8_t i = 0; i < NX_USB_MAX_ENDPOINTS; i++) {
        endpoint_init(&impl->state->endpoints[i]);
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_usb_stats_t));
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

    if (config == NULL || config->usb_index >= NX_USB_MAX_INSTANCES) {
        return NULL;
    }

    nx_usb_impl_t* impl = &g_usb_instances[config->usb_index];

    /* Initialize instance with platform configuration */
    usb_init_instance(impl, config->usb_index, config);

    /* Store device pointer */
    impl->device = (nx_device_t*)dev;

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
#define NX_USB_CONFIG(index)                                                   \
    static const nx_usb_platform_config_t usb_config_##index = {               \
        .usb_index = index,                                                    \
        .num_endpoints = CONFIG_USB##index##_NUM_ENDPOINTS,                    \
        .tx_buf_size = CONFIG_USB##index##_TX_BUFFER_SIZE,                     \
        .rx_buf_size = CONFIG_USB##index##_RX_BUFFER_SIZE,                     \
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
                       &usb_kconfig_state_##index, nx_usb_device_init)

/* Register all enabled USB instances */
NX_TRAVERSE_EACH_INSTANCE(NX_USB_DEVICE_REGISTER, DEVICE_TYPE);

/*---------------------------------------------------------------------------*/
/* Factory Functions (for testing)                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get USB instance
 */
nx_usb_t* nx_usb_native_get(uint8_t index) {
    if (index >= NX_USB_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "USB%d", index);
    return (nx_usb_t*)nx_device_get(name);
}

/**
 * \brief           Reset all USB instances (for testing)
 */
void nx_usb_native_reset_all(void) {
    for (uint8_t i = 0; i < NX_USB_MAX_INSTANCES; i++) {
        nx_usb_impl_t* impl = &g_usb_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_usb_states[i], 0, sizeof(nx_usb_state_t));

        /* Re-initialize endpoints */
        for (uint8_t j = 0; j < NX_USB_MAX_ENDPOINTS; j++) {
            endpoint_init(&g_usb_states[i].endpoints[j]);
        }
    }
}

/**
 * \brief           Inject data into RX buffer (for testing)
 */
nx_status_t nx_usb_native_inject_rx(uint8_t index, const uint8_t* data,
                                    size_t len) {
    if (index >= NX_USB_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_usb_impl_t* impl = &g_usb_instances[index];
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    size_t written = buffer_write(&impl->state->rx_buf, data, len);
    return (written == len) ? NX_OK : NX_ERR_FULL;
}

/**
 * \brief           Get USB state (for testing)
 */
nx_status_t nx_usb_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended) {
    if (index >= NX_USB_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    if (initialized == NULL || suspended == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_usb_state_t* state = &g_usb_states[index];
    *initialized = state->initialized;
    *suspended = state->suspended;

    return NX_OK;
}

/**
 * \brief           Simulate USB connect event (for testing)
 */
nx_status_t nx_usb_native_simulate_connect(uint8_t index) {
    if (index >= NX_USB_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    return usb_simulate_connect(&g_usb_states[index]);
}

/**
 * \brief           Simulate USB disconnect event (for testing)
 */
nx_status_t nx_usb_native_simulate_disconnect(uint8_t index) {
    if (index >= NX_USB_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    return usb_simulate_disconnect(&g_usb_states[index]);
}

/**
 * \brief           Simulate USB suspend event (for testing)
 */
nx_status_t nx_usb_native_simulate_suspend(uint8_t index) {
    if (index >= NX_USB_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    return usb_simulate_suspend(&g_usb_states[index]);
}

/**
 * \brief           Simulate USB resume event (for testing)
 */
nx_status_t nx_usb_native_simulate_resume(uint8_t index) {
    if (index >= NX_USB_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    return usb_simulate_resume(&g_usb_states[index]);
}

/**
 * \brief           Get USB device descriptor (for testing)
 */
nx_device_t* nx_usb_native_get_device(uint8_t index) {
    if (index >= NX_USB_MAX_INSTANCES) {
        return NULL;
    }
    return g_usb_instances[index].device;
}
