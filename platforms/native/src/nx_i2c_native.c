/**
 * \file            nx_i2c_native.c
 * \brief           Native platform I2C driver implementation (simulation)
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_i2c.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Maximum number of I2C instances */
#define NX_I2C_MAX_INSTANCES 3

/**
 * \brief           I2C instance state structure (internal)
 */
typedef struct {
    uint8_t index;          /**< I2C index */
    nx_i2c_config_t config; /**< Current configuration */
    nx_i2c_stats_t stats;   /**< Statistics */
    bool initialized;       /**< Initialization flag */
    bool suspended;         /**< Suspended flag */
    bool power_enabled;     /**< Power enabled flag */
} nx_i2c_state_t;

/**
 * \brief           I2C device implementation structure
 */
typedef struct {
    nx_i2c_t base;              /**< Base I2C interface */
    nx_lifecycle_t lifecycle;   /**< Lifecycle interface */
    nx_power_t power;           /**< Power interface */
    nx_diagnostic_t diagnostic; /**< Diagnostic interface */
    nx_i2c_state_t* state;      /**< I2C state pointer */
    nx_device_t* device;        /**< Device descriptor */
} nx_i2c_impl_t;

/* Forward declarations - I2C operations */
static nx_status_t i2c_master_transmit(nx_i2c_t* self, uint16_t addr,
                                       const uint8_t* data, size_t len,
                                       uint32_t timeout_ms);
static nx_status_t i2c_master_receive(nx_i2c_t* self, uint16_t addr,
                                      uint8_t* data, size_t len,
                                      uint32_t timeout_ms);
static nx_status_t i2c_mem_write(nx_i2c_t* self, uint16_t addr,
                                 uint16_t mem_addr, uint8_t mem_addr_size,
                                 const uint8_t* data, size_t len,
                                 uint32_t timeout_ms);
static nx_status_t i2c_mem_read(nx_i2c_t* self, uint16_t addr,
                                uint16_t mem_addr, uint8_t mem_addr_size,
                                uint8_t* data, size_t len, uint32_t timeout_ms);
static nx_status_t i2c_probe(nx_i2c_t* self, uint16_t addr,
                             uint32_t timeout_ms);
static nx_status_t i2c_scan(nx_i2c_t* self, uint8_t* addr_list, size_t max,
                            size_t* found);
static nx_status_t i2c_set_speed(nx_i2c_t* self, nx_i2c_speed_t speed);
static nx_status_t i2c_get_config(nx_i2c_t* self, nx_i2c_config_t* cfg);
static nx_status_t i2c_set_config(nx_i2c_t* self, const nx_i2c_config_t* cfg);
static nx_lifecycle_t* i2c_get_lifecycle(nx_i2c_t* self);
static nx_power_t* i2c_get_power(nx_i2c_t* self);
static nx_diagnostic_t* i2c_get_diagnostic(nx_i2c_t* self);
static nx_status_t i2c_get_stats(nx_i2c_t* self, nx_i2c_stats_t* stats);

/* Forward declarations - Lifecycle operations */
static nx_status_t i2c_lifecycle_init(nx_lifecycle_t* self);
static nx_status_t i2c_lifecycle_deinit(nx_lifecycle_t* self);
static nx_status_t i2c_lifecycle_suspend(nx_lifecycle_t* self);
static nx_status_t i2c_lifecycle_resume(nx_lifecycle_t* self);
static nx_device_state_t i2c_lifecycle_get_state(nx_lifecycle_t* self);

/* Forward declarations - Power operations */
static nx_status_t i2c_power_enable(nx_power_t* self);
static nx_status_t i2c_power_disable(nx_power_t* self);
static bool i2c_power_is_enabled(nx_power_t* self);

/* Forward declarations - Diagnostic operations */
static nx_status_t i2c_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size);
static nx_status_t i2c_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size);
static nx_status_t i2c_diagnostic_clear_statistics(nx_diagnostic_t* self);

/* I2C state storage */
static nx_i2c_state_t g_i2c_states[NX_I2C_MAX_INSTANCES];

/* I2C implementation instances */
static nx_i2c_impl_t g_i2c_instances[NX_I2C_MAX_INSTANCES];

/**
 * \brief           Get I2C implementation from base interface
 */
static nx_i2c_impl_t* i2c_get_impl(nx_i2c_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_i2c_impl_t*)self;
}

/**
 * \brief           Get I2C implementation from lifecycle interface
 */
static nx_i2c_impl_t* i2c_get_impl_from_lifecycle(nx_lifecycle_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, lifecycle));
}

/**
 * \brief           Get I2C implementation from power interface
 */
static nx_i2c_impl_t* i2c_get_impl_from_power(nx_power_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, power));
}

/**
 * \brief           Get I2C implementation from diagnostic interface
 */
static nx_i2c_impl_t* i2c_get_impl_from_diagnostic(nx_diagnostic_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, diagnostic));
}

/* ========== I2C Operations (Simulated) ========== */

/**
 * \brief           Master transmit - simulated
 */
static nx_status_t i2c_master_transmit(nx_i2c_t* self, uint16_t addr,
                                       const uint8_t* data, size_t len,
                                       uint32_t timeout_ms) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;

    /* Simulate transmission */
    state->stats.busy = true;
    state->stats.tx_count += (uint32_t)len;
    state->stats.busy = false;

    /* Debug output */
    printf("[I2C%d] TX to 0x%02X: ", state->index, addr);
    for (size_t i = 0; i < len && i < 16; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 16) {
        printf("... (%zu bytes)", len);
    }
    printf("\n");

    return NX_OK;
}

/**
 * \brief           Master receive - simulated
 */
static nx_status_t i2c_master_receive(nx_i2c_t* self, uint16_t addr,
                                      uint8_t* data, size_t len,
                                      uint32_t timeout_ms) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;

    /* Simulate reception - fill with dummy data */
    state->stats.busy = true;
    for (size_t i = 0; i < len; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }
    state->stats.rx_count += (uint32_t)len;
    state->stats.busy = false;

    printf("[I2C%d] RX from 0x%02X: %zu bytes\n", state->index, addr, len);

    return NX_OK;
}

/**
 * \brief           Memory write - simulated
 */
static nx_status_t i2c_mem_write(nx_i2c_t* self, uint16_t addr,
                                 uint16_t mem_addr, uint8_t mem_addr_size,
                                 const uint8_t* data, size_t len,
                                 uint32_t timeout_ms) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (mem_addr_size != 1 && mem_addr_size != 2) {
        return NX_ERR_INVALID_PARAM;
    }

    (void)timeout_ms;

    /* Simulate memory write */
    state->stats.busy = true;
    state->stats.tx_count += (uint32_t)len;
    state->stats.busy = false;

    printf("[I2C%d] MEM_WRITE to 0x%02X @ 0x%04X: %zu bytes\n", state->index,
           addr, mem_addr, len);

    return NX_OK;
}

/**
 * \brief           Memory read - simulated
 */
static nx_status_t i2c_mem_read(nx_i2c_t* self, uint16_t addr,
                                uint16_t mem_addr, uint8_t mem_addr_size,
                                uint8_t* data, size_t len,
                                uint32_t timeout_ms) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (mem_addr_size != 1 && mem_addr_size != 2) {
        return NX_ERR_INVALID_PARAM;
    }

    (void)timeout_ms;

    /* Simulate memory read - fill with dummy data */
    state->stats.busy = true;
    for (size_t i = 0; i < len; i++) {
        data[i] = (uint8_t)((mem_addr + i) & 0xFF);
    }
    state->stats.rx_count += (uint32_t)len;
    state->stats.busy = false;

    printf("[I2C%d] MEM_READ from 0x%02X @ 0x%04X: %zu bytes\n", state->index,
           addr, mem_addr, len);

    return NX_OK;
}

/**
 * \brief           Probe device - simulated
 */
static nx_status_t i2c_probe(nx_i2c_t* self, uint16_t addr,
                             uint32_t timeout_ms) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;

    /* Simulate probe - respond to a few addresses */
    if (addr == 0x50 || addr == 0x51 || addr == 0x68 || addr == 0x76) {
        printf("[I2C%d] PROBE 0x%02X: ACK\n", state->index, addr);
        return NX_OK;
    }

    return NX_ERR_TIMEOUT; /* NACK */
}

/**
 * \brief           Scan bus for devices - simulated
 */
static nx_status_t i2c_scan(nx_i2c_t* self, uint8_t* addr_list, size_t max,
                            size_t* found) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state || !addr_list || !found) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    *found = 0;

    /* Simulate scan - return a few simulated devices */
    uint8_t simulated_devices[] = {0x50, 0x51, 0x68, 0x76};
    size_t num_devices =
        sizeof(simulated_devices) / sizeof(simulated_devices[0]);

    for (size_t i = 0; i < num_devices && *found < max; i++) {
        addr_list[*found] = simulated_devices[i];
        (*found)++;
    }

    printf("[I2C%d] SCAN: Found %zu devices\n", state->index, *found);

    return NX_OK;
}

/**
 * \brief           Set speed - simulated
 */
static nx_status_t i2c_set_speed(nx_i2c_t* self, nx_i2c_speed_t speed) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->config.speed = speed;
    const char* speed_str = (speed == NX_I2C_SPEED_STANDARD) ? "100kHz"
                            : (speed == NX_I2C_SPEED_FAST)   ? "400kHz"
                                                             : "1MHz";
    printf("[I2C%d] Speed set to %s\n", state->index, speed_str);
    return NX_OK;
}

/**
 * \brief           Get configuration
 */
static nx_status_t i2c_get_config(nx_i2c_t* self, nx_i2c_config_t* cfg) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(cfg, &state->config, sizeof(nx_i2c_config_t));
    return NX_OK;
}

/**
 * \brief           Set configuration
 */
static nx_status_t i2c_set_config(nx_i2c_t* self, const nx_i2c_config_t* cfg) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(&state->config, cfg, sizeof(nx_i2c_config_t));
    printf("[I2C%d] Configuration updated\n", state->index);
    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* i2c_get_lifecycle(nx_i2c_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* i2c_get_power(nx_i2c_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Get diagnostic interface
 */
static nx_diagnostic_t* i2c_get_diagnostic(nx_i2c_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

/**
 * \brief           Get statistics
 */
static nx_status_t i2c_get_stats(nx_i2c_t* self, nx_i2c_stats_t* stats) {
    nx_i2c_impl_t* impl = i2c_get_impl(self);
    if (!impl || !impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(stats, &state->stats, sizeof(nx_i2c_stats_t));
    return NX_OK;
}

/* ========== Lifecycle Operations ========== */

/**
 * \brief           Initialize I2C - simulated
 */
static nx_status_t i2c_lifecycle_init(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Initialize statistics */
    memset(&state->stats, 0, sizeof(nx_i2c_stats_t));

    state->initialized = true;
    state->suspended = false;
    state->power_enabled = true;

    printf("[I2C%d] Initialized\n", state->index);
    return NX_OK;
}

/**
 * \brief           Deinitialize I2C - simulated
 */
static nx_status_t i2c_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->initialized = false;

    printf("[I2C%d] Deinitialized\n", state->index);
    return NX_OK;
}

/**
 * \brief           Suspend I2C - simulated
 */
static nx_status_t i2c_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    state->suspended = true;
    printf("[I2C%d] Suspended\n", state->index);
    return NX_OK;
}

/**
 * \brief           Resume I2C - simulated
 */
static nx_status_t i2c_lifecycle_resume(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    state->suspended = false;
    printf("[I2C%d] Resumed\n", state->index);
    return NX_OK;
}

/**
 * \brief           Get I2C state
 */
static nx_device_state_t i2c_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    nx_i2c_state_t* state = impl->state;
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
 * \brief           Enable I2C power - simulated
 */
static nx_status_t i2c_power_enable(nx_power_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    impl->state->power_enabled = true;
    printf("[I2C%d] Power enabled\n", impl->state->index);
    return NX_OK;
}

/**
 * \brief           Disable I2C power - simulated
 */
static nx_status_t i2c_power_disable(nx_power_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    impl->state->power_enabled = false;
    printf("[I2C%d] Power disabled\n", impl->state->index);
    return NX_OK;
}

/**
 * \brief           Check if I2C power is enabled - simulated
 */
static bool i2c_power_is_enabled(nx_power_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return false;
    }

    return impl->state->power_enabled;
}

/* ========== Diagnostic Operations ========== */

/**
 * \brief           Get I2C status
 */
static nx_status_t i2c_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_diagnostic(self);
    if (!impl || !impl->state || !status) {
        return NX_ERR_NULL_PTR;
    }

    if (size < sizeof(nx_i2c_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    nx_i2c_state_t* state = impl->state;
    memcpy(status, &state->stats, sizeof(nx_i2c_stats_t));
    return NX_OK;
}

/**
 * \brief           Get I2C statistics
 */
static nx_status_t i2c_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_diagnostic(self);
    if (!impl || !impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }

    if (size < sizeof(nx_i2c_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    nx_i2c_state_t* state = impl->state;
    memcpy(stats, &state->stats, sizeof(nx_i2c_stats_t));
    return NX_OK;
}

/**
 * \brief           Clear I2C statistics
 */
static nx_status_t i2c_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_diagnostic(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    state->stats.tx_count = 0;
    state->stats.rx_count = 0;
    state->stats.nack_count = 0;
    state->stats.bus_error_count = 0;
    return NX_OK;
}

/* ========== Device Initialization ========== */

/**
 * \brief           Initialize I2C device instance
 */
static void* i2c_device_init(const nx_device_t* dev) {
    if (!dev || !dev->runtime_config) {
        return NULL;
    }

    /* Get I2C index from device name (e.g., "i2c0" -> 0) */
    const char* name = dev->name;
    uint8_t index = 0;
    if (name && name[0] == 'i' && name[1] == '2' && name[2] == 'c') {
        index = name[3] - '0';
    }

    if (index >= NX_I2C_MAX_INSTANCES) {
        return NULL;
    }

    nx_i2c_impl_t* impl = &g_i2c_instances[index];
    nx_i2c_state_t* state = &g_i2c_states[index];

    /* Initialize state */
    memset(state, 0, sizeof(nx_i2c_state_t));
    state->index = index;

    /* Copy configuration */
    if (dev->runtime_config) {
        memcpy(&state->config, dev->runtime_config, sizeof(nx_i2c_config_t));
    }

    /* Initialize implementation structure */
    memset(impl, 0, sizeof(nx_i2c_impl_t));
    impl->state = state;
    impl->device = (nx_device_t*)dev;

    /* Setup base interface */
    impl->base.master_transmit = i2c_master_transmit;
    impl->base.master_receive = i2c_master_receive;
    impl->base.mem_write = i2c_mem_write;
    impl->base.mem_read = i2c_mem_read;
    impl->base.probe = i2c_probe;
    impl->base.scan = i2c_scan;
    impl->base.set_speed = i2c_set_speed;
    impl->base.get_config = i2c_get_config;
    impl->base.set_config = i2c_set_config;
    impl->base.get_lifecycle = i2c_get_lifecycle;
    impl->base.get_power = i2c_get_power;
    impl->base.get_diagnostic = i2c_get_diagnostic;
    impl->base.get_stats = i2c_get_stats;

    /* Setup lifecycle interface */
    impl->lifecycle.init = i2c_lifecycle_init;
    impl->lifecycle.deinit = i2c_lifecycle_deinit;
    impl->lifecycle.suspend = i2c_lifecycle_suspend;
    impl->lifecycle.resume = i2c_lifecycle_resume;
    impl->lifecycle.get_state = i2c_lifecycle_get_state;

    /* Setup power interface */
    impl->power.enable = i2c_power_enable;
    impl->power.disable = i2c_power_disable;
    impl->power.is_enabled = i2c_power_is_enabled;

    /* Setup diagnostic interface */
    impl->diagnostic.get_status = i2c_diagnostic_get_status;
    impl->diagnostic.get_statistics = i2c_diagnostic_get_statistics;
    impl->diagnostic.clear_statistics = i2c_diagnostic_clear_statistics;

    return &impl->base;
}

/**
 * \brief           Deinitialize I2C device instance
 */
static nx_status_t i2c_device_deinit(const nx_device_t* dev) {
    (void)dev;
    /* Cleanup handled by lifecycle deinit */
    return NX_OK;
}

/**
 * \brief           Suspend I2C device instance
 */
static nx_status_t i2c_device_suspend(const nx_device_t* dev) {
    (void)dev;
    /* Handled by lifecycle suspend */
    return NX_OK;
}

/**
 * \brief           Resume I2C device instance
 */
static nx_status_t i2c_device_resume(const nx_device_t* dev) {
    (void)dev;
    /* Handled by lifecycle resume */
    return NX_OK;
}

/* ========== Device Descriptors ========== */

/* Default I2C configurations */
static const nx_i2c_config_t g_i2c_default_configs[NX_I2C_MAX_INSTANCES] = {
    {
        .speed = NX_I2C_SPEED_STANDARD,
        .own_addr = 0x00,
        .addr_10bit = false,
    },
    {
        .speed = NX_I2C_SPEED_STANDARD,
        .own_addr = 0x00,
        .addr_10bit = false,
    },
    {
        .speed = NX_I2C_SPEED_STANDARD,
        .own_addr = 0x00,
        .addr_10bit = false,
    },
};

/* Runtime configuration storage */
static nx_i2c_config_t g_i2c_runtime_configs[NX_I2C_MAX_INSTANCES];

/* Device descriptors */
static nx_device_t g_i2c_devices[NX_I2C_MAX_INSTANCES] = {
    {
        .name = "i2c0",
        .default_config = &g_i2c_default_configs[0],
        .runtime_config = &g_i2c_runtime_configs[0],
        .config_size = sizeof(nx_i2c_config_t),
        .state = {0},
        .device_init = i2c_device_init,
        .device_deinit = i2c_device_deinit,
        .device_suspend = i2c_device_suspend,
        .device_resume = i2c_device_resume,
    },
    {
        .name = "i2c1",
        .default_config = &g_i2c_default_configs[1],
        .runtime_config = &g_i2c_runtime_configs[1],
        .config_size = sizeof(nx_i2c_config_t),
        .state = {0},
        .device_init = i2c_device_init,
        .device_deinit = i2c_device_deinit,
        .device_suspend = i2c_device_suspend,
        .device_resume = i2c_device_resume,
    },
    {
        .name = "i2c2",
        .default_config = &g_i2c_default_configs[2],
        .runtime_config = &g_i2c_runtime_configs[2],
        .config_size = sizeof(nx_i2c_config_t),
        .state = {0},
        .device_init = i2c_device_init,
        .device_deinit = i2c_device_deinit,
        .device_suspend = i2c_device_suspend,
        .device_resume = i2c_device_resume,
    },
};

/**
 * \brief           Get I2C device descriptor by index
 */
nx_device_t* nx_i2c_native_get_device(uint8_t index) {
    if (index >= NX_I2C_MAX_INSTANCES) {
        return NULL;
    }

    /* Initialize runtime config from default on first access */
    if (g_i2c_runtime_configs[index].speed == 0) {
        memcpy(&g_i2c_runtime_configs[index], &g_i2c_default_configs[index],
               sizeof(nx_i2c_config_t));
    }

    return &g_i2c_devices[index];
}

/**
 * \brief           Get I2C interface by index (factory function for tests)
 */
nx_i2c_t* nx_i2c_native_get(uint8_t index) {
    nx_device_t* dev = nx_i2c_native_get_device(index);
    if (!dev) {
        return NULL;
    }

    /* Initialize runtime config if needed */
    if (g_i2c_runtime_configs[index].speed == 0) {
        memcpy(&g_i2c_runtime_configs[index], &g_i2c_default_configs[index],
               sizeof(nx_i2c_config_t));
    }

    /* Initialize device if not already done */
    if (!dev->state.initialized) {
        void* intf = dev->device_init(dev);
        if (intf) {
            dev->state.initialized = true;
            dev->state.ref_count = 1;
            return (nx_i2c_t*)intf;
        }
        return NULL;
    }

    /* Increment reference count */
    dev->state.ref_count++;

    /* Get interface from existing device */
    return &g_i2c_instances[index].base;
}
