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
#include "nexus_config.h"
#include "nx_i2c_helpers.h"
#include "nx_i2c_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_I2C_MAX_INSTANCES 4
#define DEVICE_TYPE          NX_I2C

/* Fallback definitions for Kconfig macros (if not generated yet) */
#ifndef CONFIG_I2C0_TX_BUFFER_SIZE
#define CONFIG_I2C0_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_I2C0_RX_BUFFER_SIZE
#define CONFIG_I2C0_RX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_I2C0_SPEED
#define CONFIG_I2C0_SPEED 100000
#endif
#ifndef CONFIG_I2C0_SCL_PIN
#define CONFIG_I2C0_SCL_PIN 5
#endif
#ifndef CONFIG_I2C0_SDA_PIN
#define CONFIG_I2C0_SDA_PIN 4
#endif

#ifndef CONFIG_I2C1_TX_BUFFER_SIZE
#define CONFIG_I2C1_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_I2C1_RX_BUFFER_SIZE
#define CONFIG_I2C1_RX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_I2C1_SPEED
#define CONFIG_I2C1_SPEED 100000
#endif
#ifndef CONFIG_I2C1_SCL_PIN
#define CONFIG_I2C1_SCL_PIN 15
#endif
#ifndef CONFIG_I2C1_SDA_PIN
#define CONFIG_I2C1_SDA_PIN 14
#endif

#ifndef CONFIG_I2C2_TX_BUFFER_SIZE
#define CONFIG_I2C2_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_I2C2_RX_BUFFER_SIZE
#define CONFIG_I2C2_RX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_I2C2_SPEED
#define CONFIG_I2C2_SPEED 100000
#endif
#ifndef CONFIG_I2C2_SCL_PIN
#define CONFIG_I2C2_SCL_PIN 25
#endif
#ifndef CONFIG_I2C2_SDA_PIN
#define CONFIG_I2C2_SDA_PIN 24
#endif

#ifndef CONFIG_I2C3_TX_BUFFER_SIZE
#define CONFIG_I2C3_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_I2C3_RX_BUFFER_SIZE
#define CONFIG_I2C3_RX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_I2C3_SPEED
#define CONFIG_I2C3_SPEED 100000
#endif
#ifndef CONFIG_I2C3_SCL_PIN
#define CONFIG_I2C3_SCL_PIN 35
#endif
#ifndef CONFIG_I2C3_SDA_PIN
#define CONFIG_I2C3_SDA_PIN 34
#endif

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_i2c_state_t g_i2c_states[NX_I2C_MAX_INSTANCES];
static nx_i2c_impl_t g_i2c_instances[NX_I2C_MAX_INSTANCES];

/* Dynamic buffer allocation based on Kconfig */
static uint8_t g_i2c0_tx_buffer[CONFIG_I2C0_TX_BUFFER_SIZE];
static uint8_t g_i2c0_rx_buffer[CONFIG_I2C0_RX_BUFFER_SIZE];
static uint8_t g_i2c1_tx_buffer[CONFIG_I2C1_TX_BUFFER_SIZE];
static uint8_t g_i2c1_rx_buffer[CONFIG_I2C1_RX_BUFFER_SIZE];
static uint8_t g_i2c2_tx_buffer[CONFIG_I2C2_TX_BUFFER_SIZE];
static uint8_t g_i2c2_rx_buffer[CONFIG_I2C2_RX_BUFFER_SIZE];
static uint8_t g_i2c3_tx_buffer[CONFIG_I2C3_TX_BUFFER_SIZE];
static uint8_t g_i2c3_rx_buffer[CONFIG_I2C3_RX_BUFFER_SIZE];

/* Buffer pointer table */
static uint8_t* g_i2c_tx_buffers[NX_I2C_MAX_INSTANCES] = {
    g_i2c0_tx_buffer,
    g_i2c1_tx_buffer,
    g_i2c2_tx_buffer,
    g_i2c3_tx_buffer,
};

static uint8_t* g_i2c_rx_buffers[NX_I2C_MAX_INSTANCES] = {
    g_i2c0_rx_buffer,
    g_i2c1_rx_buffer,
    g_i2c2_rx_buffer,
    g_i2c3_rx_buffer,
};

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

    /* Link to state */
    impl->state = &g_i2c_states[index];
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

    if (config == NULL || config->i2c_index >= NX_I2C_MAX_INSTANCES) {
        return NULL;
    }

    nx_i2c_impl_t* impl = &g_i2c_instances[config->i2c_index];

    /* Initialize instance with platform configuration */
    i2c_init_instance(impl, config->i2c_index, config);

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
#define NX_I2C_CONFIG(index)                                                   \
    static const nx_i2c_platform_config_t i2c_config_##index = {               \
        .i2c_index = index,                                                    \
        .speed = CONFIG_I2C##index##_SPEED,                                    \
        .scl_pin = CONFIG_I2C##index##_SCL_PIN,                                \
        .sda_pin = CONFIG_I2C##index##_SDA_PIN,                                \
        .tx_buf_size = CONFIG_I2C##index##_TX_BUFFER_SIZE,                     \
        .rx_buf_size = CONFIG_I2C##index##_RX_BUFFER_SIZE,                     \
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
                       &i2c_kconfig_state_##index, nx_i2c_device_init)

/* Register all enabled I2C instances */
NX_TRAVERSE_EACH_INSTANCE(NX_I2C_DEVICE_REGISTER, DEVICE_TYPE);

/*---------------------------------------------------------------------------*/
/* Legacy Factory Functions (for backward compatibility)                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get I2C instance (legacy)
 */
nx_i2c_bus_t* nx_i2c_native_get(uint8_t index) {
    if (index >= NX_I2C_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "I2C%d", index);
    return (nx_i2c_bus_t*)nx_device_get(name);
}

/**
 * \brief           Reset all I2C instances (for testing)
 */
void nx_i2c_native_reset_all(void) {
    for (uint8_t i = 0; i < NX_I2C_MAX_INSTANCES; i++) {
        nx_i2c_impl_t* impl = &g_i2c_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_i2c_states[i], 0, sizeof(nx_i2c_state_t));
    }
}

/**
 * \brief           Inject data into RX buffer (for testing)
 */
nx_status_t nx_i2c_native_inject_rx(uint8_t index, const uint8_t* data,
                                    size_t len) {
    if (index >= NX_I2C_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_i2c_impl_t* impl = &g_i2c_instances[index];
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    size_t written = i2c_buffer_write(&impl->state->rx_buf, data, len);
    return (written == len) ? NX_OK : NX_ERR_FULL;
}

/**
 * \brief           Get I2C device descriptor (for testing)
 */
nx_device_t* nx_i2c_native_get_device(uint8_t index) {
    if (index >= NX_I2C_MAX_INSTANCES) {
        return NULL;
    }
    return g_i2c_instances[index].device;
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get TX buffer data (for testing)
 */
nx_status_t nx_i2c_native_get_tx_data(uint8_t index, uint8_t* data,
                                      size_t max_len, size_t* actual_len) {
    if (index >= NX_I2C_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }
    if (!data || !actual_len) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_impl_t* impl = &g_i2c_instances[index];
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    *actual_len = i2c_buffer_read(&impl->state->tx_buf, data, max_len);
    return NX_OK;
}

/**
 * \brief           Get I2C state (for testing)
 */
nx_status_t nx_i2c_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended, bool* busy) {
    if (index >= NX_I2C_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_i2c_impl_t* impl = &g_i2c_instances[index];
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (initialized) {
        *initialized = impl->state->initialized;
    }
    if (suspended) {
        *suspended = impl->state->suspended;
    }
    if (busy) {
        *busy = impl->state->busy;
    }

    return NX_OK;
}

/**
 * \brief           Reset I2C instance (for testing)
 */
nx_status_t nx_i2c_native_reset(uint8_t index) {
    if (index >= NX_I2C_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_i2c_impl_t* impl = &g_i2c_instances[index];
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Clear buffers */
    i2c_buffer_clear(&impl->state->tx_buf);
    i2c_buffer_clear(&impl->state->rx_buf);

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_i2c_stats_t));

    /* Clear device handle */
    memset(&impl->state->current_device, 0, sizeof(nx_i2c_device_handle_t));

    /* Reset state flags */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->busy = false;

    return NX_OK;
}
