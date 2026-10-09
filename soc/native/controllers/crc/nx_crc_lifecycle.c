/**
 * \file            nx_crc_lifecycle.c
 * \brief           CRC lifecycle implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements lifecycle management (init, deinit, suspend,
 *                  resume, get_state) for CRC peripheral.
 */

#include "nx_crc_helpers.h"
#include "nx_crc_types.h"

/*---------------------------------------------------------------------------*/
/* Lifecycle Implementation                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize CRC device
 */
static nx_status_t crc_lifecycle_init(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_crc_impl_t* impl =
        (nx_crc_impl_t*)((uint8_t*)self - offsetof(nx_crc_impl_t, lifecycle));
    nx_crc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Initialize CRC to initial value */
    state->current_crc = state->config.init_value;

    /* Clear statistics */
    state->stats.reset_count = 0;
    state->stats.update_count = 0;
    state->stats.calculate_count = 0;
    state->stats.bytes_processed = 0;

    /* Mark as initialized and running */
    state->initialized = true;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize CRC device
 */
static nx_status_t crc_lifecycle_deinit(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_crc_impl_t* impl =
        (nx_crc_impl_t*)((uint8_t*)self - offsetof(nx_crc_impl_t, lifecycle));
    nx_crc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Reset CRC value */
    state->current_crc = 0;

    /* Mark as uninitialized */
    state->initialized = false;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Suspend CRC device
 */
static nx_status_t crc_lifecycle_suspend(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_crc_impl_t* impl =
        (nx_crc_impl_t*)((uint8_t*)self - offsetof(nx_crc_impl_t, lifecycle));
    nx_crc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as suspended (preserve CRC state) */
    state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume CRC device
 */
static nx_status_t crc_lifecycle_resume(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_crc_impl_t* impl =
        (nx_crc_impl_t*)((uint8_t*)self - offsetof(nx_crc_impl_t, lifecycle));
    nx_crc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as running (restore CRC state) */
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get CRC device state
 */
static nx_device_state_t crc_lifecycle_get_state(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    nx_crc_impl_t* impl =
        (nx_crc_impl_t*)((uint8_t*)self - offsetof(nx_crc_impl_t, lifecycle));
    nx_crc_state_t* state = impl->state;

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
void crc_init_lifecycle(nx_lifecycle_t* lifecycle) {
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->init = crc_lifecycle_init;
    lifecycle->deinit = crc_lifecycle_deinit;
    lifecycle->suspend = crc_lifecycle_suspend;
    lifecycle->resume = crc_lifecycle_resume;
    lifecycle->get_state = crc_lifecycle_get_state;
}
