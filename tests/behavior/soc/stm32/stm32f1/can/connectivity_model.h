/**
 * \file            connectivity_model.h
 * \brief           Host-only STM32F105/F107 bxCAN supplier admission model.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F1_CONNECTIVITY_CAN_MODEL_H
#define NEXUS_STM32F1_CONNECTIVITY_CAN_MODEL_H

#include "can_access.h"
#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef nx_f1_conn_model_rcc;
extern nx_native_can_t nx_f1_conn_model_ports[2];
bool nx_f1_conn_model_irq_busy(IRQn_Type irq);
#ifdef __cplusplus
}
#endif

#undef RCC
#undef CAN1_BASE
#undef CAN2_BASE
#define RCC                               (&nx_f1_conn_model_rcc)
#define CAN1_BASE                         ((uintptr_t)&nx_f1_conn_model_ports[0].hardware)
#define CAN2_BASE                         ((uintptr_t)&nx_f1_conn_model_ports[1].hardware)
#define NX_STM32F1_CONN_CAN_IRQ_BUSY(irq) nx_f1_conn_model_irq_busy(irq)
#define NX_STM32F1_CONN_CAN_IRQ_GROUP()   3U
#endif
