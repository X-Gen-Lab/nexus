/**
 * \file            system_lifecycle.c
 * \brief           Final shared-resource ordering before system withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_platform_cold.h"

nx_result_t nx_stm32_platform_system_release(nx_stm32_system_t* system,
                                             bool acquired,
                                             uint32_t poll_limit) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (system == NULL || poll_limit == 0U)
        return NX_ERROR_INVALID;
    /* The final ordering belongs to C, after all resource withdrawal proofs. */
    nx_arch_dsb();
    return !acquired || nx_stm32_system_stop(system, poll_limit) == 0
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
