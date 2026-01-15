/**
 * \file            nx_uart.h
 * \brief           UART device interface definition
 * \author          Nexus Team
 */

#ifndef NX_UART_H
#define NX_UART_H

#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           UART configuration structure
 */
typedef struct nx_uart_config_s {
    uint32_t baudrate;    /**< Baud rate */
    uint8_t word_length;  /**< Word length: 8 or 9 */
    uint8_t stop_bits;    /**< Stop bits: 1 or 2 */
    uint8_t parity;       /**< Parity: 0=none, 1=odd, 2=even */
    uint8_t flow_control; /**< Flow control: 0=none, 1=rts, 2=cts, 3=rts_cts */
    bool dma_tx_enable;   /**< Enable DMA for TX */
    bool dma_rx_enable;   /**< Enable DMA for RX */
    size_t tx_buf_size;   /**< TX buffer size */
    size_t rx_buf_size;   /**< RX buffer size */
} nx_uart_config_t;

/**
 * \brief           UART statistics structure
 */
typedef struct nx_uart_stats_s {
    bool tx_busy;            /**< TX busy flag */
    bool rx_busy;            /**< RX busy flag */
    uint32_t tx_count;       /**< Total bytes transmitted */
    uint32_t rx_count;       /**< Total bytes received */
    uint32_t tx_errors;      /**< TX error count */
    uint32_t rx_errors;      /**< RX error count */
    uint32_t overrun_errors; /**< Overrun error count */
    uint32_t framing_errors; /**< Framing error count */
} nx_uart_stats_t;

/**
 * \brief           Asynchronous transmit interface
 */
typedef struct nx_tx_async_s nx_tx_async_t;
struct nx_tx_async_s {
    nx_status_t (*send)(nx_tx_async_t* self, const uint8_t* data, size_t len);
    size_t (*get_free_space)(nx_tx_async_t* self);
    bool (*is_busy)(nx_tx_async_t* self);
};

/**
 * \brief           Asynchronous receive interface
 */
typedef struct nx_rx_async_s nx_rx_async_t;
struct nx_rx_async_s {
    size_t (*read)(nx_rx_async_t* self, uint8_t* data, size_t max_len);
    size_t (*available)(nx_rx_async_t* self);
    nx_status_t (*set_callback)(nx_rx_async_t* self, void (*cb)(void*),
                                void* ctx);
};

/**
 * \brief           Synchronous transmit interface
 */
typedef struct nx_tx_sync_s nx_tx_sync_t;
struct nx_tx_sync_s {
    nx_status_t (*send)(nx_tx_sync_t* self, const uint8_t* data, size_t len,
                        uint32_t timeout_ms);
};

/**
 * \brief           Synchronous receive interface
 */
typedef struct nx_rx_sync_s nx_rx_sync_t;
struct nx_rx_sync_s {
    nx_status_t (*receive)(nx_rx_sync_t* self, uint8_t* data, size_t len,
                           uint32_t timeout_ms);
};

/**
 * \brief           UART device interface
 */
typedef struct nx_uart_s nx_uart_t;
struct nx_uart_s {
    /* Operation interfaces */
    nx_tx_async_t* (*get_tx_async)(nx_uart_t* self);
    nx_rx_async_t* (*get_rx_async)(nx_uart_t* self);
    nx_tx_sync_t* (*get_tx_sync)(nx_uart_t* self);
    nx_rx_sync_t* (*get_rx_sync)(nx_uart_t* self);

    /* Runtime configuration */
    nx_status_t (*set_baudrate)(nx_uart_t* self, uint32_t baudrate);
    nx_status_t (*get_config)(nx_uart_t* self, nx_uart_config_t* cfg);
    nx_status_t (*set_config)(nx_uart_t* self, const nx_uart_config_t* cfg);

    /* Base interfaces */
    nx_lifecycle_t* (*get_lifecycle)(nx_uart_t* self);
    nx_power_t* (*get_power)(nx_uart_t* self);
    nx_diagnostic_t* (*get_diagnostic)(nx_uart_t* self);

    /* Diagnostics */
    nx_status_t (*get_stats)(nx_uart_t* self, nx_uart_stats_t* stats);
    nx_status_t (*clear_errors)(nx_uart_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_UART_H */
