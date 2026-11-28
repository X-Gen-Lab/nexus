/**
 * \file            model.h
 * \brief           F103 WWDG EWI and backup register access model.
 * \author          Nexus Team
 */
#ifndef NEXUS_F103_EWI_MODEL_H
#define NEXUS_F103_EWI_MODEL_H
#include "stm32f1xx.h"
#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef rcc_model;
extern WWDG_TypeDef watchdog_model;
extern PWR_TypeDef power_model;
extern BKP_TypeDef backup_model;
void ewi_write(volatile uint32_t* reg, uint32_t value);
void ewi_enable_irq(IRQn_Type irq);
void ewi_priority(IRQn_Type irq, uint32_t priority);
uint32_t ewi_get_priority(IRQn_Type irq);
uint32_t ewi_vector(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef WWDG
#undef PWR
#undef BKP
#undef NVIC_EnableIRQ
#undef NVIC_SetPriority
#undef NVIC_GetPriority
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#define RCC                          (&rcc_model)
#define WWDG                         (&watchdog_model)
#define PWR                          (&power_model)
#define BKP                          (&backup_model)
#define NVIC_EnableIRQ(irq)          ewi_enable_irq(irq)
#define NVIC_SetPriority(irq, prio)  ewi_priority((irq), (prio))
#define NVIC_GetPriority(irq)        ewi_get_priority(irq)
#define NVIC_GetEnableIRQ(irq)       (ewi_vector(irq) & 1U)
#define NVIC_GetPendingIRQ(irq)      (ewi_vector(irq) & 2U)
#define NVIC_GetActive(irq)          (ewi_vector(irq) & 4U)
#define NX_STM32F1_WRITE(reg, value) ewi_write((reg), (uint32_t)(value))
#endif
