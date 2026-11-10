/**
 * \file            stm32_window_watchdog_model.h
 * \brief           Compile-time WWDG write faults; no firmware dispatch
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_WINDOW_WATCHDOG_MODEL_H
#define NEXUS_STM32_WINDOW_WATCHDOG_MODEL_H
#include "stm32f4xx.h"
#ifdef __cplusplus
extern "C" {
#endif
extern DBGMCU_TypeDef nx_wwdg_model_debug;
extern NVIC_Type nx_wwdg_model_nvic;
void nx_stm32_window_watchdog_model_write(volatile uint32_t* address,
                                          uint32_t value);
#ifdef __cplusplus
}
#endif
/* Architectural addresses occupy ASan's shadow gap on the host. Keep the
 * vendor register layouts; only these external hardware reads are substituted.
 */
#undef DBGMCU
#undef NVIC
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_SetPriority
#undef NVIC_GetPriority
#undef NVIC_EnableIRQ
#define DBGMCU (&nx_wwdg_model_debug)
#define NVIC   (&nx_wwdg_model_nvic)
#define NVIC_GetEnableIRQ(irq)                                                 \
    ((NVIC->ISER[(unsigned)(irq) / 32U] >> ((unsigned)(irq) % 32U)) & 1U)
#define NVIC_GetPendingIRQ(irq)                                                \
    ((NVIC->ISPR[(unsigned)(irq) / 32U] >> ((unsigned)(irq) % 32U)) & 1U)
#define NVIC_GetActive(irq)                                                    \
    ((NVIC->IABR[(unsigned)(irq) / 32U] >> ((unsigned)(irq) % 32U)) & 1U)
#define NVIC_SetPriority(irq, value)                                           \
    (NVIC->IPR[(unsigned)(irq)] = (uint8_t)((value) << 4U))
#define NVIC_GetPriority(irq) (NVIC->IPR[(unsigned)(irq)] >> 4U)
#define NVIC_EnableIRQ(irq)                                                    \
    (NVIC->ISER[(unsigned)(irq) / 32U] |= UINT32_C(1)                          \
                                          << ((unsigned)(irq) % 32U))
#define NX_STM32_WWDG_WRITE(address, value)                                    \
    nx_stm32_window_watchdog_model_write((address), (value))
#endif
