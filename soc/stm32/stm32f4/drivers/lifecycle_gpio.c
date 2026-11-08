/**
 * \file            lifecycle_gpio.c
 * \brief           Static GPIO lifecycle over the unique shared pad owner.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_lifecycle.h"

/** \brief Validate the exact bank and reject a retained software binding. */
static nx_result_t admit(void* opaque, const void* config) {
    const nx_stm32_gpio_state_t* state = opaque;
    nx_result_t result = nx_stm32_gpio_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    return state->registers == NULL ? NX_SUCCESS : NX_ERROR_BUSY;
}

/** \brief Existing C preparation owns electrical writes; shared pads outlive
 * it. */
static nx_lifecycle_acquisition_t acquire(void* opaque, const void* config) {
    nx_stm32_gpio_state_t* state = opaque;
    nx_result_t result = nx_stm32_gpio_prepare(state, config);
    return (nx_lifecycle_acquisition_t){result, state != NULL &&
                                                    state->registers != NULL};
}

/** \brief Withdraw software access at its declared idle level before pad
 * restore. */
static nx_result_t release(void* opaque, const void* config) {
    nx_result_t result = nx_stm32f4_lifecycle_context(NULL, NX_LIFECYCLE_STOP);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_gpio_state_t* state = opaque;
    const nx_stm32_gpio_config_t* gpio = config;
    if (state == NULL || gpio == NULL)
        return NX_ERROR_INVALID;
    result = nx_stm32_gpio_stop(state, gpio->initial);
    if (result == NX_SUCCESS)
        *state = (nx_stm32_gpio_state_t){0};
    return result;
}

const nx_lifecycle_ops_t nx_stm32f4_gpio_lifecycle_ops = {admit, acquire,
                                                          release};
