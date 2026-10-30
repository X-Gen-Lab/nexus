/**
 * \file            u575_pwm_leaf_port.c
 * \brief           Execute locked U575 LL timer setters from host C11
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u5xx_ll_tim.h"
#include "u575_pwm_leaf_model.h"
void nx_u575_pwm_official_set(TIM_TypeDef* timer, uint32_t prescaler,
                              uint32_t reload, uint32_t compare) {
    LL_TIM_SetPrescaler(timer, prescaler);
    LL_TIM_SetAutoReload(timer, reload);
    LL_TIM_OC_SetCompareCH1(timer, compare);
}
