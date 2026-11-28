/**
 * \file            register_model.h
 * \brief           F103xB RTC backup domain register test hooks.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_RTC_REGISTER_MODEL_H
#define NEXUS_TEST_F1_RTC_REGISTER_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef nx_f1_rtc_model_rcc;
extern PWR_TypeDef nx_f1_rtc_model_pwr;
extern RTC_TypeDef nx_f1_rtc_model_rtc;
extern BKP_TypeDef nx_f1_rtc_model_bkp;
extern EXTI_TypeDef nx_f1_rtc_model_exti;
uint32_t nx_f1_rtc_model_get_enable(IRQn_Type irq);
uint32_t nx_f1_rtc_model_get_pending(IRQn_Type irq);
uint32_t nx_f1_rtc_model_get_active(IRQn_Type irq);
uint32_t nx_f1_rtc_model_get_priority(IRQn_Type irq);
void nx_f1_rtc_model_set_priority(IRQn_Type irq, uint32_t priority);
void nx_f1_rtc_model_enable(IRQn_Type irq);
void nx_f1_rtc_model_disable(IRQn_Type irq);
void nx_f1_rtc_model_clear(IRQn_Type irq);
uint32_t nx_f1_rtc_model_read(volatile const uint32_t* reg);
void nx_f1_rtc_model_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef RCC
#undef PWR
#undef RTC
#undef BKP
#undef EXTI
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_GetPriority
#undef NVIC_SetPriority
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#define RCC                          (&nx_f1_rtc_model_rcc)
#define PWR                          (&nx_f1_rtc_model_pwr)
#define RTC                          (&nx_f1_rtc_model_rtc)
#define EXTI                         (&nx_f1_rtc_model_exti)
#define NVIC_GetEnableIRQ            nx_f1_rtc_model_get_enable
#define NVIC_GetPendingIRQ           nx_f1_rtc_model_get_pending
#define NVIC_GetActive               nx_f1_rtc_model_get_active
#define NVIC_GetPriority             nx_f1_rtc_model_get_priority
#define NVIC_SetPriority             nx_f1_rtc_model_set_priority
#define NVIC_EnableIRQ               nx_f1_rtc_model_enable
#define NVIC_DisableIRQ              nx_f1_rtc_model_disable
#define NVIC_ClearPendingIRQ         nx_f1_rtc_model_clear
#define BKP                          (&nx_f1_rtc_model_bkp)
#define NX_STM32F1_RTC_READ(reg)     nx_f1_rtc_model_read((reg))
#define NX_STM32F1_WRITE(reg, value) nx_f1_rtc_model_write((reg), (value))
#define NX_STM32F1_RTC_PROVISION_IRQS_IDLE()                                   \
    (!nx_f1_rtc_model_get_enable(RTC_Alarm_IRQn))
#endif
