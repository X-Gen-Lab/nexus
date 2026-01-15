/**
 * \file            nx_spi_stm32f4.c
 * \brief           STM32F4 SPI driver implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_spi.h"
#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"
#include <stddef.h>
#include <string.h>

/* Maximum number of SPI instances */
#define NX_SPI_MAX_INSTANCES 3 /* SPI1-3 */

/**
 * \brief           SPI instance state structure (internal)
 */
typedef struct {
    uint8_t index;               /**< SPI index (0-2) */
    nx_spi_config_t config;      /**< Current configuration */
    nx_spi_stats_t stats;        /**< Statistics */
    nx_dma_channel_t* dma_tx;    /**< DMA TX channel */
    nx_dma_channel_t* dma_rx;    /**< DMA RX channel */
    nx_isr_handle_t* isr_handle; /**< ISR manager handle */
    bool initialized;            /**< Initialization flag */
    bool suspended;              /**< Suspended flag */
    bool locked;                 /**< Bus lock flag */
    bool cs_active;              /**< CS active flag */
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

/* ========== Hardware-Specific Functions ========== */

/**
 * \brief           Hardware-specific: Configure SPI
 */
static void hw_spi_configure(uint8_t index, const nx_spi_config_t* cfg) {
    /* TODO: Implement actual STM32F4 SPI configuration */
    /* This would configure SPI_CR1, SPI_CR2 registers */
    (void)index;
    (void)cfg;
}

/**
 * \brief           Hardware-specific: Set clock
 */
static void hw_spi_set_clock(uint8_t index, uint32_t clock_hz) {
    /* TODO: Implement actual STM32F4 SPI clock configuration */
    /* This would configure SPI_CR1 BR bits */
    (void)index;
    (void)clock_hz;
}

/**
 * \brief           Hardware-specific: Set mode
 */
static void hw_spi_set_mode(uint8_t index, nx_spi_mode_t mode) {
    /* TODO: Implement actual STM32F4 SPI mode configuration */
    /* This would configure SPI_CR1 CPOL and CPHA bits */
    (void)index;
    (void)mode;
}

/**
 * \brief           Hardware-specific: Enable SPI
 */
static void hw_spi_enable(uint8_t index) {
    /* TODO: Implement actual STM32F4 SPI enable */
    /* This would set SPI_CR1 SPE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Disable SPI
 */
static void hw_spi_disable(uint8_t index) {
    /* TODO: Implement actual STM32F4 SPI disable */
    /* This would clear SPI_CR1 SPE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Enable SPI clock
 */
static void hw_spi_enable_clock(uint8_t index) {
    /* TODO: Implement actual STM32F4 SPI clock enable */
    /* This would enable RCC clock for the SPI */
    (void)index;
}

/**
 * \brief           Hardware-specific: Disable SPI clock
 */
static void hw_spi_disable_clock(uint8_t index) {
    /* TODO: Implement actual STM32F4 SPI clock disable */
    /* This would disable RCC clock for the SPI */
    (void)index;
}

/**
 * \brief           Hardware-specific: Transfer byte (polling)
 */
static nx_status_t hw_spi_transfer_byte(uint8_t index, uint8_t tx_byte,
                                        uint8_t* rx_byte, uint32_t timeout_ms) {
    /* TODO: Implement actual STM32F4 SPI transfer byte */
    /* This would wait for TXE, write to SPI_DR, wait for RXNE, read from SPI_DR
     */
    (void)index;
    (void)tx_byte;
    (void)rx_byte;
    (void)timeout_ms;
    return NX_OK;
}

/* ========== SPI Operations ========== */

/**
 * \brief           Transfer data (full duplex)
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

    state->stats.busy = true;

    /* Perform transfer byte by byte */
    for (size_t i = 0; i < len; i++) {
        uint8_t tx_byte = tx ? tx[i] : 0xFF;
        uint8_t rx_byte = 0;
        nx_status_t status =
            hw_spi_transfer_byte(state->index, tx_byte, &rx_byte, timeout_ms);
        if (status != NX_OK) {
            state->stats.error_count++;
            state->stats.busy = false;
            return status;
        }
        if (rx) {
            rx[i] = rx_byte;
        }
    }

    state->stats.tx_count += (uint32_t)len;
    state->stats.rx_count += (uint32_t)len;
    state->stats.busy = false;

    return NX_OK;
}

/**
 * \brief           Transmit data (TX only)
 */
static nx_status_t spi_transmit(nx_spi_t* self, const uint8_t* tx, size_t len,
                                uint32_t timeout_ms) {
    return spi_transfer(self, tx, NULL, len, timeout_ms);
}

/**
 * \brief           Receive data (RX only)
 */
static nx_status_t spi_receive(nx_spi_t* self, uint8_t* rx, size_t len,
                               uint32_t timeout_ms) {
    return spi_transfer(self, NULL, rx, len, timeout_ms);
}

/**
 * \brief           Select chip select
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

    /* TODO: Implement actual CS control (GPIO) */
    state->cs_active = true;
    return NX_OK;
}

/**
 * \brief           Deselect chip select
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

    /* TODO: Implement actual CS control (GPIO) */
    state->cs_active = false;
    return NX_OK;
}

/**
 * \brief           Lock SPI bus
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

    /* TODO: Implement actual mutex/semaphore locking with timeout */
    (void)timeout_ms;
    state->locked = true;
    return NX_OK;
}

/**
 * \brief           Unlock SPI bus
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
 * \brief           Set clock frequency
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

    hw_spi_set_clock(state->index, clock_hz);
    state->config.clock_hz = clock_hz;
    return NX_OK;
}

/**
 * \brief           Set SPI mode
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

    hw_spi_set_mode(state->index, mode);
    state->config.mode = mode;
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

    hw_spi_configure(state->index, cfg);
    memcpy(&state->config, cfg, sizeof(nx_spi_config_t));
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
 * \brief           Initialize SPI
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

    /* Enable clock */
    hw_spi_enable_clock(state->index);

    /* Configure SPI */
    hw_spi_configure(state->index, &state->config);

    /* Enable SPI */
    hw_spi_enable(state->index);

    /* Initialize statistics */
    memset(&state->stats, 0, sizeof(nx_spi_stats_t));

    state->initialized = true;
    state->suspended = false;
    state->locked = false;
    state->cs_active = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize SPI
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

    /* Disable SPI */
    hw_spi_disable(state->index);

    /* Disable clock */
    hw_spi_disable_clock(state->index);

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
    state->locked = false;
    state->cs_active = false;

    return NX_OK;
}

/**
 * \brief           Suspend SPI
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

    /* Disable SPI */
    hw_spi_disable(state->index);

    /* Disable clock */
    hw_spi_disable_clock(state->index);

    state->suspended = true;
    return NX_OK;
}

/**
 * \brief           Resume SPI
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

    /* Enable clock */
    hw_spi_enable_clock(state->index);

    /* Reconfigure SPI */
    hw_spi_configure(state->index, &state->config);

    /* Enable SPI */
    hw_spi_enable(state->index);

    state->suspended = false;
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
 * \brief           Enable SPI power
 */
static nx_status_t spi_power_enable(nx_power_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    hw_spi_enable_clock(state->index);
    return NX_OK;
}

/**
 * \brief           Disable SPI power
 */
static nx_status_t spi_power_disable(nx_power_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    hw_spi_disable_clock(state->index);
    return NX_OK;
}

/**
 * \brief           Check if SPI power is enabled
 */
static bool spi_power_is_enabled(nx_power_t* self) {
    nx_spi_impl_t* impl = spi_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return false;
    }

    nx_spi_state_t* state = impl->state;
    return state->initialized && !state->suspended;
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
nx_device_t* nx_spi_stm32f4_get_device(uint8_t index) {
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
