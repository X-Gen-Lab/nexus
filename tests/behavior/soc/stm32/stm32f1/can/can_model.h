/**
 * \file            can_model.h
 * \brief           Exact F103xB bxCAN register transitions for host contracts.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F103_CAN_MODEL_H
#define NEXUS_TEST_F103_CAN_MODEL_H
#include "stm32f1xx.h"
#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef nx_can_model_rcc;
extern CAN_TypeDef nx_can_model_can1;
void nx_can_model_write(volatile uint32_t* reg, uint32_t value);
uint32_t nx_can_model_irq(void);
#ifdef __cplusplus
}
#endif
#undef CAN1
#undef RCC
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#define CAN1                         (&nx_can_model_can1)
#define RCC                          (&nx_can_model_rcc)
#define NX_STM32F1_WRITE(reg, value) nx_can_model_write((reg), (value))
#define NVIC_GetEnableIRQ(irq)       ((void)(irq), nx_can_model_irq())
#define NVIC_GetPendingIRQ(irq)      ((void)(irq), nx_can_model_irq())
#define NVIC_GetActive(irq)          ((void)(irq), nx_can_model_irq())
#endif
