/**
 * \file            stm32f407_handoff_hw.h
 * \brief           Private F407 register and nonreturning instruction boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_HANDOFF_HW_H
#define NEXUS_STM32F407_HANDOFF_HW_H
#include <stdint.h>
#ifdef NX_F407_HANDOFF_MODEL
#include "stm32f407_handoff_model.h"
#define NX_F407_READ(reg)       nx_f407_handoff_model_read((uintptr_t) & (reg))
#define NX_F407_READ16(address) nx_f407_handoff_model_read16(address)
#define NX_F407_CONTROL()       nx_f407_handoff_model_control()
#define NX_F407_TAIL(vector, stack, pc)                                        \
    nx_f407_handoff_model_tail(vector, stack, pc)
#else
#if !defined(__ARM_ARCH_7EM__) || !defined(__ARM_ARCH_PROFILE) ||              \
    __ARM_ARCH_PROFILE != 'M'
#error "F407 handoff requires the selected Cortex-M4 compiler profile"
#endif
#define NX_F407_READ(reg)               ((uint32_t)(reg))
#define NX_F407_READ16(address)         (*(volatile const uint16_t*)(address))
/** \brief Observe MSP/privilege/FP ownership before touching target vectors. */
static inline uint32_t nx_f407_handoff_control(void) {
    uint32_t control;
    __asm__ volatile("mrs %0, control" : "=r"(control)::"memory");
    return control;
}
#define NX_F407_CONTROL()               nx_f407_handoff_control()
/**
 * \brief Complete transfer without stack accesses after replacing MSP.
 * \note r0=VTOR, r1=MSP, r2=Thumb PC under the standard AAPCS. A returning
 *       target faults through LR=0xffffffff instead of using the old frame.
 */
__attribute__((naked, noreturn, noinline)) static void
nx_f407_handoff_tail(uint32_t vector __attribute__((unused)),
                     uint32_t stack __attribute__((unused)),
                     uint32_t pc __attribute__((unused))) {
    __asm__ volatile("ldr r3, =0xe000ed08\n\t"
                     "str r0, [r3]\n\t"
                     "dsb\n\t"
                     "isb\n\t"
                     "movs r0, #0\n\t"
                     "ldr r3, =0xe000ed88\n\t"
                     "str r0, [r3]\n\t"
                     "dsb\n\t"
                     "isb\n\t"
                     "msr control, r0\n\t"
                     "msr psp, r0\n\t"
                     "isb\n\t"
                     "mvns r3, r0\n\t"
                     "mov lr, r3\n\t"
                     "msr msp, r1\n\t"
                     "bx r2\n\t");
}
#define NX_F407_TAIL(vector, stack, pc) nx_f407_handoff_tail(vector, stack, pc)
#endif
#endif /* NEXUS_STM32F407_HANDOFF_HW_H */
