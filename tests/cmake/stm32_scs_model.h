/**
 * \file            stm32_scs_model.h
 * \brief           Selected NVIC and SCB hardware ports in addressable host RAM
 * \author          Nexus Team
 * \note            The real Cortex-M SCS address lies in the ASan shadow gap.
 *                  Keep the official register layouts and bit observations;
 *                  only the external register backing changes in host tests.
 */
#ifndef NEXUS_TEST_STM32_SCS_MODEL_H
#define NEXUS_TEST_STM32_SCS_MODEL_H

#include "stm32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern uint8_t g_nx_stm32_scs_model[0x2000U];
#ifdef __cplusplus
}
#endif

#undef NVIC
#undef SCB
#define NVIC ((NVIC_Type*)(void*)(g_nx_stm32_scs_model + 0x100U))
#define SCB  ((SCB_Type*)(void*)(g_nx_stm32_scs_model + 0xD00U))

/* Core inlines were declared before the host port. Redirect their selected
 * read entry points too; a backing macro alone cannot change compiled inlines.
 */
static inline uint32_t nx_stm32_scs_enabled(IRQn_Type irq) {
    return (int32_t)irq < 0
               ? 0U
               : (NVIC->ISER[(uint32_t)irq / 32U] >> ((uint32_t)irq % 32U)) &
                     1U;
}
static inline uint32_t nx_stm32_scs_pending(IRQn_Type irq) {
    return (int32_t)irq < 0
               ? 0U
               : (NVIC->ISPR[(uint32_t)irq / 32U] >> ((uint32_t)irq % 32U)) &
                     1U;
}
static inline uint32_t nx_stm32_scs_active(IRQn_Type irq) {
    return (int32_t)irq < 0
               ? 0U
               : (NVIC->IABR[(uint32_t)irq / 32U] >> ((uint32_t)irq % 32U)) &
                     1U;
}
static inline uint32_t nx_stm32_scs_priority(IRQn_Type irq) {
    uint32_t value = (int32_t)irq >= 0 ? NVIC->IPR[(uint32_t)irq]
                                       : SCB->SHPR[((uint32_t)irq & 0xFU) - 4U];
    return value >> (8U - __NVIC_PRIO_BITS);
}
static inline uint32_t nx_stm32_scs_group(void) {
    return (SCB->AIRCR >> SCB_AIRCR_PRIGROUP_Pos) & 7U;
}
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_GetPriority
#undef NVIC_GetPriorityGrouping
#define NVIC_GetEnableIRQ(irq)     nx_stm32_scs_enabled(irq)
#define NVIC_GetPendingIRQ(irq)    nx_stm32_scs_pending(irq)
#define NVIC_GetActive(irq)        nx_stm32_scs_active(irq)
#define NVIC_GetPriority(irq)      nx_stm32_scs_priority(irq)
#define NVIC_GetPriorityGrouping() nx_stm32_scs_group()

#endif
