/**
 * \file            nx_i2c_lifecycle.c
 * \brief           I2C lifecycle interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements I2C lifecycle operations including init,
 *                  deinit, suspend, resume, and state query functions.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_i2c_helpers.h"
#include "nx_i2c_types.h"

/*---------------------------------------------------------------------------*/
/* External Buffer References                                                */
/*---------------------------------------------------------------------------*/

/* Buffer pointers are managed in nx_i2c_device.c */
extern uint8_t* g_i2c_tx_buffers[];
extern uint8_t* g_i2c_rx_buffers[];

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Implementation                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize implementation
 */
static nx_status_t i2c_lifecycle_init(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Initialize buffers */
    i2c_buffer_init(&impl->state->tx_buf, g_i2c_tx_buffers[impl->state->index],
                    impl->state->config.tx_buf_size);
    i2c_buffer_init(&impl->state->rx_buf, g_i2c_rx_buffers[impl->state->index],
                    impl->state->config.rx_buf_size);

    /* Set state flags */
    impl->state->initialized = true;
    impl->state->suspended = false;
    impl->state->busy = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize implementation
 */
static nx_status_t i2c_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Clear state flag */
    impl->state->initialized = false;

    return NX_OK;
}

/**
 * \brief           Suspend implementation
 */
static nx_status_t i2c_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Set suspend flag */
    impl->state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume implementation
 */
static nx_status_t i2c_lifecycle_resume(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Clear suspend flag */
    impl->state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get state implementation
 */
static nx_device_state_t i2c_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, lifecycle);

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
void i2c_init_lifecycle(nx_lifecycle_t* lifecycle) {
    lifecycle->init = i2c_lifecycle_init;
    lifecycle->deinit = i2c_lifecycle_deinit;
    lifecycle->suspend = i2c_lifecycle_suspend;
    lifecycle->resume = i2c_lifecycle_resume;
    lifecycle->get_state = i2c_lifecycle_get_state;
}
