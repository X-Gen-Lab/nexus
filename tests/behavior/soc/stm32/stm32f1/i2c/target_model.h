/**
 * \file            target_model.h
 * \brief           Add NVIC simulation to the STM32F1 I2C bus model.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_I2C_TARGET_MODEL_H
#define NEXUS_TEST_F1_I2C_TARGET_MODEL_H
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_test_target_enabled(IRQn_Type irq);
uint32_t nx_test_target_pending(IRQn_Type irq);
uint32_t nx_test_target_active(IRQn_Type irq);
uint32_t nx_test_target_priority(IRQn_Type irq);
void nx_test_target_enable(IRQn_Type irq);
void nx_test_target_disable(IRQn_Type irq);
void nx_test_target_ack(IRQn_Type irq);
void nx_test_target_set_priority(IRQn_Type irq, uint32_t priority);
#ifdef __cplusplus
}
#endif

#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_GetPriority
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#undef NVIC_SetPriority
#define NVIC_GetEnableIRQ    nx_test_target_enabled
#define NVIC_GetPendingIRQ   nx_test_target_pending
#define NVIC_GetActive       nx_test_target_active
#define NVIC_GetPriority     nx_test_target_priority
#define NVIC_EnableIRQ       nx_test_target_enable
#define NVIC_DisableIRQ      nx_test_target_disable
#define NVIC_ClearPendingIRQ nx_test_target_ack
#define NVIC_SetPriority     nx_test_target_set_priority
#endif
