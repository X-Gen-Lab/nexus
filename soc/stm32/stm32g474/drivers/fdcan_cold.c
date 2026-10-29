/**
 * \file            fdcan_cold.c
 * \brief           Concrete cold route and lifecycle integration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_fdcan.h"
#include "stm32g474_provider.h"

/* Cold ownership uses existing tail padding in the physical controller. */
_Static_assert(sizeof(nx_stm32g474_fdcan_state_t) ==
                   ((offsetof(nx_stm32g474_fdcan_state_t, retained) +
                     sizeof(bool) + _Alignof(nx_stm32g474_fdcan_state_t) - 1U) /
                    _Alignof(nx_stm32g474_fdcan_state_t)) *
                       _Alignof(nx_stm32g474_fdcan_state_t),
               "cold domain ownership must not enlarge controller storage");

/** \brief Controller admission precedes any active transmitter connection. */
nx_result_t
nx_stm32g474_fdcan_prepare(nx_stm32g474_fdcan_state_t* state,
                           const nx_stm32g474_fdcan_config_t* config) {
    if (state == NULL || config == NULL || config->domain == NULL)
        return NX_ERROR_INVALID;
    if (state->retained || state->cold_domain_owned ||
        config->domain->retained || config->domain->acquired_mask != 0U)
        return NX_ERROR_BUSY;
    state->registers = FDCAN1;
    state->mode = config->mode;
    state->domain = config->domain;
    state->filters = config->filters;
    state->filter_count = config->filter_count;
    state->events = config->events;
    state->capacity = config->capacity;
    state->irq_policy = config->irq_policy;
    state->drain_timeout_us = config->drain_timeout_us;
    state->poll_limit = config->poll_limit;
    nx_result_t status = nx_stm32g474_fdcan_domain_initialize(config->domain);
    if (status != NX_SUCCESS)
        return status;
    state->cold_domain_owned = true;
    status = nx_stm32g474_fdcan_initialize(state);
    if (status != NX_SUCCESS)
        return status;
    NX_G474_WRITE(&GPIOA->BSRR, 4096U);
    nx_arch_dsb();
    if ((NX_G474_READ(&GPIOA->ODR) & 4096U) != 4096U)
        return NX_ERROR_IO;
    status = nx_stm32g474_pin_alternate(GPIOA, 11U, 9U);
    if (status != NX_SUCCESS)
        return status;
    return nx_stm32g474_pin_alternate(GPIOA, 12U, 9U);
}
/** \brief Preserve the domain while the controller retains a publisher. */
nx_result_t
nx_stm32g474_fdcan_close(nx_stm32g474_fdcan_state_t* state,
                         const nx_stm32g474_fdcan_config_t* config) {
    if (state == NULL || config == NULL || config->domain == NULL)
        return NX_ERROR_INVALID;
    if (!state->cold_domain_owned)
        return NX_SUCCESS;
    if (state->retained) {
        nx_result_t status = nx_stm32g474_fdcan_ops.stop(state);
        if (status != NX_SUCCESS)
            return status;
    }
    nx_result_t status = nx_stm32g474_fdcan_domain_stop(config->domain);
    if (status == NX_SUCCESS)
        state->cold_domain_owned = false;
    return status;
}
