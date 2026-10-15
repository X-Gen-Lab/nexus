/**
 * \file            g474_exti_model.h
 * \brief           External EXTI register and vector acknowledgement boundary
 * \author          Nexus Team
 */
#ifndef NX_G474_EXTI_MODEL_H
#define NX_G474_EXTI_MODEL_H
#include "g474_factory_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern EXTI_TypeDef g_g474_exti;
extern SYSCFG_TypeDef g_g474_exti_mux;
uint32_t nx_g474_exti_model_read(volatile uint32_t* reg);
void nx_g474_exti_model_write(volatile uint32_t* reg, uint32_t value);
nx_result_t nx_g474_exti_model_irq_control(int irq, bool enabled,
                                           uint8_t priority);
#ifdef __cplusplus
}
#endif
#undef EXTI
#undef SYSCFG
#undef NX_G474_READ
#undef NX_G474_WRITE
#undef NX_G474_IRQ_CONTROL
#define EXTI                      (&g_g474_exti)
#define SYSCFG                    (&g_g474_exti_mux)
#define NX_G474_READ(reg)         nx_g474_exti_model_read(reg)
#define NX_G474_WRITE(reg, value) nx_g474_exti_model_write((reg), (value))
#define NX_G474_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_g474_exti_model_irq_control((irq), (enabled), (priority))
#endif
