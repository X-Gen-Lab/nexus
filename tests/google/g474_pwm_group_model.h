/**
 * \file            g474_pwm_group_model.h
 * \brief           External TIM1 shadow/update and COMP1 register boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_G474_PWM_GROUP_MODEL_H
#define NX_G474_PWM_GROUP_MODEL_H
#include "stm32g474xx.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
extern TIM_TypeDef g_group_timer;
extern COMP_TypeDef g_group_comparator;
extern RCC_TypeDef g_group_clocks;
extern bool g_group_irq_quiesced;
uint32_t nx_group_model_read(volatile uint32_t* address);
void nx_group_model_write(volatile uint32_t* address, uint32_t value);
#define NX_G474_GROUP_TIM1            (&g_group_timer)
#define NX_G474_GROUP_COMP1           (&g_group_comparator)
#define NX_G474_GROUP_RCC             (&g_group_clocks)
#define NX_G474_READ(address)         nx_group_model_read(address)
#define NX_G474_WRITE(address, value) nx_group_model_write((address), (value))
#define NX_G474_IRQ_AVAILABLE(irq)    (g_group_irq_quiesced)
#define NX_G474_BOOST_DELAY(system)   ((void)(system))
#ifdef __cplusplus
}
#endif
#endif
