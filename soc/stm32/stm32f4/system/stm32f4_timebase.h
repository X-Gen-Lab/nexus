/**
 * \file            stm32f4_timebase.h
 * \brief           Selected F4 system timer role and its cold binding checks
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_TIMEBASE_H
#define NX_STM32F4_TIMEBASE_H

#include "nexus_config.h"
#include "stm32f4xx.h"
#include <stdbool.h>

/* Register models explicitly retain the reviewed TIM2 fixture. Production
 * configuration must supply the complete role from its selected package. */
#ifndef NEXUS_STM32_TIMEBASE_TIMER
#if defined(NEXUS_CMSIS_HOST_MODEL) || defined(NEXUS_STM32_MODEL) ||           \
    defined(NEXUS_TEST_F4_CORE_CM4_H)
#define NEXUS_STM32_TIMEBASE_TIMER        TIM2
#define NEXUS_STM32_TIMEBASE_IRQ          TIM2_IRQn
#define NEXUS_STM32_TIMEBASE_GATE         RCC_APB1ENR_TIM2EN
#define NEXUS_STM32_TIMEBASE_RESET        RCC_APB1RSTR_TIM2RST
#define NEXUS_STM32_TIMEBASE_HANDLER      TIM2_IRQHandler
#define NEXUS_STM32_TIMEBASE_COUNTER_BITS 32U
#else
#error "STM32F4 requires a complete selected system timer role"
#endif
#endif

#if !defined(NEXUS_STM32_TIMEBASE_IRQ) ||                                      \
    !defined(NEXUS_STM32_TIMEBASE_GATE) ||                                     \
    !defined(NEXUS_STM32_TIMEBASE_RESET) ||                                    \
    !defined(NEXUS_STM32_TIMEBASE_HANDLER) ||                                  \
    !defined(NEXUS_STM32_TIMEBASE_COUNTER_BITS)
#error "STM32F4 system timer role is incomplete"
#endif
#if NEXUS_STM32_TIMEBASE_COUNTER_BITS != 32U
#error "STM32F4 system timer requires a reviewed 32-bit counter"
#endif

/**
 * \brief Validate the immutable timer, clock gate, reset and vector tuple.
 * \return True for one reviewed TIM2 or TIM5 APB1 system timer role.
 * \note Bounded pure admission, with no state, access to MMIO or side effects.
 *       The selected constants let the compiler remove this check. A matching
 *       tuple does not authorize a Board route or an independent timer owner.
 */
static inline bool nx_stm32_timebase_binding_valid(void) {
    return (NEXUS_STM32_TIMEBASE_TIMER == TIM2 &&
            NEXUS_STM32_TIMEBASE_IRQ == TIM2_IRQn &&
            NEXUS_STM32_TIMEBASE_GATE == RCC_APB1ENR_TIM2EN &&
            NEXUS_STM32_TIMEBASE_RESET == RCC_APB1RSTR_TIM2RST) ||
           (NEXUS_STM32_TIMEBASE_TIMER == TIM5 &&
            NEXUS_STM32_TIMEBASE_IRQ == TIM5_IRQn &&
            NEXUS_STM32_TIMEBASE_GATE == RCC_APB1ENR_TIM5EN &&
            NEXUS_STM32_TIMEBASE_RESET == RCC_APB1RSTR_TIM5RST);
}

#endif /* NX_STM32F4_TIMEBASE_H */
