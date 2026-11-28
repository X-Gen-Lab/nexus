/**
 * \file            can_normal_model.h
 * \brief           F103 CAN, GPIO and external transceiver register model.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_TEST_F103_CAN_NORMAL_MODEL_H
#define NEXUS_TEST_F103_CAN_NORMAL_MODEL_H
#include "can_model.h"

#ifdef __cplusplus
extern "C" {
#endif
extern GPIO_TypeDef nx_can_model_gpioa;
extern GPIO_TypeDef nx_can_model_gpioc;
extern AFIO_TypeDef nx_can_model_afio;
void nx_can_model_bsrr(GPIO_TypeDef* registers, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef GPIOA
#undef GPIOC
#undef AFIO
#define GPIOA (&nx_can_model_gpioa)
#define GPIOC (&nx_can_model_gpioc)
#define AFIO  (&nx_can_model_afio)
#define NX_STM32F1_GPIO_BSRR(registers, value)                                 \
    nx_can_model_bsrr((registers), (value))
#define NX_STM32F1_CAN_RS_WRITE(registers, value)                              \
    nx_can_model_bsrr((registers), (value))
#endif
