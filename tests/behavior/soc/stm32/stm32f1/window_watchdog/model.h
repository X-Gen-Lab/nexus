/**
 * \file            model.h
 * \brief           F103 WWDG register access interception
 * \author          Nexus Team
 */
#ifndef NEXUS_F103_WWDG_MODEL_H
#define NEXUS_F103_WWDG_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef f103_wwdg_rcc;
extern WWDG_TypeDef f103_wwdg_regs;
void f103_wwdg_write(volatile uint32_t* reg, uint32_t value);
uint32_t f103_wwdg_vector(IRQn_Type irq);
#ifdef __cplusplus
}
#endif

#undef RCC
#undef WWDG
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#define RCC                          (&f103_wwdg_rcc)
#define WWDG                         (&f103_wwdg_regs)
#define NVIC_GetEnableIRQ(irq)       (f103_wwdg_vector(irq) & 1U)
#define NVIC_GetPendingIRQ(irq)      (f103_wwdg_vector(irq) & 2U)
#define NVIC_GetActive(irq)          (f103_wwdg_vector(irq) & 4U)
#define NX_STM32F1_WRITE(reg, value) f103_wwdg_write((reg), (uint32_t)(value))
#endif
