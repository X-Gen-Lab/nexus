/**
 * \file            timer_capture_cold.c
 * \brief           Concrete cold route and lifecycle integration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_provider.h"
#include "stm32g474_timer.h"

/** \brief Acquire the original vector only after exact pin/controller setup. */
nx_result_t
nx_stm32g474_timer_capture_prepare(nx_timer_capture_controller_t* state,
                                   const nx_timer_capture_config_t* config,
                                   nx_stm32g474_capture_cold_t* cold,
                                   uint8_t priority) {
    if (state == NULL || config == NULL || cold == NULL || priority >= 16U)
        return NX_ERROR_INVALID;
    if (state->config != NULL || state->initialized || cold->vector_owned)
        return NX_ERROR_BUSY;
    nx_result_t status = nx_stm32g474_pin_alternate(GPIOA, 0U, 2U);
    if (status != NX_SUCCESS)
        return status;
    status = nx_stm32g474_timer_capture_initialize(state, config);
    if (status != NX_SUCCESS)
        return status;
    cold->saved_priority = (uint8_t)NX_G474_IRQ_PRIORITY(TIM5_IRQn);
    cold->vector_owned = true;
    return NX_G474_IRQ_CONTROL(TIM5_IRQn, true, priority);
}
/** \brief Retain the vector receipt until disable and priority restoration. */
nx_result_t
nx_stm32g474_timer_capture_close(nx_timer_capture_controller_t* state,
                                 nx_stm32g474_capture_cold_t* cold) {
    if (state == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    nx_result_t status;
    if (state->config != NULL) {
        status = nx_stm32g474_timer_capture_ops.stop(state);
        if (status != NX_SUCCESS)
            return status;
    }
    if (cold->vector_owned) {
        status = NX_G474_IRQ_CONTROL(TIM5_IRQn, false, 0U);
        if (status != NX_SUCCESS)
            return status;
        if (!NX_G474_IRQ_AVAILABLE(TIM5_IRQn))
            return NX_ERROR_BUSY;
        NX_G474_IRQ_PRIORITY_WRITE(TIM5_IRQn, cold->saved_priority);
        nx_arch_dsb();
        nx_arch_isb();
        if (NX_G474_IRQ_PRIORITY(TIM5_IRQn) != cold->saved_priority)
            return NX_ERROR_IO;
        cold->vector_owned = false;
    }
    return nx_stm32g474_timer_capture_release(state);
}
