/**
 * \file            nx_spi_native.c
 * \brief           Native platform SPI driver implementation (simulation)
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_spi.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Maximum number of SPI instances */
#define NX_SPI_MAX_INSTANCES 3

/**
 * \brief           SPI instance state structure (internal)
 */
typedef struct {
    uint8_t index;          /**< SPI index */
    nx_spi_config_t config; /**< Current configuration */
    nx_spi_stats_t stats;   /**< Statistics */
    bool initialized;       /**< Initialization flag */
    bool suspended;         /**< Suspended flag */
    bool locked;            /**< Bus lock flag */
    bool cs_active;         /**< CS active flag */
    bool power_enabled;     /**< Power enabled flag */
} nx_spi_state_t;

/**
 * \brief           SPI device implementation structure
 */
typedef struct {
    nx_spi_t base;              /**< Base SPI interface */
    nx_lifecycle_t lifecycle;   /**< Lifecycle interface */
    nx_power_t power;           /**< Power interface */
    nx_diagnostic_t diagnostic; /**< Diagnostic interface */
    nx_spi_state_t* state;      /**< SPI state pointer */
    nx_device_t* device;        /**< Device descriptor */
} nx_spi_impl_t;

/* Forward declarations - SPI operations */
static nx_status_t spi_transfer(nx_spi_t* self, const uint8_t* tx, uint8_t* rx,
                                size_t len, uint32_t timeout_ms);
static nx_status_t spi_transmit(nx_spi_t* self, const uint8_t* tx, size_t len,
                                uint32_t timeout_ms);
static nx_status_t spi_receive(nx_spi_t* self, uint8_t* rx, size_t len,
                               uint32_t timeout_ms);
static nx_status_t spi_cs_select(nx_spi_t* self);
static nx_status_t spi_cs_deselect(nx_spi_t* self);
static nx_status_t spi_lock(nx_spi_t* self, uint32_t timeout_ms);
static nx_status_t spi_unlock(nx_spi_t* self);
static nx_status_t spi_set_clock(nx_spi_t* self, uint32_t clock_hz);
static nx_status_t spi_set_mode(nx_spi_t* self, nx_spi_mode_t mode);
static nx_status_t spi_get_config(nx_spi_t* self, nx_spi_config_t* cfg);
static nx_status_t spi_set_config(nx_spi_t* self, const nx_spi_config_t* cfg);
static nx_lifecycle_t* spi_get_lifecycle(nx_spi_t* self);
static nx_power_t* spi_get_power(nx_spi_t* self);
static nx_diagnostic_t* spi_get_diagnostic(nx_spi_t* self);
static nx_status_t spi_get_stats(nx_spi_t* self, nx_spi_stats_t* stats);

/* Forward declarations - Lifecycle operations */
static nx_status_t spi_lifecycle_init(nx_lifecycle_t* self);
static nx_status_t spi_lifecycle_deinit(nx_lifecycle_t* self);
static nx_status_t spi_lifecycle_suspend(nx_lifecycle_t* self);
static nx_status_t spi_lifecycle_resume(nx_lifecycle_t* self);
static nx_device_state_t spi_lifecycle_get_state(nx_lifecycle_t* self);

/* Forward declarations - Power operations */
static nx_status_t spi_power_enable(nx_power_t* self);
static nx_status_t spi_power_disable(nx_power_t* self);
static bool spi_power_is_enabled(nx_power_t* self);

/* Forward declarations - Diagnostic operations */
static nx_status_t spi_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size);
static nx_status_t spi_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size);
static nx_status_t spi_diagnostic_clear_statistics(nx_diagnostic_t* self);

/* SPI state storage */
static nx_spi_state_t g_spi_states[NX_SPI_MAX_INSTANCES];

/* SPI implementation instances */
static nx_spi_impl_t g_spi_instances[NX_SPI_MAX_INSTANCES];

/**
 * \brief           Get SPI implementation from base interface
 */
static nx_spi_impl_t* spi_get_impl(nx_spi_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_spi_impl_t*)self;
}

/**
 * \brief           Get SPI implementation from lifecycle interface
 */
static nx_spi_impl_t* spi_get_impl_from_lifecycle(nx_lifecycle_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, lifecycle));
}

/**
 * \brief           Get SPI implementation from power interface
 */
static nx_spi_impl_t* spi_get_impl_from_power(nx_power_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, power));
}

/**
 * \brief           Get SPI implementation from diagnostic interface
 */
static nx_spi_impl_t* spi_get_impl_from_diagnostic(nx_diagnostic_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, diagnostic));
}

/* ========== SPI Operations (Simulated) ========== */

/**
 * \brief           Transfer data (full duplex) - simulated
 */
static nx_status_t spi_transfer(nx_spi_t* self, const uint8_t* tx, uint8_t* rx,
                                size_t len, uint32_t timeout_ms) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->locked) {
        return NX_ERR_LOCKED;
    }

    (void)timeout_ms;

    /* Simulate transfer - echo TX data to RX */
    state->stats.busy = true;

    for (size_t i = 0; i < len; i++) {
        if (rx) {
            rx[i] = tx ? tx[i] : 0xFF;
        }
    }

    state->stats.tx_count += (uint32_t)len;
    state->stats.rx_count += (uint32_t)len;
    state->stats.busy = false;

    /* Debug output */
    if (tx && len > 0) {
        printf("[SPI%d] TX: ", state->index);
        for (size_t i = 0; i < len && i < 16; i++) {
            printf("%02X ", tx[i]);
        }
        if (len > 16) {
            printf("... (%zu bytes)", len);
        }
        printf("\n");
    }

    return NX_OK;
}

/**
 * \brief           Transmit data (TX only) - simulated
 */
static nx_status_t spi_transmit(nx_spi_t* self, const uint8_t* tx, size_t len,
                                uint32_t timeout_ms) {
    return spi_transfer(self, tx, NULL, len, timeout_ms);
}

/**
 * \brief           Receive data (RX only) - simulated
 */
static nx_status_t spi_receive(nx_spi_t* self, uint8_t* rx, size_t len,
                               uint32_t timeout_ms) {
    return spi_transfer(self, NULL, rx, len, timeout_ms);
}

/**
 * \brief           Select chip select - simulated
 */
static nx_status_t spi_cs_select(nx_spi_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->cs_active = true;
    printf("[SPI%d] CS: SELECT\n", state->index);
    return NX_OK;
}

/**
 * \brief           Deselect chip select - simulated
 */
static nx_status_t spi_cs_deselect(nx_spi_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->cs_active = false;
    printf("[SPI%d] CS: DESELECT\n", state->index);
    return NX_OK;
}

/**
 * \brief           Lock SPI bus - simulated
 */
static nx_status_t spi_lock(nx_spi_t* self, uint32_t timeout_ms) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->locked) {
        return NX_ERR_LOCKED;
    }

    (void)timeout_ms;
    state->locked = true;
    return NX_OK;
}

/**
 * \brief           Unlock SPI bus - simulated
 */
static nx_status_t spi_unlock(nx_spi_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->locked) {
        return NX_ERR_INVALID_STATE;
    }

    state->locked = false;
    return NX_OK;
}

/**
 * \brief           Set clock frequency - simulated
 */
static nx_status_t spi_set_clock(nx_spi_t* self, uint32_t clock_hz) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->config.clock_hz = clock_hz;
    printf("[SPI%d] Clock set to %u Hz\n", state->index, clock_hz);
    return NX_OK;
}

/**
 * \brief           Set SPI mode - simulated
 */
static nx_status_t spi_set_mode(nx_spi_t* self, nx_spi_mode_t mode) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->config.mode = mode;
    printf("[SPI%d] Mode set to %d\n", state->index, mode);
    return NX_OK;
}

/**
 * \brief           Get configuration
 */
static nx_status_t spi_get_config(nx_spi_t* self, nx_spi_config_t* cfg) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(cfg, &state->config, sizeof(nx_spi_config_t));
    return NX_OK;
}

/**
 * \brief           Set configuration
 */
static nx_status_t spi_set_config(nx_spi_t* self, const nx_spi_config_t* cfg) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(&state->config, cfg, sizeof(nx_spi_config_t));
    printf("[SPI%d] Configuration updated\n", state->index);
    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* spi_get_lifecycle(nx_spi_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* spi_get_power(nx_spi_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Get diagnostic interface
 */
static nx_diagnostic_t* spi_get_diagnostic(nx_spi_t* self) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

/**
 * \brief           Get statistics
 */
static nx_status_t spi_get_stats(nx_spi_t* self, nx_spi_stats_t* stats) {
    nx_spi_impl_t* impl = spi_get_impl(self);
    if (!impl || !impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(stats, &state->stats, sizeof(nx_spi_stats_t));
    return NX_OK;
}

/* ========== Lifecycle Operations ========== */

/**
 * \brief           Initialize SPI - simulated
 */
static nx_status_t spi_lifecycle_init(nx_lifecycle_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Initialize statistics */
    memset(&state->stats, 0, sizeof(nx_spi_stats_t));

    state->initialized = true;
    state->suspended = false;
    state->locked = false;
    state->cs_active = false;
    state->power_enabled = true;

    printf("[SPI%d] Initialized\n", state->index);
    return NX_OK;
}

/**
 * \brief           Deinitialize SPI - simulated
 */
static nx_status_t spi_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->initialized = false;
    state->locked = false;
    state->cs_active = false;

    printf("[SPI%d] Deinitialized\n", state->index);
    return NX_OK;
}

/**
 * \brief           Suspend SPI - simulated
 */
static nx_status_t spi_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    state->suspended = true;
    printf("[SPI%d] Suspended\n", state->index);
    return NX_OK;
}

/**
 * \brief           Resume SPI - simulated
 */
static nx_status_t spi_lifecycle_resume(nx_lifecycle_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    state->suspended = false;
    printf("[SPI%d] Resumed\n", state->index);
    return NX_OK;
}

/**
 * \brief           Get SPI state
 */
static nx_device_state_t spi_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    if (state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }

    return NX_DEV_STATE_RUNNING;
}

/* ========== Power Operations ========== */

/**
 * \brief           Enable SPI power - simulated
 */
static nx_status_t spi_power_enable(nx_power_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    impl->state->power_enabled = true;
    printf("[SPI%d] Power enabled\n", impl->state->index);
    return NX_OK;
}

/**
 * \brief           Disable SPI power - simulated
 */
static nx_status_t spi_power_disable(nx_power_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    impl->state->power_enabled = false;
    printf("[SPI%d] Power disabled\n", impl->state->index);
    return NX_OK;
}

/**
 * \brief           Check if SPI power is enabled - simulated
 */
static bool spi_power_is_enabled(nx_power_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return false;
    }

    return impl->state->power_enabled;
}

/* ========== Diagnostic Operations ========== */

/**
 * \brief           Get SPI status
 */
static nx_status_t spi_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size) {
    nx_spi_impl_t* impl = spi_get_impl_from_diagnostic(self);
    if (!impl || !impl->state || !status) {
        return NX_ERR_NULL_PTR;
    }

    if (size < sizeof(nx_spi_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    nx_spi_state_t* state = impl->state;
    memcpy(status, &state->stats, sizeof(nx_spi_stats_t));
    return NX_OK;
}

/**
 * \brief           Get SPI statistics
 */
static nx_status_t spi_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size) {
    nx_spi_impl_t* impl = spi_get_impl_from_diagnostic(self);
    if (!impl || !impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }

    if (size < sizeof(nx_spi_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    nx_spi_state_t* state = impl->state;
    memcpy(stats, &state->stats, sizeof(nx_spi_stats_t));
    return NX_OK;
}

/**
 * \brief           Clear SPI statistics
 */
static nx_status_t spi_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_diagnostic(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    state->stats.tx_count = 0;
    state->stats.rx_count = 0;
    state->stats.error_count = 0;
    return NX_OK;
}

/* ========== Device Initialization ========== */

/**
 * \brief           Initialize SPI device instance
 */
static void* spi_device_init(const nx_device_t* dev) {
    if (!dev || !dev->runtime_config) {
        return NULL;
    }

    /* Get SPI index from device name (e.g., "spi0" -> 0) */
    const char* name = dev->name;
    uint8_t index = 0;
    if (name && name[0] == 's' && name[1] == 'p' && name[2] == 'i') {
        index = name[3] - '0';
    }

    if (index >= NX_SPI_MAX_INSTANCES) {
        return NULL;
    }

    nx_spi_impl_t* impl = &g_spi_instances[index];
    nx_spi_state_t* state = &g_spi_states[index];

    /* Initialize state */
    memset(state, 0, sizeof(nx_spi_state_t));
    state->index = index;

    /* Copy configuration */
    if (dev->runtime_config) {
        memcpy(&state->config, dev->runtime_config, sizeof(nx_spi_config_t));
    }

    /* Initialize implementation structure */
    memset(impl, 0, sizeof(nx_spi_impl_t));
    impl->state = state;
    impl->device = (nx_device_t*)dev;

    /* Setup base interface */
    impl->base.transfer = spi_transfer;
    impl->base.transmit = spi_transmit;
    impl->base.receive = spi_receive;
    impl->base.cs_select = spi_cs_select;
    impl->base.cs_deselect = spi_cs_deselect;
    impl->base.lock = spi_lock;
    impl->base.unlock = spi_unlock;
    impl->base.set_clock = spi_set_clock;
    impl->base.set_mode = spi_set_mode;
    impl->base.get_config = spi_get_config;
    impl->base.set_config = spi_set_config;
    impl->base.get_lifecycle = spi_get_lifecycle;
    impl->base.get_power = spi_get_power;
    impl->base.get_diagnostic = spi_get_diagnostic;
    impl->base.get_stats = spi_get_stats;

    /* Setup lifecycle interface */
    impl->lifecycle.init = spi_lifecycle_init;
    impl->lifecycle.deinit = spi_lifecycle_deinit;
    impl->lifecycle.suspend = spi_lifecycle_suspend;
    impl->lifecycle.resume = spi_lifecycle_resume;
    impl->lifecycle.get_state = spi_lifecycle_get_state;

    /* Setup power interface */
    impl->power.enable = spi_power_enable;
    impl->power.disable = spi_power_disable;
    impl->power.is_enabled = spi_power_is_enabled;

    /* Setup diagnostic interface */
    impl->diagnostic.get_status = spi_diagnostic_get_status;
    impl->diagnostic.get_statistics = spi_diagnostic_get_statistics;
    impl->diagnostic.clear_statistics = spi_diagnostic_clear_statistics;

    return &impl->base;
}

/**
 * \brief           Deinitialize SPI device instance
 */
static nx_status_t spi_device_deinit(const nx_device_t* dev) {
    (void)dev;
    /* Cleanup handled by lifecycle deinit */
    return NX_OK;
}

/**
 * \brief           Suspend SPI device instance
 */
static nx_status_t spi_device_suspend(const nx_device_t* dev) {
    (void)dev;
    /* Handled by lifecycle suspend */
    return NX_OK;
}

/**
 * \brief           Resume SPI device instance
 */
static nx_status_t spi_device_resume(const nx_device_t* dev) {
    (void)dev;
    /* Handled by lifecycle resume */
    return NX_OK;
}

/* ========== Device Descriptors ========== */

/* Default SPI configurations */
static const nx_spi_config_t g_spi_default_configs[NX_SPI_MAX_INSTANCES] = {
    {
        .clock_hz = 1000000, /* 1 MHz */
        .mode = NX_SPI_MODE_0,
        .bits = 8,
        .msb_first = true,
        .cs_delay_us = 0,
    },
    {
        .clock_hz = 1000000, /* 1 MHz */
        .mode = NX_SPI_MODE_0,
        .bits = 8,
        .msb_first = true,
        .cs_delay_us = 0,
    },
    {
        .clock_hz = 1000000, /* 1 MHz */
        .mode = NX_SPI_MODE_0,
        .bits = 8,
        .msb_first = true,
        .cs_delay_us = 0,
    },
};

/* Runtime configuration storage */
static nx_spi_config_t g_spi_runtime_configs[NX_SPI_MAX_INSTANCES];

/* Device descriptors */
static nx_device_t g_spi_devices[NX_SPI_MAX_INSTANCES] = {
    {
        .name = "spi0",
        .default_config = &g_spi_default_configs[0],
        .runtime_config = &g_spi_runtime_configs[0],
        .config_size = sizeof(nx_spi_config_t),
        .state = {0},
        .device_init = spi_device_init,
        .device_deinit = spi_device_deinit,
        .device_suspend = spi_device_suspend,
        .device_resume = spi_device_resume,
    },
    {
        .name = "spi1",
        .default_config = &g_spi_default_configs[1],
        .runtime_config = &g_spi_runtime_configs[1],
        .config_size = sizeof(nx_spi_config_t),
        .state = {0},
        .device_init = spi_device_init,
        .device_deinit = spi_device_deinit,
        .device_suspend = spi_device_suspend,
        .device_resume = spi_device_resume,
    },
    {
        .name = "spi2",
        .default_config = &g_spi_default_configs[2],
        .runtime_config = &g_spi_runtime_configs[2],
        .config_size = sizeof(nx_spi_config_t),
        .state = {0},
        .device_init = spi_device_init,
        .device_deinit = spi_device_deinit,
        .device_suspend = spi_device_suspend,
        .device_resume = spi_device_resume,
    },
};

/**
 * \brief           Get SPI device descriptor by index
 */
nx_device_t* nx_spi_native_get_device(uint8_t index) {
    if (index >= NX_SPI_MAX_INSTANCES) {
        return NULL;
    }

    /* Initialize runtime config from default on first access */
    if (g_spi_runtime_configs[index].clock_hz == 0) {
        memcpy(&g_spi_runtime_configs[index], &g_spi_default_configs[index],
               sizeof(nx_spi_config_t));
    }

    return &g_spi_devices[index];
}

/**
 * \brief           Get SPI interface by index (factory function for tests)
 */
nx_spi_t* nx_spi_native_get(uint8_t index) {
    nx_device_t* dev = nx_spi_native_get_device(index);
    if (!dev) {
        return NULL;
    }

    /* Initialize runtime config if needed */
    if (g_spi_runtime_configs[index].clock_hz == 0) {
        memcpy(&g_spi_runtime_configs[index], &g_spi_default_configs[index],
               sizeof(nx_spi_config_t));
    }

    /* Initialize device if not already done */
    if (!dev->state.initialized) {
        void* intf = dev->device_init(dev);
        if (intf) {
            dev->state.initialized = true;
            dev->state.ref_count = 1;
            return (nx_spi_t*)intf;
        }
        return NULL;
    }

    /* Increment reference count */
    dev->state.ref_count++;

    /* Get interface from existing device */
    return &g_spi_instances[index].base;
}
