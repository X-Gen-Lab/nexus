/**
 * \file            nx_usb_types.h
 * \brief           USB type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_USB_TYPES_H
#define NX_USB_TYPES_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_usb.h"
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
/* USB Constants                                                             */
/*---------------------------------------------------------------------------*/

#define NX_USB_MAX_ENDPOINTS  8    /**< Maximum number of endpoints */
#define NX_USB_EP_BUFFER_SIZE 512  /**< Endpoint buffer size */
#define NX_USB_TX_BUFFER_SIZE 1024 /**< TX buffer size */
#define NX_USB_RX_BUFFER_SIZE 1024 /**< RX buffer size */

/*---------------------------------------------------------------------------*/
/* USB Endpoint Types                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB endpoint type
 */
typedef enum {
    NX_USB_EP_TYPE_CONTROL = 0, /**< Control endpoint */
    NX_USB_EP_TYPE_BULK,        /**< Bulk endpoint */
    NX_USB_EP_TYPE_INTERRUPT,   /**< Interrupt endpoint */
    NX_USB_EP_TYPE_ISOCHRONOUS, /**< Isochronous endpoint */
} nx_usb_ep_type_t;

/**
 * \brief           USB endpoint direction
 */
typedef enum {
    NX_USB_EP_DIR_OUT = 0, /**< OUT endpoint (host to device) */
    NX_USB_EP_DIR_IN,      /**< IN endpoint (device to host) */
} nx_usb_ep_dir_t;

/*---------------------------------------------------------------------------*/
/* USB Endpoint Structure                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB endpoint structure
 */
typedef struct nx_usb_endpoint_s {
    bool enabled;                          /**< Endpoint enabled flag */
    nx_usb_ep_type_t type;                 /**< Endpoint type */
    nx_usb_ep_dir_t direction;             /**< Endpoint direction */
    uint16_t max_packet_size;              /**< Maximum packet size */
    uint8_t buffer[NX_USB_EP_BUFFER_SIZE]; /**< Endpoint buffer */
    size_t buffer_len;                     /**< Current buffer length */
} nx_usb_endpoint_t;

/*---------------------------------------------------------------------------*/
/* Circular Buffer Structure                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Circular buffer structure
 *
 * Used for TX and RX buffering.
 */
typedef struct nx_usb_buffer_s {
    uint8_t* data; /**< Buffer data pointer */
    size_t size;   /**< Buffer size */
    size_t head;   /**< Write position */
    size_t tail;   /**< Read position */
    size_t count;  /**< Number of bytes in buffer */
} nx_usb_buffer_t;

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_usb_platform_config_s {
    uint8_t usb_index;     /**< USB instance index */
    uint8_t num_endpoints; /**< Number of endpoints */
    size_t tx_buf_size;    /**< TX buffer size */
    size_t rx_buf_size;    /**< RX buffer size */
} nx_usb_platform_config_t;

/*---------------------------------------------------------------------------*/
/* USB Configuration Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB runtime configuration structure
 */
typedef struct nx_usb_config_s {
    uint8_t num_endpoints; /**< Number of endpoints */
    size_t tx_buf_size;    /**< TX buffer size */
    size_t rx_buf_size;    /**< RX buffer size */
} nx_usb_config_t;

/*---------------------------------------------------------------------------*/
/* USB Statistics Structure                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB statistics structure
 */
typedef struct nx_usb_stats_s {
    uint32_t tx_count;         /**< Number of transmit operations */
    uint32_t rx_count;         /**< Number of receive operations */
    uint32_t tx_bytes;         /**< Total bytes transmitted */
    uint32_t rx_bytes;         /**< Total bytes received */
    uint32_t connect_count;    /**< Number of connect events */
    uint32_t disconnect_count; /**< Number of disconnect events */
    uint32_t suspend_count;    /**< Number of suspend events */
    uint32_t resume_count;     /**< Number of resume events */
} nx_usb_stats_t;

/*---------------------------------------------------------------------------*/
/* USB State Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct nx_usb_state_s {
    uint8_t index;                                     /**< Instance index */
    nx_usb_config_t config;                            /**< Configuration */
    nx_usb_stats_t stats;                              /**< Statistics */
    nx_usb_buffer_t tx_buf;                            /**< TX buffer */
    nx_usb_buffer_t rx_buf;                            /**< RX buffer */
    nx_usb_endpoint_t endpoints[NX_USB_MAX_ENDPOINTS]; /**< Endpoints */
    bool initialized; /**< Initialization flag */
    bool suspended;   /**< Suspend flag */
    bool connected;   /**< Connection flag */
    bool tx_busy;     /**< TX busy flag */
} nx_usb_state_t;

/*---------------------------------------------------------------------------*/
/* USB Implementation Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB implementation structure
 *
 * Contains all interfaces and state pointer.
 */
typedef struct nx_usb_impl_s {
    nx_usb_t base;            /**< Base USB interface */
    nx_tx_async_t tx_async;   /**< TX async interface */
    nx_rx_async_t rx_async;   /**< RX async interface */
    nx_tx_sync_t tx_sync;     /**< TX sync interface */
    nx_rx_sync_t rx_sync;     /**< RX sync interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */
    nx_usb_state_t* state;    /**< State pointer */
    nx_device_t* device;      /**< Device descriptor */
} nx_usb_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_USB_TYPES_H */
