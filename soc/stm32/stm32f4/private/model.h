/**
 * \file            model.h
 * \brief           Official device types and the explicit GPIO model binding
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_MODEL_H
#define NX_STM32F4_MODEL_H

#include "stm32f4xx.h"

#ifdef NEXUS_STM32_MODEL
#ifdef __cplusplus
extern "C" {
#endif
extern GPIO_TypeDef g_nx_stm32_gpioa_model;
#ifdef __cplusplus
}
#endif
#undef GPIOA
#define GPIOA (&g_nx_stm32_gpioa_model)
#endif

#endif /* NX_STM32F4_MODEL_H */
