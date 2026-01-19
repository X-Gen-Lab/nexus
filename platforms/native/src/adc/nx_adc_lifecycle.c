/**
 * \file            nx_adc_lifecycle.c
 * \brief           ADC lifecycle interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements ADC lifecycle operations including init,
 *                  deinit, suspend, resume, and state query functions.
 */

#include "nx_adc_helpers.h"
#include "nx_adc_types.h"
#include <stddef.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC implementation from lifecycle interface
 */
static nx_adc_impl_t* adc_get_impl_from_lifecycle(nx_lifecycle_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_adc_impl_t*)((char*)self - offsetof(nx_adc_impl_t, lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Operations                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize ADC device
 */
static nx_status_t adc_lifecycle_init(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    impl->state->clock_enabled = true;
    impl->state->initialized = true;
    impl->state->stats.conversion_count = 0;
    impl->state->stats.error_count = 0;

    /* Initialize simulated channel values with random data */
    for (int i = 0; i < NX_ADC_MAX_CHANNELS; i++) {
        impl->channels[i].simulated_value = (uint16_t)(rand() % 4096);
    }

    return NX_OK;
}

/**
 * \brief           Deinitialize ADC device
 */
static nx_status_t adc_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
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
 * \brief           Suspend ADC device
 */
static nx_status_t adc_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
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
 * \brief           Resume ADC device
 */
static nx_status_t adc_lifecycle_resume(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
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
 * \brief           Get ADC device state
 */
static nx_device_state_t adc_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
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
void adc_init_lifecycle(nx_lifecycle_t* lifecycle) {
    lifecycle->init = adc_lifecycle_init;
    lifecycle->deinit = adc_lifecycle_deinit;
    lifecycle->suspend = adc_lifecycle_suspend;
    lifecycle->resume = adc_lifecycle_resume;
    lifecycle->get_state = adc_lifecycle_get_state;
}
