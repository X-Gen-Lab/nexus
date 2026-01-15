/**
 * \file            nx_uart_native.c
 * \brief           Native platform UART driver implementation (simulation)
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_uart.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Maximum number of UART instances */
#define NX_UART_MAX_INSTANCES 4

/* Default buffer sizes */
#define NX_UART_DEFAULT_TX_BUF_SIZE 256
#define NX_UART_DEFAULT_RX_BUF_SIZE 256

/**
 * \brief           Circular buffer structure
 */
typedef struct {
    uint8_t* data;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
} nx_uart_buffer_t;

/**
 * \brief           UART instance state structure (internal)
 */
typedef struct {
    uint8_t index;              /**< UART index */
    nx_uart_config_t config;    /**< Current configuration */
    nx_uart_stats_t stats;      /**< Statistics */
    nx_uart_buffer_t tx_buf;    /**< TX buffer */
    nx_uart_buffer_t rx_buf;    /**< RX buffer */
    void (*rx_callback)(void*); /**< RX callback */
    void* rx_callback_ctx;      /**< RX callback context */
    bool initialized;           /**< Initialization flag */
    bool suspended;             /**< Suspended flag */
} nx_uart_state_t;

/**
 * \brief           UART device implementation structure
 */
typedef struct {
    nx_uart_t base;             /**< Base UART interface */
    nx_tx_async_t tx_async;     /**< TX async interface */
    nx_rx_async_t rx_async;     /**< RX async interface */
    nx_tx_sync_t tx_sync;       /**< TX sync interface */
    nx_rx_sync_t rx_sync;       /**< RX sync interface */
    nx_lifecycle_t lifecycle;   /**< Lifecycle interface */
    nx_power_t power;           /**< Power interface */
    nx_diagnostic_t diagnostic; /**< Diagnostic interface */
    nx_uart_state_t* state;     /**< UART state pointer */
    nx_device_t* device;        /**< Device descriptor */
} nx_uart_impl_t;
/* Forward declarations - UART operations */
static nx_tx_async_t* uart_get_tx_async(nx_uart_t* self);
static nx_rx_async_t* uart_get_rx_async(nx_uart_t* self);
static nx_tx_sync_t* uart_get_tx_sync(nx_uart_t* self);
static nx_rx_sync_t* uart_get_rx_sync(nx_uart_t* self);
static nx_status_t uart_set_baudrate(nx_uart_t* self, uint32_t baudrate);
static nx_status_t uart_get_config(nx_uart_t* self, nx_uart_config_t* cfg);
static nx_status_t uart_set_config(nx_uart_t* self,
                                   const nx_uart_config_t* cfg);
static nx_lifecycle_t* uart_get_lifecycle(nx_uart_t* self);
static nx_power_t* uart_get_power(nx_uart_t* self);
static nx_diagnostic_t* uart_get_diagnostic(nx_uart_t* self);
static nx_status_t uart_get_stats(nx_uart_t* self, nx_uart_stats_t* stats);
static nx_status_t uart_clear_errors(nx_uart_t* self);

/* Forward declarations - TX async operations */
static nx_status_t tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len);
static size_t tx_async_get_free_space(nx_tx_async_t* self);
static bool tx_async_is_busy(nx_tx_async_t* self);

/* Forward declarations - RX async operations */
static size_t rx_async_read(nx_rx_async_t* self, uint8_t* data, size_t max_len);
static size_t rx_async_available(nx_rx_async_t* self);
static nx_status_t rx_async_set_callback(nx_rx_async_t* self, void (*cb)(void*),
                                         void* ctx);

/* Forward declarations - TX sync operations */
static nx_status_t tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                size_t len, uint32_t timeout_ms);

/* Forward declarations - RX sync operations */
static nx_status_t rx_sync_receive(nx_rx_sync_t* self, uint8_t* data,
                                   size_t len, uint32_t timeout_ms);

/* Forward declarations - Lifecycle operations */
static nx_status_t uart_lifecycle_init(nx_lifecycle_t* self);
static nx_status_t uart_lifecycle_deinit(nx_lifecycle_t* self);
static nx_status_t uart_lifecycle_suspend(nx_lifecycle_t* self);
static nx_status_t uart_lifecycle_resume(nx_lifecycle_t* self);
static nx_device_state_t uart_lifecycle_get_state(nx_lifecycle_t* self);

/* Forward declarations - Power operations */
static nx_status_t uart_power_enable(nx_power_t* self);
static nx_status_t uart_power_disable(nx_power_t* self);
static bool uart_power_is_enabled(nx_power_t* self);

/* Forward declarations - Diagnostic operations */
static nx_status_t uart_diagnostic_get_status(nx_diagnostic_t* self,
                                              void* status, size_t size);
static nx_status_t uart_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                  void* stats, size_t size);
static nx_status_t uart_diagnostic_clear_statistics(nx_diagnostic_t* self);

/* UART state storage */
static nx_uart_state_t g_uart_states[NX_UART_MAX_INSTANCES];

/* UART implementation instances */
static nx_uart_impl_t g_uart_instances[NX_UART_MAX_INSTANCES];

/* TX/RX buffer storage */
static uint8_t g_uart_tx_buffers[NX_UART_MAX_INSTANCES]
                                [NX_UART_DEFAULT_TX_BUF_SIZE];
static uint8_t g_uart_rx_buffers[NX_UART_MAX_INSTANCES]
                                [NX_UART_DEFAULT_RX_BUF_SIZE];
/**
 * \brief           Get UART implementation from base interface
 */
static nx_uart_impl_t* uart_get_impl(nx_uart_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_uart_impl_t*)self;
}

/**
 * \brief           Get UART implementation from lifecycle interface
 */
static nx_uart_impl_t* uart_get_impl_from_lifecycle(nx_lifecycle_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_uart_impl_t*)((char*)self - offsetof(nx_uart_impl_t, lifecycle));
}

/**
 * \brief           Get UART implementation from power interface
 */
static nx_uart_impl_t* uart_get_impl_from_power(nx_power_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_uart_impl_t*)((char*)self - offsetof(nx_uart_impl_t, power));
}

/**
 * \brief           Get UART implementation from diagnostic interface
 */
static nx_uart_impl_t* uart_get_impl_from_diagnostic(nx_diagnostic_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_uart_impl_t*)((char*)self -
                             offsetof(nx_uart_impl_t, diagnostic));
}

/**
 * \brief           Get UART implementation from TX async interface
 */
static nx_uart_impl_t* uart_get_impl_from_tx_async(nx_tx_async_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_uart_impl_t*)((char*)self - offsetof(nx_uart_impl_t, tx_async));
}

/**
 * \brief           Get UART implementation from RX async interface
 */
static nx_uart_impl_t* uart_get_impl_from_rx_async(nx_rx_async_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_uart_impl_t*)((char*)self - offsetof(nx_uart_impl_t, rx_async));
}
/**
 * \brief           Get UART implementation from TX sync interface
 */
static nx_uart_impl_t* uart_get_impl_from_tx_sync(nx_tx_sync_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_uart_impl_t*)((char*)self - offsetof(nx_uart_impl_t, tx_sync));
}

/**
 * \brief           Get UART implementation from RX sync interface
 */
static nx_uart_impl_t* uart_get_impl_from_rx_sync(nx_rx_sync_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_uart_impl_t*)((char*)self - offsetof(nx_uart_impl_t, rx_sync));
}

/* ========== Circular Buffer Helper Functions ========== */

static void buffer_init(nx_uart_buffer_t* buf, uint8_t* data, size_t size) {
    buf->data = data;
    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

static size_t buffer_write(nx_uart_buffer_t* buf, const uint8_t* data,
                           size_t len) {
    size_t written = 0;
    while (written < len && buf->count < buf->size) {
        buf->data[buf->head] = data[written];
        buf->head = (buf->head + 1) % buf->size;
        buf->count++;
        written++;
    }
    return written;
}

static size_t buffer_read(nx_uart_buffer_t* buf, uint8_t* data, size_t len) {
    size_t read = 0;
    while (read < len && buf->count > 0) {
        data[read] = buf->data[buf->tail];
        buf->tail = (buf->tail + 1) % buf->size;
        buf->count--;
        read++;
    }
    return read;
}

static size_t buffer_get_free(const nx_uart_buffer_t* buf) {
    return buf->size - buf->count;
}

static size_t buffer_get_count(const nx_uart_buffer_t* buf) {
    return buf->count;
}
/* ========== TX/RX Operations (Simulated using stdio) ========== */

static nx_status_t tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len) {
    nx_uart_impl_t* impl = uart_get_impl_from_tx_async(self);
    if (!impl || !impl->state || !data || !impl->state->initialized) {
        return NX_ERR_NULL_PTR;
    }

    /* Simulate: write to stdout */
    fwrite(data, 1, len, stdout);
    fflush(stdout);

    impl->state->stats.tx_count += (uint32_t)len;
    return NX_OK;
}

static size_t tx_async_get_free_space(nx_tx_async_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_tx_async(self);
    if (!impl || !impl->state || !impl->state->initialized) {
        return 0;
    }
    return buffer_get_free(&impl->state->tx_buf);
}

static bool tx_async_is_busy(nx_tx_async_t* self) {
    (void)self;
    return false; /* Native simulation is never busy */
}

static size_t rx_async_read(nx_rx_async_t* self, uint8_t* data,
                            size_t max_len) {
    nx_uart_impl_t* impl = uart_get_impl_from_rx_async(self);
    if (!impl || !impl->state || !data || !impl->state->initialized) {
        return 0;
    }

    size_t read = buffer_read(&impl->state->rx_buf, data, max_len);
    impl->state->stats.rx_count += (uint32_t)read;
    return read;
}

static size_t rx_async_available(nx_rx_async_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_rx_async(self);
    if (!impl || !impl->state || !impl->state->initialized) {
        return 0;
    }
    return buffer_get_count(&impl->state->rx_buf);
}

static nx_status_t rx_async_set_callback(nx_rx_async_t* self, void (*cb)(void*),
                                         void* ctx) {
    nx_uart_impl_t* impl = uart_get_impl_from_rx_async(self);
    if (!impl || !impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->rx_callback = cb;
    impl->state->rx_callback_ctx = ctx;
    return NX_OK;
}
static nx_status_t tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                size_t len, uint32_t timeout_ms) {
    nx_uart_impl_t* impl = uart_get_impl_from_tx_sync(self);
    if (!impl || !impl->state || !data || !impl->state->initialized) {
        return NX_ERR_NULL_PTR;
    }

    (void)timeout_ms; /* Ignore timeout in simulation */

    /* Simulate: write to stdout */
    fwrite(data, 1, len, stdout);
    fflush(stdout);

    impl->state->stats.tx_count += (uint32_t)len;
    return NX_OK;
}

static nx_status_t rx_sync_receive(nx_rx_sync_t* self, uint8_t* data,
                                   size_t len, uint32_t timeout_ms) {
    nx_uart_impl_t* impl = uart_get_impl_from_rx_sync(self);
    if (!impl || !impl->state || !data || !impl->state->initialized) {
        return NX_ERR_NULL_PTR;
    }

    (void)timeout_ms; /* Ignore timeout in simulation */

    /* Simulate: read from stdin (non-blocking simulation) */
    size_t read = buffer_read(&impl->state->rx_buf, data, len);
    impl->state->stats.rx_count += (uint32_t)read;

    return (read == len) ? NX_OK : NX_ERR_TIMEOUT;
}

/* ========== UART Base Operations ========== */

static nx_tx_async_t* uart_get_tx_async(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->tx_async : NULL;
}

static nx_rx_async_t* uart_get_rx_async(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->rx_async : NULL;
}

static nx_tx_sync_t* uart_get_tx_sync(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->tx_sync : NULL;
}

static nx_rx_sync_t* uart_get_rx_sync(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->rx_sync : NULL;
}

static nx_status_t uart_set_baudrate(nx_uart_t* self, uint32_t baudrate) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    if (!impl || !impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->config.baudrate = baudrate;
    return NX_OK;
}
static nx_status_t uart_get_config(nx_uart_t* self, nx_uart_config_t* cfg) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    if (!impl || !impl->state || !cfg || !impl->state->initialized) {
        return NX_ERR_NULL_PTR;
    }

    memcpy(cfg, &impl->state->config, sizeof(nx_uart_config_t));
    return NX_OK;
}

static nx_status_t uart_set_config(nx_uart_t* self,
                                   const nx_uart_config_t* cfg) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    if (!impl || !impl->state || !cfg || !impl->state->initialized) {
        return NX_ERR_NULL_PTR;
    }

    memcpy(&impl->state->config, cfg, sizeof(nx_uart_config_t));
    return NX_OK;
}

static nx_lifecycle_t* uart_get_lifecycle(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

static nx_power_t* uart_get_power(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->power : NULL;
}

static nx_diagnostic_t* uart_get_diagnostic(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

static nx_status_t uart_get_stats(nx_uart_t* self, nx_uart_stats_t* stats) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    if (!impl || !impl->state || !stats || !impl->state->initialized) {
        return NX_ERR_NULL_PTR;
    }

    memcpy(stats, &impl->state->stats, sizeof(nx_uart_stats_t));
    return NX_OK;
}

static nx_status_t uart_clear_errors(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    if (!impl || !impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->stats.tx_errors = 0;
    impl->state->stats.rx_errors = 0;
    impl->state->stats.overrun_errors = 0;
    impl->state->stats.framing_errors = 0;
    return NX_OK;
}
/* ========== Lifecycle Operations ========== */

static nx_status_t uart_lifecycle_init(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Initialize buffers */
    buffer_init(&impl->state->tx_buf, g_uart_tx_buffers[impl->state->index],
                impl->state->config.tx_buf_size);
    buffer_init(&impl->state->rx_buf, g_uart_rx_buffers[impl->state->index],
                impl->state->config.rx_buf_size);

    impl->state->initialized = true;
    impl->state->suspended = false;

    return NX_OK;
}

static nx_status_t uart_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);
    if (!impl || !impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->initialized = false;
    return NX_OK;
}

static nx_status_t uart_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);
    if (!impl || !impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->suspended = true;
    return NX_OK;
}

static nx_status_t uart_lifecycle_resume(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);
    if (!impl || !impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->suspended = false;
    return NX_OK;
}

static nx_device_state_t uart_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_DEV_STATE_ERROR;
    }

    if (!impl->state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    if (impl->state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }

    return NX_DEV_STATE_RUNNING;
}
/* ========== Power Operations ========== */

static nx_status_t uart_power_enable(nx_power_t* self) {
    (void)self;
    return NX_OK; /* No-op in simulation */
}

static nx_status_t uart_power_disable(nx_power_t* self) {
    (void)self;
    return NX_OK; /* No-op in simulation */
}

static bool uart_power_is_enabled(nx_power_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return false;
    }
    return impl->state->initialized && !impl->state->suspended;
}

/* ========== Diagnostic Operations ========== */

static nx_status_t uart_diagnostic_get_status(nx_diagnostic_t* self,
                                              void* status, size_t size) {
    nx_uart_impl_t* impl = uart_get_impl_from_diagnostic(self);
    if (!impl || !impl->state || !status) {
        return NX_ERR_NULL_PTR;
    }

    if (size < sizeof(nx_uart_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    memcpy(status, &impl->state->stats, sizeof(nx_uart_stats_t));
    return NX_OK;
}

static nx_status_t uart_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                  void* stats, size_t size) {
    nx_uart_impl_t* impl = uart_get_impl_from_diagnostic(self);
    if (!impl || !impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }

    if (size < sizeof(nx_uart_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    memcpy(stats, &impl->state->stats, sizeof(nx_uart_stats_t));
    return NX_OK;
}

static nx_status_t uart_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_diagnostic(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    memset(&impl->state->stats, 0, sizeof(nx_uart_stats_t));
    return NX_OK;
}
/* ========== Instance Initialization ========== */

static void uart_init_instance(nx_uart_impl_t* impl, uint8_t index) {
    /* Initialize base interface */
    impl->base.get_tx_async = uart_get_tx_async;
    impl->base.get_rx_async = uart_get_rx_async;
    impl->base.get_tx_sync = uart_get_tx_sync;
    impl->base.get_rx_sync = uart_get_rx_sync;
    impl->base.set_baudrate = uart_set_baudrate;
    impl->base.get_config = uart_get_config;
    impl->base.set_config = uart_set_config;
    impl->base.get_lifecycle = uart_get_lifecycle;
    impl->base.get_power = uart_get_power;
    impl->base.get_diagnostic = uart_get_diagnostic;
    impl->base.get_stats = uart_get_stats;
    impl->base.clear_errors = uart_clear_errors;

    /* Initialize TX async interface */
    impl->tx_async.send = tx_async_send;
    impl->tx_async.get_free_space = tx_async_get_free_space;
    impl->tx_async.is_busy = tx_async_is_busy;

    /* Initialize RX async interface */
    impl->rx_async.read = rx_async_read;
    impl->rx_async.available = rx_async_available;
    impl->rx_async.set_callback = rx_async_set_callback;

    /* Initialize TX sync interface */
    impl->tx_sync.send = tx_sync_send;

    /* Initialize RX sync interface */
    impl->rx_sync.receive = rx_sync_receive;

    /* Initialize lifecycle interface */
    impl->lifecycle.init = uart_lifecycle_init;
    impl->lifecycle.deinit = uart_lifecycle_deinit;
    impl->lifecycle.suspend = uart_lifecycle_suspend;
    impl->lifecycle.resume = uart_lifecycle_resume;
    impl->lifecycle.get_state = uart_lifecycle_get_state;

    /* Initialize power interface */
    impl->power.enable = uart_power_enable;
    impl->power.disable = uart_power_disable;
    impl->power.is_enabled = uart_power_is_enabled;

    /* Initialize diagnostic interface */
    impl->diagnostic.get_status = uart_diagnostic_get_status;
    impl->diagnostic.get_statistics = uart_diagnostic_get_statistics;
    impl->diagnostic.clear_statistics = uart_diagnostic_clear_statistics;

    /* Link to state */
    impl->state = &g_uart_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->rx_callback = NULL;
    impl->state->rx_callback_ctx = NULL;

    /* Set default configuration */
    impl->state->config.baudrate = 115200;
    impl->state->config.word_length = 8;
    impl->state->config.stop_bits = 1;
    impl->state->config.parity = 0;
    impl->state->config.flow_control = 0;
    impl->state->config.dma_tx_enable = false;
    impl->state->config.dma_rx_enable = false;
    impl->state->config.tx_buf_size = NX_UART_DEFAULT_TX_BUF_SIZE;
    impl->state->config.rx_buf_size = NX_UART_DEFAULT_RX_BUF_SIZE;

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_uart_stats_t));
}
/* ========== Factory Functions ========== */

/**
 * \brief           Get UART instance (factory function)
 * \param[in]       index: UART index (0-3)
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t* nx_uart_native_get(uint8_t index) {
    if (index >= NX_UART_MAX_INSTANCES) {
        return NULL;
    }

    nx_uart_impl_t* impl = &g_uart_instances[index];

    /* Initialize instance if not already done */
    if (!impl->state) {
        uart_init_instance(impl, index);
    }

    return &impl->base;
}

/**
 * \brief           Get UART instance with configuration
 * \param[in]       index: UART index (0-3)
 * \param[in]       cfg: UART configuration
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t* nx_uart_native_get_with_config(uint8_t index,
                                          const nx_uart_config_t* cfg) {
    nx_uart_t* uart = nx_uart_native_get(index);

    if (!uart || !cfg) {
        return NULL;
    }

    /* Apply configuration */
    nx_uart_impl_t* impl = uart_get_impl(uart);
    if (impl && impl->state) {
        memcpy(&impl->state->config, cfg, sizeof(nx_uart_config_t));
    }

    return uart;
}

/**
 * \brief           Reset all UART instances (for testing)
 */
void nx_uart_native_reset_all(void) {
    for (uint8_t i = 0; i < NX_UART_MAX_INSTANCES; i++) {
        nx_uart_impl_t* impl = &g_uart_instances[i];
        if (impl->state && impl->state->initialized) {
            /* Deinitialize if initialized */
            nx_lifecycle_t* lifecycle = &impl->lifecycle;
            lifecycle->deinit(lifecycle);
        }

        /* Reset state */
        memset(&g_uart_states[i], 0, sizeof(nx_uart_state_t));

        /* Re-initialize instance structure */
        uart_init_instance(impl, i);
    }
}

/**
 * \brief           Get UART device descriptor
 * \param[in]       index: UART index
 * \return          Device descriptor pointer, NULL on failure
 */
nx_device_t* nx_uart_native_get_device(uint8_t index) {
    if (index >= NX_UART_MAX_INSTANCES) {
        return NULL;
    }

    nx_uart_impl_t* impl = &g_uart_instances[index];
    return impl->device;
}
