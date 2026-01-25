/**
 * \file            nx_uart_lifecycle.c
 * \brief           UART lifecycle interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART lifecycle operations including init,
 *                  deinit, suspend, resume, and state query functions.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_uart_helpers.h"
#include "nx_uart_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* External Buffer References                                                */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Implementation                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize implementation
 */
static nx_status_t uart_lifecycle_init(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Clear buffer contents and reset pointers */
    if (impl->state->tx_buf.data != NULL) {
        memset(impl->state->tx_buf.data, 0, impl->state->tx_buf.size);
        impl->state->tx_buf.head = 0;
        impl->state->tx_buf.tail = 0;
        impl->state->tx_buf.count = 0;
    }

    if (impl->state->rx_buf.data != NULL) {
        memset(impl->state->rx_buf.data, 0, impl->state->rx_buf.size);
        impl->state->rx_buf.head = 0;
        impl->state->rx_buf.tail = 0;
        impl->state->rx_buf.count = 0;
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_uart_stats_t));

    /* Set state flags */
    impl->state->initialized = true;
    impl->state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize implementation
 */
static nx_status_t uart_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Clear buffer contents and reset pointers */
    if (impl->state->tx_buf.data != NULL) {
        memset(impl->state->tx_buf.data, 0, impl->state->tx_buf.size);
        impl->state->tx_buf.head = 0;
        impl->state->tx_buf.tail = 0;
        impl->state->tx_buf.count = 0;
    }

    if (impl->state->rx_buf.data != NULL) {
        memset(impl->state->rx_buf.data, 0, impl->state->rx_buf.size);
        impl->state->rx_buf.head = 0;
        impl->state->rx_buf.tail = 0;
        impl->state->rx_buf.count = 0;
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_uart_stats_t));

    /* Clear state flags */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->tx_busy = false;

    return NX_OK;
}

/**
 * \brief           Suspend implementation
 */
static nx_status_t uart_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if already suspended */
    if (impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Set suspend flag */
    impl->state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume implementation
 */
static nx_status_t uart_lifecycle_resume(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if not suspended */
    if (!impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Clear suspend flag */
    impl->state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get state implementation
 */
static nx_device_state_t uart_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state) {
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

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface
 */
void uart_init_lifecycle(nx_lifecycle_t* lifecycle) {
    lifecycle->init = uart_lifecycle_init;
    lifecycle->deinit = uart_lifecycle_deinit;
    lifecycle->suspend = uart_lifecycle_suspend;
    lifecycle->resume = uart_lifecycle_resume;
    lifecycle->get_state = uart_lifecycle_get_state;
}
