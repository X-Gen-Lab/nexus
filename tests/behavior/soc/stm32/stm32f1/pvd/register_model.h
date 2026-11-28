/**
 * \file            register_model.h
 * \brief           F103xB PVD power gate, flags and vector observations.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_PVD_REGISTER_MODEL_H
#define NEXUS_TEST_F1_PVD_REGISTER_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef nx_pvd_model_rcc;
extern PWR_TypeDef nx_pvd_model_pwr;
extern EXTI_TypeDef nx_pvd_model_exti;
void nx_pvd_model_write(volatile uint32_t* reg, uint32_t value);
uint32_t nx_pvd_model_enabled(void);
uint32_t nx_pvd_model_pending(void);
uint32_t nx_pvd_model_active(void);
#ifdef __cplusplus
}
#endif

#undef RCC
#undef PWR
#undef EXTI
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#define RCC                          (&nx_pvd_model_rcc)
#define PWR                          (&nx_pvd_model_pwr)
#define EXTI                         (&nx_pvd_model_exti)
#define NX_STM32F1_WRITE(reg, value) nx_pvd_model_write((reg), (value))
#define NVIC_GetEnableIRQ(irq)       nx_pvd_model_enabled()
#define NVIC_GetPendingIRQ(irq)      nx_pvd_model_pending()
#define NVIC_GetActive(irq)          nx_pvd_model_active()
#endif
