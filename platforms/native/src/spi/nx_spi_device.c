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
#include "nexus_config.h"
#include "nx_spi_helpers.h"
#include "nx_spi_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_SPI_MAX_INSTANCES 4
#define DEVICE_TYPE          NX_SPI

/* Fallback definitions for Kconfig macros (if not generated yet) */
#ifndef CONFIG_SPI0_TX_BUFFER_SIZE
#define CONFIG_SPI0_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_SPI0_RX_BUFFER_SIZE
#define CONFIG_SPI0_RX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_SPI0_MAX_SPEED
#define CONFIG_SPI0_MAX_SPEED 1000000
#endif
#ifndef CONFIG_SPI0_MOSI_PIN
#define CONFIG_SPI0_MOSI_PIN 11
#endif
#ifndef CONFIG_SPI0_MISO_PIN
#define CONFIG_SPI0_MISO_PIN 12
#endif
#ifndef CONFIG_SPI0_SCK_PIN
#define CONFIG_SPI0_SCK_PIN 13
#endif

#ifndef CONFIG_SPI1_TX_BUFFER_SIZE
#define CONFIG_SPI1_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_SPI1_RX_BUFFER_SIZE
#define CONFIG_SPI1_RX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_SPI1_MAX_SPEED
#define CONFIG_SPI1_MAX_SPEED 1000000
#endif
#ifndef CONFIG_SPI1_MOSI_PIN
#define CONFIG_SPI1_MOSI_PIN 21
#endif
#ifndef CONFIG_SPI1_MISO_PIN
#define CONFIG_SPI1_MISO_PIN 22
#endif
#ifndef CONFIG_SPI1_SCK_PIN
#define CONFIG_SPI1_SCK_PIN 23
#endif

#ifndef CONFIG_SPI2_TX_BUFFER_SIZE
#define CONFIG_SPI2_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_SPI2_RX_BUFFER_SIZE
#define CONFIG_SPI2_RX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_SPI2_MAX_SPEED
#define CONFIG_SPI2_MAX_SPEED 1000000
#endif
#ifndef CONFIG_SPI2_MOSI_PIN
#define CONFIG_SPI2_MOSI_PIN 31
#endif
#ifndef CONFIG_SPI2_MISO_PIN
#define CONFIG_SPI2_MISO_PIN 32
#endif
#ifndef CONFIG_SPI2_SCK_PIN
#define CONFIG_SPI2_SCK_PIN 33
#endif

#ifndef CONFIG_SPI3_TX_BUFFER_SIZE
#define CONFIG_SPI3_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_SPI3_RX_BUFFER_SIZE
#define CONFIG_SPI3_RX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_SPI3_MAX_SPEED
#define CONFIG_SPI3_MAX_SPEED 1000000
#endif
#ifndef CONFIG_SPI3_MOSI_PIN
#define CONFIG_SPI3_MOSI_PIN 41
#endif
#ifndef CONFIG_SPI3_MISO_PIN
#define CONFIG_SPI3_MISO_PIN 42
#endif
#ifndef CONFIG_SPI3_SCK_PIN
#define CONFIG_SPI3_SCK_PIN 43
#endif

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_spi_state_t g_spi_states[NX_SPI_MAX_INSTANCES];
static nx_spi_impl_t g_spi_instances[NX_SPI_MAX_INSTANCES];

/* Dynamic buffer allocation based on Kconfig */
static uint8_t g_spi0_tx_buffer[CONFIG_SPI0_TX_BUFFER_SIZE];
static uint8_t g_spi0_rx_buffer[CONFIG_SPI0_RX_BUFFER_SIZE];
static uint8_t g_spi1_tx_buffer[CONFIG_SPI1_TX_BUFFER_SIZE];
static uint8_t g_spi1_rx_buffer[CONFIG_SPI1_RX_BUFFER_SIZE];
static uint8_t g_spi2_tx_buffer[CONFIG_SPI2_TX_BUFFER_SIZE];
static uint8_t g_spi2_rx_buffer[CONFIG_SPI2_RX_BUFFER_SIZE];
static uint8_t g_spi3_tx_buffer[CONFIG_SPI3_TX_BUFFER_SIZE];
static uint8_t g_spi3_rx_buffer[CONFIG_SPI3_RX_BUFFER_SIZE];

/* Buffer pointer table */
static uint8_t* g_spi_tx_buffers[NX_SPI_MAX_INSTANCES] = {
    g_spi0_tx_buffer,
    g_spi1_tx_buffer,
    g_spi2_tx_buffer,
    g_spi3_tx_buffer,
};

static uint8_t* g_spi_rx_buffers[NX_SPI_MAX_INSTANCES] = {
    g_spi0_rx_buffer,
    g_spi1_rx_buffer,
    g_spi2_rx_buffer,
    g_spi3_rx_buffer,
};

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

    /* Link to state */
    impl->state = &g_spi_states[index];
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

    if (config == NULL || config->spi_index >= NX_SPI_MAX_INSTANCES) {
        return NULL;
    }

    nx_spi_impl_t* impl = &g_spi_instances[config->spi_index];

    /* Initialize instance with platform configuration */
    spi_init_instance(impl, config->spi_index, config);

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
#define NX_SPI_CONFIG(index)                                                   \
    static const nx_spi_platform_config_t spi_config_##index = {               \
        .spi_index = index,                                                    \
        .max_speed = CONFIG_SPI##index##_MAX_SPEED,                            \
        .mosi_pin = CONFIG_SPI##index##_MOSI_PIN,                              \
        .miso_pin = CONFIG_SPI##index##_MISO_PIN,                              \
        .sck_pin = CONFIG_SPI##index##_SCK_PIN,                                \
        .tx_buf_size = CONFIG_SPI##index##_TX_BUFFER_SIZE,                     \
        .rx_buf_size = CONFIG_SPI##index##_RX_BUFFER_SIZE,                     \
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
                       &spi_kconfig_state_##index, nx_spi_device_init)

/* Register all enabled SPI instances */
NX_TRAVERSE_EACH_INSTANCE(NX_SPI_DEVICE_REGISTER, DEVICE_TYPE);

/*---------------------------------------------------------------------------*/
/* Legacy Factory Functions (for backward compatibility)                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SPI instance (legacy)
 */
nx_spi_bus_t* nx_spi_native_get(uint8_t index) {
    if (index >= NX_SPI_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "SPI%d", index);
    return (nx_spi_bus_t*)nx_device_get(name);
}

/**
 * \brief           Reset all SPI instances (for testing)
 */
void nx_spi_native_reset_all(void) {
    for (uint8_t i = 0; i < NX_SPI_MAX_INSTANCES; i++) {
        nx_spi_impl_t* impl = &g_spi_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_spi_states[i], 0, sizeof(nx_spi_state_t));
    }
}

/**
 * \brief           Inject data into RX buffer (for testing)
 */
nx_status_t nx_spi_native_inject_rx(uint8_t index, const uint8_t* data,
                                    size_t len) {
    if (index >= NX_SPI_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_spi_impl_t* impl = &g_spi_instances[index];
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    size_t written = spi_inject_rx_data(impl->state, data, len);
    return (written == len) ? NX_OK : NX_ERR_FULL;
}

/**
 * \brief           Get SPI device descriptor (for testing)
 */
nx_device_t* nx_spi_native_get_device(uint8_t index) {
    if (index >= NX_SPI_MAX_INSTANCES) {
        return NULL;
    }
    return g_spi_instances[index].device;
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get TX buffer data (for testing)
 */
nx_status_t nx_spi_native_get_tx_data(uint8_t index, uint8_t* data,
                                      size_t max_len, size_t* actual_len) {
    if (index >= NX_SPI_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }
    if (!data || !actual_len) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_impl_t* impl = &g_spi_instances[index];
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    *actual_len = spi_buffer_read(&impl->state->tx_buf, data, max_len);
    return NX_OK;
}

/**
 * \brief           Get SPI state (for testing)
 */
nx_status_t nx_spi_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended, bool* busy) {
    if (index >= NX_SPI_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_spi_impl_t* impl = &g_spi_instances[index];
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
 * \brief           Reset SPI instance (for testing)
 */
nx_status_t nx_spi_native_reset(uint8_t index) {
    if (index >= NX_SPI_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_spi_impl_t* impl = &g_spi_instances[index];
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Clear buffers */
    spi_buffer_clear(&impl->state->tx_buf);
    spi_buffer_clear(&impl->state->rx_buf);

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_spi_stats_t));

    /* Clear device handle */
    memset(&impl->state->current_device, 0, sizeof(nx_spi_device_handle_t));

    /* Reset state flags */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->busy = false;

    return NX_OK;
}
