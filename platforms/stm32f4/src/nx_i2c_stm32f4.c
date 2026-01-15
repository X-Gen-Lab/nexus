/**
 * \file            nx_i2c_stm32f4.c
 * \brief           STM32F4 I2C driver implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_i2c.h"
#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"
#include <stddef.h>
#include <string.h>

/* Maximum number of I2C instances */
#define NX_I2C_MAX_INSTANCES 3 /* I2C1-3 */

/**
 * \brief           I2C instance state structure (internal)
 */
typedef struct {
    uint8_t index;               /**< I2C index (0-2) */
    nx_i2c_config_t config;      /**< Current configuration */
    nx_i2c_stats_t stats;        /**< Statistics */
    nx_dma_channel_t* dma_tx;    /**< DMA TX channel */
    nx_dma_channel_t* dma_rx;    /**< DMA RX channel */
    nx_isr_handle_t* isr_handle; /**< ISR manager handle */
    bool initialized;            /**< Initialization flag */
    bool suspended;              /**< Suspended flag */
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

/* ========== Hardware-Specific Functions ========== */

/**
 * \brief           Hardware-specific: Configure I2C
 */
static void hw_i2c_configure(uint8_t index, const nx_i2c_config_t* cfg) {
    /* TODO: Implement actual STM32F4 I2C configuration */
    /* This would configure I2C_CR1, I2C_CR2, I2C_CCR, I2C_TRISE registers */
    (void)index;
    (void)cfg;
}

/**
 * \brief           Hardware-specific: Set speed
 */
static void hw_i2c_set_speed(uint8_t index, nx_i2c_speed_t speed) {
    /* TODO: Implement actual STM32F4 I2C speed configuration */
    /* This would configure I2C_CCR and I2C_TRISE registers */
    (void)index;
    (void)speed;
}

/**
 * \brief           Hardware-specific: Enable I2C
 */
static void hw_i2c_enable(uint8_t index) {
    /* TODO: Implement actual STM32F4 I2C enable */
    /* This would set I2C_CR1 PE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Disable I2C
 */
static void hw_i2c_disable(uint8_t index) {
    /* TODO: Implement actual STM32F4 I2C disable */
    /* This would clear I2C_CR1 PE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Enable I2C clock
 */
static void hw_i2c_enable_clock(uint8_t index) {
    /* TODO: Implement actual STM32F4 I2C clock enable */
    /* This would enable RCC clock for the I2C */
    (void)index;
}

/**
 * \brief           Hardware-specific: Disable I2C clock
 */
static void hw_i2c_disable_clock(uint8_t index) {
    /* TODO: Implement actual STM32F4 I2C clock disable */
    /* This would disable RCC clock for the I2C */
    (void)index;
}

/**
 * \brief           Hardware-specific: Generate START condition
 */
static nx_status_t hw_i2c_start(uint8_t index, uint32_t timeout_ms) {
    /* TODO: Implement actual STM32F4 I2C START generation */
    /* This would set I2C_CR1 START bit and wait for SB flag */
    (void)index;
    (void)timeout_ms;
    return NX_OK;
}

/**
 * \brief           Hardware-specific: Generate STOP condition
 */
static nx_status_t hw_i2c_stop(uint8_t index) {
    /* TODO: Implement actual STM32F4 I2C STOP generation */
    /* This would set I2C_CR1 STOP bit */
    (void)index;
    return NX_OK;
}

/**
 * \brief           Hardware-specific: Send address
 */
static nx_status_t hw_i2c_send_address(uint8_t index, uint16_t addr, bool read,
                                       uint32_t timeout_ms) {
    /* TODO: Implement actual STM32F4 I2C address transmission */
    /* This would write to I2C_DR and wait for ADDR flag */
    (void)index;
    (void)addr;
    (void)read;
    (void)timeout_ms;
    return NX_OK;
}

/**
 * \brief           Hardware-specific: Send byte
 */
static nx_status_t hw_i2c_send_byte(uint8_t index, uint8_t byte,
                                    uint32_t timeout_ms) {
    /* TODO: Implement actual STM32F4 I2C byte transmission */
    /* This would write to I2C_DR and wait for TXE flag */
    (void)index;
    (void)byte;
    (void)timeout_ms;
    return NX_OK;
}

/**
 * \brief           Hardware-specific: Receive byte
 */
static nx_status_t hw_i2c_receive_byte(uint8_t index, uint8_t* byte,
                                       uint32_t timeout_ms) {
    /* TODO: Implement actual STM32F4 I2C byte reception */
    /* This would wait for RXNE flag and read from I2C_DR */
    (void)index;
    (void)byte;
    (void)timeout_ms;
    return NX_OK;
}

/* ========== I2C Operations ========== */

/**
 * \brief           Master transmit
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

    state->stats.busy = true;

    /* Generate START condition */
    nx_status_t status = hw_i2c_start(state->index, timeout_ms);
    if (status != NX_OK) {
        state->stats.bus_error_count++;
        state->stats.busy = false;
        return status;
    }

    /* Send address with write bit */
    status = hw_i2c_send_address(state->index, addr, false, timeout_ms);
    if (status != NX_OK) {
        state->stats.nack_count++;
        hw_i2c_stop(state->index);
        state->stats.busy = false;
        return status;
    }

    /* Send data bytes */
    for (size_t i = 0; i < len; i++) {
        status = hw_i2c_send_byte(state->index, data[i], timeout_ms);
        if (status != NX_OK) {
            state->stats.bus_error_count++;
            hw_i2c_stop(state->index);
            state->stats.busy = false;
            return status;
        }
    }

    /* Generate STOP condition */
    hw_i2c_stop(state->index);

    state->stats.tx_count += (uint32_t)len;
    state->stats.busy = false;

    return NX_OK;
}

/**
 * \brief           Master receive
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

    state->stats.busy = true;

    /* Generate START condition */
    nx_status_t status = hw_i2c_start(state->index, timeout_ms);
    if (status != NX_OK) {
        state->stats.bus_error_count++;
        state->stats.busy = false;
        return status;
    }

    /* Send address with read bit */
    status = hw_i2c_send_address(state->index, addr, true, timeout_ms);
    if (status != NX_OK) {
        state->stats.nack_count++;
        hw_i2c_stop(state->index);
        state->stats.busy = false;
        return status;
    }

    /* Receive data bytes */
    for (size_t i = 0; i < len; i++) {
        status = hw_i2c_receive_byte(state->index, &data[i], timeout_ms);
        if (status != NX_OK) {
            state->stats.bus_error_count++;
            hw_i2c_stop(state->index);
            state->stats.busy = false;
            return status;
        }
    }

    /* Generate STOP condition */
    hw_i2c_stop(state->index);

    state->stats.rx_count += (uint32_t)len;
    state->stats.busy = false;

    return NX_OK;
}

/**
 * \brief           Memory write
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

    state->stats.busy = true;

    /* Generate START condition */
    nx_status_t status = hw_i2c_start(state->index, timeout_ms);
    if (status != NX_OK) {
        state->stats.bus_error_count++;
        state->stats.busy = false;
        return status;
    }

    /* Send device address with write bit */
    status = hw_i2c_send_address(state->index, addr, false, timeout_ms);
    if (status != NX_OK) {
        state->stats.nack_count++;
        hw_i2c_stop(state->index);
        state->stats.busy = false;
        return status;
    }

    /* Send memory address */
    if (mem_addr_size == 2) {
        status = hw_i2c_send_byte(state->index, (uint8_t)(mem_addr >> 8),
                                  timeout_ms);
        if (status != NX_OK) {
            state->stats.bus_error_count++;
            hw_i2c_stop(state->index);
            state->stats.busy = false;
            return status;
        }
    }
    status =
        hw_i2c_send_byte(state->index, (uint8_t)(mem_addr & 0xFF), timeout_ms);
    if (status != NX_OK) {
        state->stats.bus_error_count++;
        hw_i2c_stop(state->index);
        state->stats.busy = false;
        return status;
    }

    /* Send data bytes */
    for (size_t i = 0; i < len; i++) {
        status = hw_i2c_send_byte(state->index, data[i], timeout_ms);
        if (status != NX_OK) {
            state->stats.bus_error_count++;
            hw_i2c_stop(state->index);
            state->stats.busy = false;
            return status;
        }
    }

    /* Generate STOP condition */
    hw_i2c_stop(state->index);

    state->stats.tx_count += (uint32_t)len;
    state->stats.busy = false;

    return NX_OK;
}

/**
 * \brief           Memory read
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

    state->stats.busy = true;

    /* Generate START condition */
    nx_status_t status = hw_i2c_start(state->index, timeout_ms);
    if (status != NX_OK) {
        state->stats.bus_error_count++;
        state->stats.busy = false;
        return status;
    }

    /* Send device address with write bit */
    status = hw_i2c_send_address(state->index, addr, false, timeout_ms);
    if (status != NX_OK) {
        state->stats.nack_count++;
        hw_i2c_stop(state->index);
        state->stats.busy = false;
        return status;
    }

    /* Send memory address */
    if (mem_addr_size == 2) {
        status = hw_i2c_send_byte(state->index, (uint8_t)(mem_addr >> 8),
                                  timeout_ms);
        if (status != NX_OK) {
            state->stats.bus_error_count++;
            hw_i2c_stop(state->index);
            state->stats.busy = false;
            return status;
        }
    }
    status =
        hw_i2c_send_byte(state->index, (uint8_t)(mem_addr & 0xFF), timeout_ms);
    if (status != NX_OK) {
        state->stats.bus_error_count++;
        hw_i2c_stop(state->index);
        state->stats.busy = false;
        return status;
    }

    /* Generate repeated START condition */
    status = hw_i2c_start(state->index, timeout_ms);
    if (status != NX_OK) {
        state->stats.bus_error_count++;
        hw_i2c_stop(state->index);
        state->stats.busy = false;
        return status;
    }

    /* Send device address with read bit */
    status = hw_i2c_send_address(state->index, addr, true, timeout_ms);
    if (status != NX_OK) {
        state->stats.nack_count++;
        hw_i2c_stop(state->index);
        state->stats.busy = false;
        return status;
    }

    /* Receive data bytes */
    for (size_t i = 0; i < len; i++) {
        status = hw_i2c_receive_byte(state->index, &data[i], timeout_ms);
        if (status != NX_OK) {
            state->stats.bus_error_count++;
            hw_i2c_stop(state->index);
            state->stats.busy = false;
            return status;
        }
    }

    /* Generate STOP condition */
    hw_i2c_stop(state->index);

    state->stats.rx_count += (uint32_t)len;
    state->stats.busy = false;

    return NX_OK;
}

/**
 * \brief           Probe device
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

    /* Generate START condition */
    nx_status_t status = hw_i2c_start(state->index, timeout_ms);
    if (status != NX_OK) {
        return status;
    }

    /* Send address with write bit */
    status = hw_i2c_send_address(state->index, addr, false, timeout_ms);

    /* Generate STOP condition */
    hw_i2c_stop(state->index);

    return status;
}

/**
 * \brief           Scan bus for devices
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

    /* Scan 7-bit address range (0x08 to 0x77) */
    for (uint16_t addr = 0x08; addr <= 0x77 && *found < max; addr++) {
        nx_status_t status = i2c_probe(self, addr, 10); /* 10ms timeout */
        if (status == NX_OK) {
            addr_list[*found] = (uint8_t)addr;
            (*found)++;
        }
    }

    return NX_OK;
}

/**
 * \brief           Set speed
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

    hw_i2c_set_speed(state->index, speed);
    state->config.speed = speed;
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

    hw_i2c_configure(state->index, cfg);
    memcpy(&state->config, cfg, sizeof(nx_i2c_config_t));
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
 * \brief           Initialize I2C
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

    /* Enable clock */
    hw_i2c_enable_clock(state->index);

    /* Configure I2C */
    hw_i2c_configure(state->index, &state->config);

    /* Enable I2C */
    hw_i2c_enable(state->index);

    /* Initialize statistics */
    memset(&state->stats, 0, sizeof(nx_i2c_stats_t));

    state->initialized = true;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize I2C
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

    /* Disable I2C */
    hw_i2c_disable(state->index);

    /* Disable clock */
    hw_i2c_disable_clock(state->index);

    /* Release DMA channels if allocated */
    if (state->dma_tx) {
        nx_dma_manager_t* dma_mgr = nx_dma_manager_get();
        if (dma_mgr) {
            dma_mgr->free(dma_mgr, state->dma_tx);
            state->dma_tx = NULL;
        }
    }

    if (state->dma_rx) {
        nx_dma_manager_t* dma_mgr = nx_dma_manager_get();
        if (dma_mgr) {
            dma_mgr->free(dma_mgr, state->dma_rx);
            state->dma_rx = NULL;
        }
    }

    state->initialized = false;

    return NX_OK;
}

/**
 * \brief           Suspend I2C
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

    /* Disable I2C */
    hw_i2c_disable(state->index);

    /* Disable clock */
    hw_i2c_disable_clock(state->index);

    state->suspended = true;
    return NX_OK;
}

/**
 * \brief           Resume I2C
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

    /* Enable clock */
    hw_i2c_enable_clock(state->index);

    /* Reconfigure I2C */
    hw_i2c_configure(state->index, &state->config);

    /* Enable I2C */
    hw_i2c_enable(state->index);

    state->suspended = false;
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
 * \brief           Enable I2C power
 */
static nx_status_t i2c_power_enable(nx_power_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    hw_i2c_enable_clock(state->index);
    return NX_OK;
}

/**
 * \brief           Disable I2C power
 */
static nx_status_t i2c_power_disable(nx_power_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    hw_i2c_disable_clock(state->index);
    return NX_OK;
}

/**
 * \brief           Check if I2C power is enabled
 */
static bool i2c_power_is_enabled(nx_power_t* self) {
    nx_i2c_impl_t* impl = i2c_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return false;
    }

    nx_i2c_state_t* state = impl->state;
    return state->initialized && !state->suspended;
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
nx_device_t* nx_i2c_stm32f4_get_device(uint8_t index) {
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
