/**
 * \file            cold_context.h
 * \brief           Shared privileged unmasked cold-call predicate
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_PRIVATE_COLD_CONTEXT_H
#define NEXUS_STM32F4_PRIVATE_COLD_CONTEXT_H

#include "nexus/arch/arch.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

/** \brief All cold helpers require serialized privileged unmasked Thread mode.
 */
static inline bool nx_stm32_control_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

#ifdef __cplusplus
}
#endif
#endif
