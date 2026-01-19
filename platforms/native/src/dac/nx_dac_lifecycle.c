/**
 * \file            nx_dac_lifecycle.c
 * \brief           DAC lifecycle interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements DAC lifecycle operations including init,
 *                  deinit, suspend, resume, and state query functions.
 */

#include "nx_dac_helpers.h"
#include "nx_dac_types.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC implementation from lifecycle interface
 */
static nx_dac_impl_t* dac_get_impl_from_lifecycle(nx_lifecycle_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_dac_impl_t*)((char*)self - offsetof(nx_dac_impl_t, lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Operations                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DAC device
 */
static nx_status_t dac_lifecycle_init(nx_lifecycle_t* self) {
    nx_dac_impl_t* impl = dac_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    impl->state->clock_enabled = true;
    impl->state->initialized = true;

    /* Initialize all channels to 0 */
    for (int i = 0; i < NX_DAC_MAX_CHANNELS; i++) {
        impl->channels[i].current_value = 0;
    }

    return NX_OK;
}

/**
 * \brief           Deinitialize DAC device
 */
static nx_status_t dac_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_dac_impl_t* impl = dac_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->clock_enabled = false;
    impl->state->initialized = false;

    return NX_OK;
}

/**
 * \brief           Suspend DAC device
 */
static nx_status_t dac_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_dac_impl_t* impl = dac_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->suspended = true;
    impl->state->clock_enabled = false;
    return NX_OK;
}

/**
 * \brief           Resume DAC device
 */
static nx_status_t dac_lifecycle_resume(nx_lifecycle_t* self) {
    nx_dac_impl_t* impl = dac_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->suspended = false;
    impl->state->clock_enabled = true;
    return NX_OK;
}

/**
 * \brief           Get DAC device state
 */
static nx_device_state_t dac_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_dac_impl_t* impl = dac_get_impl_from_lifecycle(self);
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

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface
 */
void dac_init_lifecycle(nx_lifecycle_t* lifecycle) {
    lifecycle->init = dac_lifecycle_init;
    lifecycle->deinit = dac_lifecycle_deinit;
    lifecycle->suspend = dac_lifecycle_suspend;
    lifecycle->resume = dac_lifecycle_resume;
    lifecycle->get_state = dac_lifecycle_get_state;
}
