/**
 * \file            nx_usb_helpers.c
 * \brief           USB helper function implementations for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements helper functions for USB buffer management,
 *                  endpoint configuration, and state validation.
 */

#include "nx_usb_helpers.h"
#include "hal/nx_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Implementation Helpers                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get USB implementation from base interface
 */
nx_usb_impl_t* usb_get_impl(nx_usb_t* base) {
    if (base == NULL) {
        return NULL;
    }
    /* Implementation structure contains base as first member */
    return (nx_usb_impl_t*)base;
}

/**
 * \brief           Get USB state from implementation
 */
nx_usb_state_t* usb_get_state(nx_usb_impl_t* impl) {
    return (impl != NULL) ? impl->state : NULL;
}

/**
 * \brief           Validate USB state
 */
nx_status_t usb_validate_state(nx_usb_state_t* state) {
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Buffer Management Functions                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize circular buffer
 */
void buffer_init(nx_usb_buffer_t* buf, uint8_t* data, size_t size) {
    NX_ASSERT(buf != NULL);
    NX_ASSERT(data != NULL);
    NX_ASSERT(size > 0);

    buf->data = data;
    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

/**
 * \brief           Write data to circular buffer
 */
size_t usb_buffer_write(nx_usb_buffer_t* buf, const uint8_t* data, size_t len) {
    if (buf == NULL || data == NULL || len == 0) {
        return 0;
    }

    size_t space = buf->size - buf->count;
    size_t to_write = (len < space) ? len : space;

    for (size_t i = 0; i < to_write; i++) {
        buf->data[buf->head] = data[i];
        buf->head = (buf->head + 1) % buf->size;
        buf->count++;
    }

    return to_write;
}

/**
 * \brief           Read data from circular buffer
 */
size_t usb_buffer_read(nx_usb_buffer_t* buf, uint8_t* data, size_t len) {
    if (buf == NULL || data == NULL || len == 0) {
        return 0;
    }

    size_t to_read = (len < buf->count) ? len : buf->count;

    for (size_t i = 0; i < to_read; i++) {
        data[i] = buf->data[buf->tail];
        buf->tail = (buf->tail + 1) % buf->size;
        buf->count--;
    }

    return to_read;
}

/**
 * \brief           Get available bytes in buffer
 */
size_t buffer_available(const nx_usb_buffer_t* buf) {
    return (buf != NULL) ? buf->count : 0;
}

/**
 * \brief           Get free space in buffer
 */
size_t buffer_free(const nx_usb_buffer_t* buf) {
    return (buf != NULL) ? (buf->size - buf->count) : 0;
}

/**
 * \brief           Clear buffer
 */
void buffer_clear(nx_usb_buffer_t* buf) {
    if (buf != NULL) {
        buf->head = 0;
        buf->tail = 0;
        buf->count = 0;
    }
}

/*---------------------------------------------------------------------------*/
/* Endpoint Management Functions                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize endpoint
 */
void endpoint_init(nx_usb_endpoint_t* ep) {
    if (ep == NULL) {
        return;
    }

    ep->enabled = false;
    ep->type = NX_USB_EP_TYPE_CONTROL;
    ep->direction = NX_USB_EP_DIR_OUT;
    ep->max_packet_size = 64;
    ep->buffer_len = 0;
    memset(ep->buffer, 0, sizeof(ep->buffer));
}

/**
 * \brief           Configure endpoint
 */
nx_status_t endpoint_configure(nx_usb_endpoint_t* ep, nx_usb_ep_type_t type,
                               nx_usb_ep_dir_t direction,
                               uint16_t max_packet_size) {
    if (ep == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (max_packet_size == 0 || max_packet_size > NX_USB_EP_BUFFER_SIZE) {
        return NX_ERR_INVALID_PARAM;
    }

    ep->enabled = true;
    ep->type = type;
    ep->direction = direction;
    ep->max_packet_size = max_packet_size;
    ep->buffer_len = 0;

    return NX_OK;
}

/**
 * \brief           Write data to endpoint buffer
 */
size_t endpoint_write(nx_usb_endpoint_t* ep, const uint8_t* data, size_t len) {
    if (ep == NULL || data == NULL || len == 0 || !ep->enabled) {
        return 0;
    }

    size_t space = NX_USB_EP_BUFFER_SIZE - ep->buffer_len;
    size_t to_write = (len < space) ? len : space;

    memcpy(&ep->buffer[ep->buffer_len], data, to_write);
    ep->buffer_len += to_write;

    return to_write;
}

/**
 * \brief           Read data from endpoint buffer
 */
size_t endpoint_read(nx_usb_endpoint_t* ep, uint8_t* data, size_t len) {
    if (ep == NULL || data == NULL || len == 0 || !ep->enabled) {
        return 0;
    }

    size_t to_read = (len < ep->buffer_len) ? len : ep->buffer_len;

    memcpy(data, ep->buffer, to_read);

    /* Shift remaining data */
    if (to_read < ep->buffer_len) {
        memmove(ep->buffer, &ep->buffer[to_read], ep->buffer_len - to_read);
    }
    ep->buffer_len -= to_read;

    return to_read;
}

/**
 * \brief           Clear endpoint buffer
 */
void endpoint_clear(nx_usb_endpoint_t* ep) {
    if (ep != NULL) {
        ep->buffer_len = 0;
        memset(ep->buffer, 0, sizeof(ep->buffer));
    }
}
