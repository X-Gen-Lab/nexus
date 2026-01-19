/**
 * \file            nx_uart_types.h
 * \brief           UART type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_UART_TYPES_H
#define NX_UART_TYPES_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_uart.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Forward declare device type */
typedef struct nx_device_s nx_device_t;

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_uart_platform_config_s {
    uint8_t uart_index;   /**< UART instance index */
    uint32_t baudrate;    /**< Baud rate */
    uint8_t word_length;  /**< Word length (data bits) */
    uint8_t stop_bits;    /**< Stop bits */
    uint8_t parity;       /**< Parity setting */
    uint8_t flow_control; /**< Flow control setting */
    size_t tx_buf_size;   /**< TX buffer size */
    size_t rx_buf_size;   /**< RX buffer size */
} nx_uart_platform_config_t;

/*---------------------------------------------------------------------------*/
/* Circular Buffer Structure                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Circular buffer structure
 *
 * Used for TX and RX buffering.
 */
typedef struct nx_uart_buffer_s {
    uint8_t* data; /**< Buffer data pointer */
    size_t size;   /**< Buffer size */
    size_t head;   /**< Write position */
    size_t tail;   /**< Read position */
    size_t count;  /**< Number of bytes in buffer */
} nx_uart_buffer_t;

/*---------------------------------------------------------------------------*/
/* UART Configuration Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART runtime configuration structure
 */
typedef struct nx_uart_config_s {
    uint32_t baudrate;    /**< Baud rate */
    uint8_t word_length;  /**< Word length (data bits) */
    uint8_t stop_bits;    /**< Stop bits */
    uint8_t parity;       /**< Parity setting */
    uint8_t flow_control; /**< Flow control setting */
    bool dma_tx_enable;   /**< DMA TX enable flag */
    bool dma_rx_enable;   /**< DMA RX enable flag */
    size_t tx_buf_size;   /**< TX buffer size */
    size_t rx_buf_size;   /**< RX buffer size */
} nx_uart_config_t;

/*---------------------------------------------------------------------------*/
/* UART State Structure                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct nx_uart_state_s {
    uint8_t index;           /**< Instance index */
    nx_uart_config_t config; /**< Configuration */
    nx_uart_stats_t stats;   /**< Statistics */
    nx_uart_buffer_t tx_buf; /**< TX buffer */
    nx_uart_buffer_t rx_buf; /**< RX buffer */
    bool initialized;        /**< Initialization flag */
    bool suspended;          /**< Suspend flag */
    bool tx_busy;            /**< TX busy flag */
} nx_uart_state_t;

/*---------------------------------------------------------------------------*/
/* UART Implementation Structure                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART implementation structure
 *
 * Contains all interfaces and state pointer.
 */
typedef struct nx_uart_impl_s {
    nx_uart_t base;             /**< Base UART interface */
    nx_tx_async_t tx_async;     /**< TX async interface */
    nx_rx_async_t rx_async;     /**< RX async interface */
    nx_tx_sync_t tx_sync;       /**< TX sync interface */
    nx_rx_sync_t rx_sync;       /**< RX sync interface */
    nx_lifecycle_t lifecycle;   /**< Lifecycle interface */
    nx_power_t power;           /**< Power interface */
    nx_diagnostic_t diagnostic; /**< Diagnostic interface */
    nx_uart_state_t* state;     /**< State pointer */
    nx_device_t* device;        /**< Device descriptor */
} nx_uart_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_UART_TYPES_H */
