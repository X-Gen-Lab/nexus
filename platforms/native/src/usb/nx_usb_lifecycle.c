/**
 * \file            nx_usb_lifecycle.c
 * \brief           USB lifecycle implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements lifecycle management (init, deinit, suspend,
 *                  resume, get_state) for USB peripheral.
 */

#include "nexus_config.h"
#include "nx_usb_helpers.h"
#include "nx_usb_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* External Buffer References                                                */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Lifecycle Implementation                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize USB device
 */
static nx_status_t usb_lifecycle_init(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, lifecycle));
    nx_usb_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Clear TX and RX buffers */
    if (state->tx_buf.data) {
        memset(state->tx_buf.data, 0, state->config.tx_buf_size);
    }
    if (state->rx_buf.data) {
        memset(state->rx_buf.data, 0, state->config.rx_buf_size);
    }
    state->tx_buf.head = 0;
    state->tx_buf.tail = 0;
    state->tx_buf.count = 0;
    state->rx_buf.head = 0;
    state->rx_buf.tail = 0;
    state->rx_buf.count = 0;

    /* Initialize endpoints */
    for (uint8_t i = 0; i < NX_USB_MAX_ENDPOINTS; i++) {
        endpoint_init(&state->endpoints[i]);
    }

    /* Clear statistics */
    state->stats.tx_count = 0;
    state->stats.rx_count = 0;
    state->stats.tx_bytes = 0;
    state->stats.rx_bytes = 0;
    state->stats.connect_count = 0;
    state->stats.disconnect_count = 0;
    state->stats.suspend_count = 0;
    state->stats.resume_count = 0;

    /* Set initial connection state based on Kconfig */
#if defined(NX_CONFIG_USB_AUTO_CONNECT) && (NX_CONFIG_USB_AUTO_CONNECT == 0)
    state->connected = false;
#else
    /* Default to connected for simulation/testing */
    state->connected = true;
#endif

    /* Mark as initialized and running */
    state->initialized = true;
    state->suspended = false;
    state->tx_busy = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize USB device
 */
static nx_status_t usb_lifecycle_deinit(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, lifecycle));
    nx_usb_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Clear buffers */
    buffer_clear(&state->tx_buf);
    buffer_clear(&state->rx_buf);

    /* Disable all endpoints */
    for (uint8_t i = 0; i < NX_USB_MAX_ENDPOINTS; i++) {
        state->endpoints[i].enabled = false;
        endpoint_clear(&state->endpoints[i]);
    }

    /* Disconnect */
    state->connected = false;

    /* Mark as uninitialized */
    state->initialized = false;
    state->suspended = false;
    state->tx_busy = false;

    return NX_OK;
}

/**
 * \brief           Suspend USB device
 */
static nx_status_t usb_lifecycle_suspend(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, lifecycle));
    nx_usb_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as suspended (preserve USB state) */
    state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume USB device
 */
static nx_status_t usb_lifecycle_resume(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, lifecycle));
    nx_usb_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as running (restore USB state) */
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get USB device state
 */
static nx_device_state_t usb_lifecycle_get_state(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, lifecycle));
    nx_usb_state_t* state = impl->state;

    if (state == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    if (!state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    if (state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }

    return NX_DEV_STATE_RUNNING;
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Initialization                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface
 */
void usb_init_lifecycle(nx_lifecycle_t* lifecycle) {
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->init = usb_lifecycle_init;
    lifecycle->deinit = usb_lifecycle_deinit;
    lifecycle->suspend = usb_lifecycle_suspend;
    lifecycle->resume = usb_lifecycle_resume;
    lifecycle->get_state = usb_lifecycle_get_state;
}
