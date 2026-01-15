/**
 * \file            nx_uart_stm32f4.c
 * \brief           STM32F4 UART driver implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_uart.h"
#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"
#include <stddef.h>
#include <string.h>

/* Maximum number of UART instances */
#define NX_UART_MAX_INSTANCES 6 /* USART1-6 */

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
    uint8_t index;               /**< UART index (0-5) */
    nx_uart_config_t config;     /**< Current configuration */
    nx_uart_stats_t stats;       /**< Statistics */
    nx_uart_buffer_t tx_buf;     /**< TX buffer */
    nx_uart_buffer_t rx_buf;     /**< RX buffer */
    nx_dma_channel_t* dma_tx;    /**< DMA TX channel */
    nx_dma_channel_t* dma_rx;    /**< DMA RX channel */
    nx_isr_handle_t* isr_handle; /**< ISR manager handle */
    void (*rx_callback)(void*);  /**< RX callback */
    void* rx_callback_ctx;       /**< RX callback context */
    bool initialized;            /**< Initialization flag */
    bool suspended;              /**< Suspended flag */
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

/**
 * \brief           Initialize circular buffer
 */
static void buffer_init(nx_uart_buffer_t* buf, uint8_t* data, size_t size) {
    buf->data = data;
    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

/**
 * \brief           Write data to circular buffer
 */
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

/**
 * \brief           Read data from circular buffer
 */
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

/**
 * \brief           Get available space in buffer
 */
static size_t buffer_get_free(const nx_uart_buffer_t* buf) {
    return buf->size - buf->count;
}

/**
 * \brief           Get available data in buffer
 */
static size_t buffer_get_count(const nx_uart_buffer_t* buf) {
    return buf->count;
}
/* ========== Hardware-Specific Functions ========== */

/**
 * \brief           Hardware-specific: Configure UART
 */
static void hw_uart_configure(uint8_t index, const nx_uart_config_t* cfg) {
    /* TODO: Implement actual STM32F4 UART configuration */
    /* This would configure USART_CR1, USART_CR2, USART_CR3, USART_BRR */
    (void)index;
    (void)cfg;
}

/**
 * \brief           Hardware-specific: Set baudrate
 */
static void hw_uart_set_baudrate(uint8_t index, uint32_t baudrate) {
    /* TODO: Implement actual STM32F4 UART baudrate configuration */
    /* This would configure USART_BRR register */
    (void)index;
    (void)baudrate;
}

/**
 * \brief           Hardware-specific: Enable UART
 */
static void hw_uart_enable(uint8_t index) {
    /* TODO: Implement actual STM32F4 UART enable */
    /* This would set USART_CR1 UE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Disable UART
 */
static void hw_uart_disable(uint8_t index) {
    /* TODO: Implement actual STM32F4 UART disable */
    /* This would clear USART_CR1 UE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Enable UART clock
 */
static void hw_uart_enable_clock(uint8_t index) {
    /* TODO: Implement actual STM32F4 UART clock enable */
    /* This would enable RCC clock for the UART */
    (void)index;
}

/**
 * \brief           Hardware-specific: Disable UART clock
 */
static void hw_uart_disable_clock(uint8_t index) {
    /* TODO: Implement actual STM32F4 UART clock disable */
    /* This would disable RCC clock for the UART */
    (void)index;
}
/**
 * \brief           Hardware-specific: Send byte (polling)
 */
static nx_status_t hw_uart_send_byte(uint8_t index, uint8_t byte,
                                     uint32_t timeout_ms) {
    /* TODO: Implement actual STM32F4 UART send byte */
    /* This would wait for TXE flag and write to USART_DR */
    (void)index;
    (void)byte;
    (void)timeout_ms;
    return NX_OK;
}

/**
 * \brief           Hardware-specific: Receive byte (polling)
 */
static nx_status_t hw_uart_receive_byte(uint8_t index, uint8_t* byte,
                                        uint32_t timeout_ms) {
    /* TODO: Implement actual STM32F4 UART receive byte */
    /* This would wait for RXNE flag and read from USART_DR */
    (void)index;
    (void)byte;
    (void)timeout_ms;
    return NX_OK;
}

/**
 * \brief           Hardware-specific: Enable TX interrupt
 */
static void hw_uart_enable_tx_interrupt(uint8_t index) {
    /* TODO: Implement actual STM32F4 UART TX interrupt enable */
    /* This would set USART_CR1 TXEIE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Disable TX interrupt
 */
static void hw_uart_disable_tx_interrupt(uint8_t index) {
    /* TODO: Implement actual STM32F4 UART TX interrupt disable */
    /* This would clear USART_CR1 TXEIE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Enable RX interrupt
 */
static void hw_uart_enable_rx_interrupt(uint8_t index) {
    /* TODO: Implement actual STM32F4 UART RX interrupt enable */
    /* This would set USART_CR1 RXNEIE bit */
    (void)index;
}

/**
 * \brief           Hardware-specific: Disable RX interrupt
 */
static void hw_uart_disable_rx_interrupt(uint8_t index) {
    /* TODO: Implement actual STM32F4 UART RX interrupt disable */
    /* This would clear USART_CR1 RXNEIE bit */
    (void)index;
}
/* ========== ISR Callback Functions ========== */

/**
 * \brief           UART ISR callback
 */
static void uart_isr_callback(void* data) {
    nx_uart_state_t* state = (nx_uart_state_t*)data;

    if (!state || !state->initialized) {
        return;
    }

    /* TODO: Handle TX interrupt - send data from TX buffer */
    /* TODO: Handle RX interrupt - receive data to RX buffer */
    /* TODO: Handle error interrupts */

    /* Call RX callback if data available */
    if (state->rx_callback && buffer_get_count(&state->rx_buf) > 0) {
        state->rx_callback(state->rx_callback_ctx);
    }
}

/**
 * \brief           DMA TX complete callback
 */
static void uart_dma_tx_callback(void* user_data, nx_status_t result) {
    nx_uart_state_t* state = (nx_uart_state_t*)user_data;

    if (!state) {
        return;
    }

    state->stats.tx_busy = false;

    if (result != NX_OK) {
        state->stats.tx_errors++;
    }
}

/**
 * \brief           DMA RX complete callback
 */
static void uart_dma_rx_callback(void* user_data, nx_status_t result) {
    nx_uart_state_t* state = (nx_uart_state_t*)user_data;

    if (!state) {
        return;
    }

    state->stats.rx_busy = false;

    if (result != NX_OK) {
        state->stats.rx_errors++;
    }

    /* Call RX callback if configured */
    if (state->rx_callback) {
        state->rx_callback(state->rx_callback_ctx);
    }
}
/* ========== TX Async Operations ========== */

/**
 * \brief           Send data asynchronously
 */
static nx_status_t tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len) {
    nx_uart_impl_t* impl = uart_get_impl_from_tx_async(self);

    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (len == 0) {
        return NX_OK;
    }

    /* Write to TX buffer */
    size_t written = buffer_write(&impl->state->tx_buf, data, len);

    if (written > 0) {
        /* Enable TX interrupt to start transmission */
        hw_uart_enable_tx_interrupt(impl->state->index);
        impl->state->stats.tx_count += written;
    }

    return (written == len) ? NX_OK : NX_ERR_NO_MEMORY;
}

/**
 * \brief           Get free space in TX buffer
 */
static size_t tx_async_get_free_space(nx_tx_async_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_tx_async(self);

    if (!impl || !impl->state || !impl->state->initialized) {
        return 0;
    }

    return buffer_get_free(&impl->state->tx_buf);
}

/**
 * \brief           Check if TX is busy
 */
static bool tx_async_is_busy(nx_tx_async_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_tx_async(self);

    if (!impl || !impl->state || !impl->state->initialized) {
        return false;
    }

    return impl->state->stats.tx_busy ||
           buffer_get_count(&impl->state->tx_buf) > 0;
}
/* ========== RX Async Operations ========== */

/**
 * \brief           Read data asynchronously
 */
static size_t rx_async_read(nx_rx_async_t* self, uint8_t* data,
                            size_t max_len) {
    nx_uart_impl_t* impl = uart_get_impl_from_rx_async(self);

    if (!impl || !impl->state || !data || !impl->state->initialized) {
        return 0;
    }

    size_t read = buffer_read(&impl->state->rx_buf, data, max_len);
    impl->state->stats.rx_count += read;

    return read;
}

/**
 * \brief           Get available data in RX buffer
 */
static size_t rx_async_available(nx_rx_async_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_rx_async(self);

    if (!impl || !impl->state || !impl->state->initialized) {
        return 0;
    }

    return buffer_get_count(&impl->state->rx_buf);
}

/**
 * \brief           Set RX callback
 */
static nx_status_t rx_async_set_callback(nx_rx_async_t* self, void (*cb)(void*),
                                         void* ctx) {
    nx_uart_impl_t* impl = uart_get_impl_from_rx_async(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->rx_callback = cb;
    impl->state->rx_callback_ctx = ctx;

    return NX_OK;
}
/* ========== TX Sync Operations ========== */

/**
 * \brief           Send data synchronously
 */
static nx_status_t tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                size_t len, uint32_t timeout_ms) {
    nx_uart_impl_t* impl = uart_get_impl_from_tx_sync(self);

    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (len == 0) {
        return NX_OK;
    }

    /* Send each byte using polling */
    for (size_t i = 0; i < len; i++) {
        nx_status_t status =
            hw_uart_send_byte(impl->state->index, data[i], timeout_ms);
        if (status != NX_OK) {
            impl->state->stats.tx_errors++;
            return status;
        }
        impl->state->stats.tx_count++;
    }

    return NX_OK;
}

/* ========== RX Sync Operations ========== */

/**
 * \brief           Receive data synchronously
 */
static nx_status_t rx_sync_receive(nx_rx_sync_t* self, uint8_t* data,
                                   size_t len, uint32_t timeout_ms) {
    nx_uart_impl_t* impl = uart_get_impl_from_rx_sync(self);

    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (len == 0) {
        return NX_OK;
    }

    /* Receive each byte using polling */
    for (size_t i = 0; i < len; i++) {
        nx_status_t status =
            hw_uart_receive_byte(impl->state->index, &data[i], timeout_ms);
        if (status != NX_OK) {
            impl->state->stats.rx_errors++;
            return status;
        }
        impl->state->stats.rx_count++;
    }

    return NX_OK;
}
/* ========== UART Base Operations ========== */

/**
 * \brief           Get TX async interface
 */
static nx_tx_async_t* uart_get_tx_async(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->tx_async : NULL;
}

/**
 * \brief           Get RX async interface
 */
static nx_rx_async_t* uart_get_rx_async(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->rx_async : NULL;
}

/**
 * \brief           Get TX sync interface
 */
static nx_tx_sync_t* uart_get_tx_sync(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->tx_sync : NULL;
}

/**
 * \brief           Get RX sync interface
 */
static nx_rx_sync_t* uart_get_rx_sync(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->rx_sync : NULL;
}

/**
 * \brief           Set UART baudrate
 */
static nx_status_t uart_set_baudrate(nx_uart_t* self, uint32_t baudrate) {
    nx_uart_impl_t* impl = uart_get_impl(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    hw_uart_set_baudrate(impl->state->index, baudrate);
    impl->state->config.baudrate = baudrate;

    return NX_OK;
}
/**
 * \brief           Get UART configuration
 */
static nx_status_t uart_get_config(nx_uart_t* self, nx_uart_config_t* cfg) {
    nx_uart_impl_t* impl = uart_get_impl(self);

    if (!impl || !impl->state || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(cfg, &impl->state->config, sizeof(nx_uart_config_t));

    return NX_OK;
}

/**
 * \brief           Set UART configuration
 */
static nx_status_t uart_set_config(nx_uart_t* self,
                                   const nx_uart_config_t* cfg) {
    nx_uart_impl_t* impl = uart_get_impl(self);

    if (!impl || !impl->state || !cfg) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Apply configuration */
    hw_uart_configure(impl->state->index, cfg);
    memcpy(&impl->state->config, cfg, sizeof(nx_uart_config_t));

    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* uart_get_lifecycle(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* uart_get_power(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Get diagnostic interface
 */
static nx_diagnostic_t* uart_get_diagnostic(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}
/**
 * \brief           Get UART statistics
 */
static nx_status_t uart_get_stats(nx_uart_t* self, nx_uart_stats_t* stats) {
    nx_uart_impl_t* impl = uart_get_impl(self);

    if (!impl || !impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    memcpy(stats, &impl->state->stats, sizeof(nx_uart_stats_t));

    return NX_OK;
}

/**
 * \brief           Clear UART errors
 */
static nx_status_t uart_clear_errors(nx_uart_t* self) {
    nx_uart_impl_t* impl = uart_get_impl(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->stats.tx_errors = 0;
    impl->state->stats.rx_errors = 0;
    impl->state->stats.overrun_errors = 0;
    impl->state->stats.framing_errors = 0;

    return NX_OK;
}
/* ========== Lifecycle Operations ========== */

/**
 * \brief           Initialize UART
 */
static nx_status_t uart_lifecycle_init(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Enable UART clock */
    hw_uart_enable_clock(impl->state->index);

    /* Configure UART hardware */
    hw_uart_configure(impl->state->index, &impl->state->config);

    /* Initialize buffers */
    buffer_init(&impl->state->tx_buf, g_uart_tx_buffers[impl->state->index],
                impl->state->config.tx_buf_size);
    buffer_init(&impl->state->rx_buf, g_uart_rx_buffers[impl->state->index],
                impl->state->config.rx_buf_size);

    /* Register ISR callback */
    nx_isr_manager_t* isr_mgr = nx_isr_manager_get();
    if (isr_mgr) {
        /* Calculate UART IRQ number (simplified) */
        uint32_t uart_irq = 37 + impl->state->index; /* USART1 IRQ = 37 */

        impl->state->isr_handle =
            isr_mgr->connect(isr_mgr, uart_irq, uart_isr_callback, impl->state,
                             NX_ISR_PRIORITY_NORMAL);

        if (impl->state->isr_handle) {
            isr_mgr->enable(isr_mgr, uart_irq);
        }
    }

    /* Allocate DMA channels if enabled */
    if (impl->state->config.dma_tx_enable) {
        nx_dma_manager_t* dma_mgr = nx_dma_manager_get();
        if (dma_mgr) {
            impl->state->dma_tx = dma_mgr->alloc(dma_mgr, impl->state->index);
        }
    }

    if (impl->state->config.dma_rx_enable) {
        nx_dma_manager_t* dma_mgr = nx_dma_manager_get();
        if (dma_mgr) {
            impl->state->dma_rx = dma_mgr->alloc(dma_mgr, impl->state->index);
        }
    }

    /* Enable UART */
    hw_uart_enable(impl->state->index);

    /* Enable RX interrupt */
    hw_uart_enable_rx_interrupt(impl->state->index);

    impl->state->initialized = true;
    impl->state->suspended = false;

    return NX_OK;
}
/**
 * \brief           Deinitialize UART
 */
static nx_status_t uart_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Disable interrupts */
    hw_uart_disable_tx_interrupt(impl->state->index);
    hw_uart_disable_rx_interrupt(impl->state->index);

    /* Disconnect ISR callback */
    if (impl->state->isr_handle) {
        nx_isr_manager_t* isr_mgr = nx_isr_manager_get();
        if (isr_mgr) {
            uint32_t uart_irq = 37 + impl->state->index;
            isr_mgr->disable(isr_mgr, uart_irq);
            isr_mgr->disconnect(isr_mgr, impl->state->isr_handle);
        }
        impl->state->isr_handle = NULL;
    }

    /* Free DMA channels */
    if (impl->state->dma_tx) {
        nx_dma_manager_t* dma_mgr = nx_dma_manager_get();
        if (dma_mgr) {
            dma_mgr->free(dma_mgr, impl->state->dma_tx);
        }
        impl->state->dma_tx = NULL;
    }

    if (impl->state->dma_rx) {
        nx_dma_manager_t* dma_mgr = nx_dma_manager_get();
        if (dma_mgr) {
            dma_mgr->free(dma_mgr, impl->state->dma_rx);
        }
        impl->state->dma_rx = NULL;
    }

    /* Disable UART */
    hw_uart_disable(impl->state->index);

    /* Disable clock */
    hw_uart_disable_clock(impl->state->index);

    impl->state->initialized = false;

    return NX_OK;
}
/**
 * \brief           Suspend UART
 */
static nx_status_t uart_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (impl->state->suspended) {
        return NX_OK;
    }

    /* Disable UART */
    hw_uart_disable(impl->state->index);

    /* Disable clock to save power */
    hw_uart_disable_clock(impl->state->index);

    impl->state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume UART
 */
static nx_status_t uart_lifecycle_resume(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_lifecycle(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!impl->state->suspended) {
        return NX_OK;
    }

    /* Re-enable clock */
    hw_uart_enable_clock(impl->state->index);

    /* Reconfigure UART */
    hw_uart_configure(impl->state->index, &impl->state->config);

    /* Re-enable UART */
    hw_uart_enable(impl->state->index);

    /* Re-enable RX interrupt */
    hw_uart_enable_rx_interrupt(impl->state->index);

    impl->state->suspended = false;

    return NX_OK;
}
/**
 * \brief           Get UART state
 */
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

/**
 * \brief           Enable UART power
 */
static nx_status_t uart_power_enable(nx_power_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_power(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    hw_uart_enable_clock(impl->state->index);

    return NX_OK;
}

/**
 * \brief           Disable UART power
 */
static nx_status_t uart_power_disable(nx_power_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_power(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    hw_uart_disable_clock(impl->state->index);

    return NX_OK;
}

/**
 * \brief           Check if UART power is enabled
 */
static bool uart_power_is_enabled(nx_power_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_power(self);

    if (!impl || !impl->state) {
        return false;
    }

    return impl->state->initialized && !impl->state->suspended;
}
/* ========== Diagnostic Operations ========== */

/**
 * \brief           Get UART status
 */
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

/**
 * \brief           Get UART statistics
 */
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

/**
 * \brief           Clear UART statistics
 */
static nx_status_t uart_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_uart_impl_t* impl = uart_get_impl_from_diagnostic(self);

    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    memset(&impl->state->stats, 0, sizeof(nx_uart_stats_t));

    return NX_OK;
}
/* ========== Instance Initialization ========== */

/**
 * \brief           Initialize UART instance
 */
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
    impl->state->dma_tx = NULL;
    impl->state->dma_rx = NULL;
    impl->state->isr_handle = NULL;
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
 * \param[in]       index: UART index (0-5)
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t* nx_uart_stm32f4_get(uint8_t index) {
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
 * \param[in]       index: UART index (0-5)
 * \param[in]       cfg: UART configuration
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t* nx_uart_stm32f4_get_with_config(uint8_t index,
                                           const nx_uart_config_t* cfg) {
    nx_uart_t* uart = nx_uart_stm32f4_get(index);

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
 * \brief           Get UART device descriptor
 * \param[in]       index: UART index
 * \return          Device descriptor pointer, NULL on failure
 */
nx_device_t* nx_uart_stm32f4_get_device(uint8_t index) {
    if (index >= NX_UART_MAX_INSTANCES) {
        return NULL;
    }

    nx_uart_impl_t* impl = &g_uart_instances[index];
    return impl->device;
}
