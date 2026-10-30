/**
 * \file            u575_watchdog_leaf_port.c
 * \brief           Execute pinned IWDG word commands
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u5xx_ll_iwdg.h"
#include "u575_watchdog_leaf_model.h"
void nx_u575_iwdg_official_commands(IWDG_TypeDef* registers, uint32_t prescaler,
                                    uint32_t reload, uint32_t keys[4]) {
    LL_IWDG_Enable(registers);
    keys[0] = registers->KR;
    LL_IWDG_EnableWriteAccess(registers);
    keys[1] = registers->KR;
    LL_IWDG_SetPrescaler(registers, prescaler);
    LL_IWDG_SetReloadCounter(registers, reload);
    LL_IWDG_ReloadCounter(registers);
    keys[2] = registers->KR;
    LL_IWDG_DisableWriteAccess(registers);
    keys[3] = registers->KR;
}
