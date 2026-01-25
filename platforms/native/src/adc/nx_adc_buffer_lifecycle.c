/**
 * \file            nx_adc_buffer_lifecycle.c
 * \brief           ADC Buffer lifecycle interface implementation for Native
 *                  platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements ADC Buffer lifecycle operations including init,
 *                  deinit, suspend, resume, and state query functions.
 */

#include "nx_adc_helpers.h"
#include "nx_adc_types.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* Lifecycle Operations                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief
 *
 */
static nx_status_t adc_buffer_lifecycle_init(nx_lifecycle_t* self) {
    nx_adc_buffer_impl_t* impl =
        NX_CONTAINER_OF(self, nx_adc_buffer_impl_t, lifecycle);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    impl->state->clock_enabled = true;
    impl->state->initialized = true;
    impl->state->sampling_active = false;
    impl->state->current_index = 0;

    return NX_OK;
}

/**
 * \brief           Deinitialize ADC buffer device
 */
static nx_status_t adc_buffer_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_adc_buffer_impl_t* impl =
        NX_CONTAINER_OF(self, nx_adc_buffer_impl_t, lifecycle);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->clock_enabled = false;
    impl->state->initialized = false;
    impl->state->sampling_active = false;

    return NX_OK;
}

/**
 * \brief           Suspend ADC buffer device
 */
static nx_status_t adc_buffer_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_adc_buffer_impl_t* impl =
        NX_CONTAINER_OF(self, nx_adc_buffer_impl_t, lifecycle);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->clock_enabled = false;
    return NX_OK;
}

/**
 * \brief           Resume ADC buffer device
 */
static nx_status_t adc_buffer_lifecycle_resume(nx_lifecycle_t* self) {
    nx_adc_buffer_impl_t* impl =
        NX_CONTAINER_OF(self, nx_adc_buffer_impl_t, lifecycle);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    impl->state->clock_enabled = true;
    return NX_OK;
}

/**
 * \brief           Get ADC buffer device state
 */
static nx_device_state_t adc_buffer_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_adc_buffer_impl_t* impl =
        NX_CONTAINER_OF(self, nx_adc_buffer_impl_t, lifecycle);
    if (!impl || !impl->state) {
        return NX_DEV_STATE_ERROR;
    }
    if (!impl->state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }
    if (!impl->state->clock_enabled) {
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
void adc_buffer_init_lifecycle(nx_lifecycle_t* lifecycle) {
    lifecycle->init = adc_buffer_lifecycle_init;
    lifecycle->deinit = adc_buffer_lifecycle_deinit;
    lifecycle->suspend = adc_buffer_lifecycle_suspend;
    lifecycle->resume = adc_buffer_lifecycle_resume;
    lifecycle->get_state = adc_buffer_lifecycle_get_state;
}
