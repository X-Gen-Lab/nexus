/**
 * \file            lifecycle_spi.c
 * \brief           Static polling SPI lifecycle retaining partial cold
 * rollback. \author          Nexus Team \version         1.0.0 \date 2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_lifecycle.h"

/** \brief Admit exact polling resources without touching selected registers. */
static nx_result_t admit(void* state, const void* config) {
    const nx_stm32f4_spi_lifecycle_config_t* binding = config;
    if (binding == NULL)
        return NX_ERROR_INVALID;
    return nx_stm32_spi_admit(state, binding->config, binding->receipt);
}

/** \brief Preserve the actual SPI receipt when built-in rollback is refused. */
static nx_lifecycle_acquisition_t acquire(void* opaque, const void* config) {
    nx_stm32_spi_state_t* state = opaque;
    const nx_stm32f4_spi_lifecycle_config_t* binding = config;
    if (binding == NULL || state == NULL || binding->receipt == NULL)
        return (nx_lifecycle_acquisition_t){NX_ERROR_INVALID, false};
    nx_result_t result =
        nx_stm32_spi_prepare(state, binding->config, binding->receipt);
    return (nx_lifecycle_acquisition_t){
        result, state->registers != NULL || state->active ||
                    binding->receipt->rollback_pending ||
                    binding->receipt->registers_captured ||
                    binding->receipt->captured != 0U};
}

/** \brief Actual SPI close retains busy calls and failed register readback. */
static nx_result_t release(void* state, const void* config) {
    const nx_stm32f4_spi_lifecycle_config_t* binding = config;
    if (binding == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result =
        nx_stm32_spi_close(state, binding->config, binding->receipt);
    if (result == NX_SUCCESS && state != NULL)
        *(nx_stm32_spi_state_t*)state = (nx_stm32_spi_state_t){0};
    return result;
}

const nx_lifecycle_ops_t nx_stm32f4_spi_poll_lifecycle_ops = {admit, acquire,
                                                              release};
