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
uint32_t nx_f1_rtc_model_read(volatile const uint32_t* reg);
void nx_f1_rtc_model_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef RCC
#undef PWR
#undef RTC
#undef BKP
#define RCC                          (&nx_f1_rtc_model_rcc)
#define PWR                          (&nx_f1_rtc_model_pwr)
#define RTC                          (&nx_f1_rtc_model_rtc)
#define BKP                          (&nx_f1_rtc_model_bkp)
#define NX_STM32F1_RTC_READ(reg)     nx_f1_rtc_model_read((reg))
#define NX_STM32F1_WRITE(reg, value) nx_f1_rtc_model_write((reg), (value))
#endif
