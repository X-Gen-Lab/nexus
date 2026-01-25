/**
 * \file            nx_usb_helpers.h
 * \brief           USB helper function declarations for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_USB_HELPERS_H
#define NX_USB_HELPERS_H

#include "hal/nx_status.h"
#include "nx_usb_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get USB implementation from base interface
 * \param[in]       base: Base USB interface pointer
 * \return          USB implementation pointer or NULL
 */
nx_usb_impl_t* usb_get_impl(nx_usb_t* base);

/**
 * \brief           Get USB state from implementation
 * \param[in]       impl: USB implementation pointer
 * \return          USB state pointer or NULL
 */
nx_usb_state_t* usb_get_state(nx_usb_impl_t* impl);

/**
 * \brief           Validate USB state
 * \param[in]       state: USB state pointer
 * \return          NX_OK if valid, error code otherwise
 */
nx_status_t usb_validate_state(nx_usb_state_t* state);

/*---------------------------------------------------------------------------*/
/* Buffer Management Functions                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize circular buffer
 * \param[in]       buf: Buffer pointer
 * \param[in]       data: Data storage pointer
 * \param[in]       size: Buffer size
 */
void buffer_init(nx_usb_buffer_t* buf, uint8_t* data, size_t size);

/**
 * \brief           Write data to circular buffer
 * \param[in]       buf: Buffer pointer
 * \param[in]       data: Data to write
 * \param[in]       len: Data length
 * \return          Number of bytes written
 */
size_t usb_buffer_write(nx_usb_buffer_t* buf, const uint8_t* data, size_t len);

/**
 * \brief           Read data from circular buffer
 * \param[in]       buf: Buffer pointer
 * \param[out]      data: Data buffer
 * \param[in]       len: Maximum bytes to read
 * \return          Number of bytes read
 */
size_t usb_buffer_read(nx_usb_buffer_t* buf, uint8_t* data, size_t len);

/**
 * \brief           Get available bytes in buffer
 * \param[in]       buf: Buffer pointer
 * \return          Number of available bytes
 */
size_t buffer_available(const nx_usb_buffer_t* buf);

/**
 * \brief           Get free space in buffer
 * \param[in]       buf: Buffer pointer
 * \return          Number of free bytes
 */
size_t buffer_free(const nx_usb_buffer_t* buf);

/**
 * \brief           Clear buffer
 * \param[in]       buf: Buffer pointer
 */
void buffer_clear(nx_usb_buffer_t* buf);

/*---------------------------------------------------------------------------*/
/* Endpoint Management Functions                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize endpoint
 * \param[in]       ep: Endpoint pointer
 */
void endpoint_init(nx_usb_endpoint_t* ep);

/**
 * \brief           Configure endpoint
 * \param[in]       ep: Endpoint pointer
 * \param[in]       type: Endpoint type
 * \param[in]       direction: Endpoint direction
 * \param[in]       max_packet_size: Maximum packet size
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t endpoint_configure(nx_usb_endpoint_t* ep, nx_usb_ep_type_t type,
                               nx_usb_ep_dir_t direction,
                               uint16_t max_packet_size);

/**
 * \brief           Write data to endpoint buffer
 * \param[in]       ep: Endpoint pointer
 * \param[in]       data: Data to write
 * \param[in]       len: Data length
 * \return          Number of bytes written
 */
size_t endpoint_write(nx_usb_endpoint_t* ep, const uint8_t* data, size_t len);

/**
 * \brief           Read data from endpoint buffer
 * \param[in]       ep: Endpoint pointer
 * \param[out]      data: Data buffer
 * \param[in]       len: Maximum bytes to read
 * \return          Number of bytes read
 */
size_t endpoint_read(nx_usb_endpoint_t* ep, uint8_t* data, size_t len);

/**
 * \brief           Clear endpoint buffer
 * \param[in]       ep: Endpoint pointer
 */
void endpoint_clear(nx_usb_endpoint_t* ep);

/*---------------------------------------------------------------------------*/
/* Event Simulation Functions                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulate USB connect event
 * \param[in]       state: USB state pointer
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t usb_simulate_connect(nx_usb_state_t* state);

/**
 * \brief           Simulate USB disconnect event
 * \param[in]       state: USB state pointer
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t usb_simulate_disconnect(nx_usb_state_t* state);

/**
 * \brief           Simulate USB suspend event
 * \param[in]       state: USB state pointer
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t usb_simulate_suspend(nx_usb_state_t* state);

/**
 * \brief           Simulate USB resume event
 * \param[in]       state: USB state pointer
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t usb_simulate_resume(nx_usb_state_t* state);

/*---------------------------------------------------------------------------*/
/* Interface Initialization Functions                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX async interface
 * \param[in]       tx_async: TX async interface pointer
 */
void usb_init_tx_async(nx_tx_async_t* tx_async);

/**
 * \brief           Initialize RX async interface
 * \param[in]       rx_async: RX async interface pointer
 */
void usb_init_rx_async(nx_rx_async_t* rx_async);

/**
 * \brief           Initialize TX sync interface
 * \param[in]       tx_sync: TX sync interface pointer
 */
void usb_init_tx_sync(nx_tx_sync_t* tx_sync);

/**
 * \brief           Initialize RX sync interface
 * \param[in]       rx_sync: RX sync interface pointer
 */
void usb_init_rx_sync(nx_rx_sync_t* rx_sync);

/**
 * \brief           Initialize USB base interface
 * \param[in]       base: USB base interface pointer
 */
void usb_init_base(nx_usb_t* base);

/**
 * \brief           Reset power context for testing
 * \param[in]       index: USB instance index
 */
void usb_reset_power_context(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NX_USB_HELPERS_H */
