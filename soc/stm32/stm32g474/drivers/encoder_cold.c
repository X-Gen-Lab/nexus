/**
 * \file            encoder_cold.c
 * \brief           Concrete cold route and lifecycle integration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_provider.h"
#include "stm32g474_timer.h"

/** \brief A retained encoder must reject before either input pad is changed. */
nx_result_t
nx_stm32g474_encoder_prepare(nx_stm32g474_encoder_state_t* state,
                             const nx_stm32g474_encoder_config_t* config) {
    if (state == NULL || config == NULL)
        return NX_ERROR_INVALID;
    if (state->config != NULL || state->initialized)
        return NX_ERROR_BUSY;
    nx_result_t status = nx_stm32g474_pin_alternate(GPIOA, 6U, 2U);
    if (status != NX_SUCCESS)
        return status;
    status = nx_stm32g474_pin_alternate(GPIOA, 7U, 2U);
    if (status != NX_SUCCESS)
        return status;
    return nx_stm32g474_encoder_initialize(state, config);
}
