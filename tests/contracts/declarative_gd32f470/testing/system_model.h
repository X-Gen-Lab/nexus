/**
 * \file            system_model.h
 * \brief           External Cortex interrupt boundaries for GD32 System tests
 * \author          Nexus Team
 */
#ifndef NX_GD32_SYSTEM_MODEL_H
#define NX_GD32_SYSTEM_MODEL_H
#include "gd32f4xx.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_gd32_system_irq_enabled(IRQn_Type irq);
uint32_t nx_gd32_system_irq_pending(IRQn_Type irq);
uint32_t nx_gd32_system_irq_active(IRQn_Type irq);
void nx_gd32_system_irq_priority(IRQn_Type irq, uint32_t value);
void nx_gd32_system_irq_enable(IRQn_Type irq);
void nx_gd32_system_irq_disable(IRQn_Type irq);
void nx_gd32_system_irq_clear(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_SetPriority
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#define NVIC_GetEnableIRQ(irq)       nx_gd32_system_irq_enabled(irq)
#define NVIC_GetPendingIRQ(irq)      nx_gd32_system_irq_pending(irq)
#define NVIC_GetActive(irq)          nx_gd32_system_irq_active(irq)
#define NVIC_SetPriority(irq, value) nx_gd32_system_irq_priority(irq, value)
#define NVIC_EnableIRQ(irq)          nx_gd32_system_irq_enable(irq)
#define NVIC_DisableIRQ(irq)         nx_gd32_system_irq_disable(irq)
#define NVIC_ClearPendingIRQ(irq)    nx_gd32_system_irq_clear(irq)
#endif /* NX_GD32_SYSTEM_MODEL_H */
