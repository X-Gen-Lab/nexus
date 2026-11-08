/**
 * \file            fsmc_boot.c
 * \brief           Selected bridge from actual reset SRAM proof to runtime
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_fsmc.h"

nx_result_t nx_stm32_fsmc_adopt_boot(nx_stm32_fsmc_t* bus,
                                     const nx_stm32_fsmc_config_t* config,
                                     nx_stm32_fsmc_bank_state_t* banks,
                                     size_t bank_count,
                                     const nx_stm32_sram_boot_plan_t* plan) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (plan == NULL)
        return NX_ERROR_INVALID;
    if (!nx_stm32_sram_boot_matches(plan))
        return NX_ERROR_STATE;
    return nx_stm32_fsmc_adopt_proven(bus, config, banks, bank_count, plan);
}
