/**
 * \file            compiler.h
 *
 * \brief           Private inline Cortex-M instruction and register boundary.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_CORTEX_M_COMPILER_H
#define NEXUS_ARCH_CORTEX_M_COMPILER_H
#include <stdint.h>

#if !(defined(__GNUC__) || defined(__clang__))
#error "Nexus Cortex-M primitives require GCC or Clang"
#endif
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) ||                   \
    defined(__ARM_ARCH_8M_MAIN__) || defined(__ARM_ARCH_8_1M_MAIN__)
#define NX_ARCH_CORTEX_M_PRIORITY_MASK 1
#elif defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_8M_BASE__)
#define NX_ARCH_CORTEX_M_PRIORITY_MASK 0
#else
#error "Nexus Cortex-M primitives require a maintained architecture profile"
#endif

#ifndef NEXUS_ARCH_HAS_DWT_CYCCNT
#error "Nexus Cortex-M primitives require an explicit reviewed DWT capability"
#elif NEXUS_ARCH_HAS_DWT_CYCCNT != 0 && NEXUS_ARCH_HAS_DWT_CYCCNT != 1
#error "Nexus DWT capability must be zero or one"
#elif NEXUS_ARCH_HAS_DWT_CYCCNT && !NX_ARCH_CORTEX_M_PRIORITY_MASK
#error "Nexus does not maintain DWT CYCCNT on a Baseline Cortex-M profile"
#endif
/* Unknown preprocessor identifiers evaluate to zero in #if expressions. The
 * C check rejects those tokens instead of silently selecting disabled DWT. */
_Static_assert(NEXUS_ARCH_HAS_DWT_CYCCNT == 0 || NEXUS_ARCH_HAS_DWT_CYCCNT == 1,
               "Nexus DWT capability must be zero or one");

#define NX_ARCH_HW_INLINE static inline __attribute__((always_inline))

#ifdef NEXUS_ARCH_CORTEX_M_MODEL
#include "arch_cortex_model.h"
NX_ARCH_HW_INLINE uint32_t nx_arch_hw_irq_save(void) {
    uint32_t previous = nx_arch_model_read(NX_ARCH_MODEL_PRIMASK);
    nx_arch_model_instruction(NX_ARCH_MODEL_CPSID_I);
    nx_arch_model_instruction(NX_ARCH_MODEL_DSB);
    nx_arch_model_instruction(NX_ARCH_MODEL_ISB);
    return previous;
}
NX_ARCH_HW_INLINE void nx_arch_hw_irq_restore(uint32_t previous) {
    nx_arch_model_instruction(NX_ARCH_MODEL_DSB);
    nx_arch_model_write(NX_ARCH_MODEL_PRIMASK, previous);
    nx_arch_model_instruction(NX_ARCH_MODEL_ISB);
}
#define NX_ARCH_HW_READ(name, reg)                                             \
    NX_ARCH_HW_INLINE uint32_t nx_arch_hw_##name(void) {                       \
        return nx_arch_model_read(NX_ARCH_MODEL_##reg);                        \
    }
NX_ARCH_HW_READ(primask, PRIMASK)
NX_ARCH_HW_READ(ipsr, IPSR)
NX_ARCH_HW_READ(control, CONTROL)
#if NX_ARCH_CORTEX_M_PRIORITY_MASK
NX_ARCH_HW_READ(basepri, BASEPRI)
NX_ARCH_HW_READ(faultmask, FAULTMASK)
#endif
#if NEXUS_ARCH_HAS_DWT_CYCCNT
NX_ARCH_HW_READ(demcr, DEMCR)
NX_ARCH_HW_READ(dwt_control, DWT_CONTROL)
NX_ARCH_HW_READ(cyccnt, CYCCNT)
#endif
#undef NX_ARCH_HW_READ
#define NX_ARCH_HW_BARRIER(name, instruction)                                  \
    NX_ARCH_HW_INLINE void nx_arch_hw_##name(void) {                           \
        nx_arch_model_instruction(NX_ARCH_MODEL_##instruction);                \
    }
NX_ARCH_HW_BARRIER(dmb, DMB)
NX_ARCH_HW_BARRIER(dsb, DSB)
NX_ARCH_HW_BARRIER(isb, ISB)
#undef NX_ARCH_HW_BARRIER
#else
NX_ARCH_HW_INLINE uint32_t nx_arch_hw_irq_save(void) {
    uint32_t previous;
    __asm__ volatile("mrs %0, primask\n\tcpsid i\n\tdsb\n\tisb"
                     : "=r"(previous)::"memory");
    return previous;
}
NX_ARCH_HW_INLINE void nx_arch_hw_irq_restore(uint32_t previous) {
    __asm__ volatile("dsb\n\tmsr primask, %0\n\tisb" ::"r"(previous)
                     : "memory");
}
#define NX_ARCH_HW_READ(name, reg)                                             \
    NX_ARCH_HW_INLINE uint32_t nx_arch_hw_##name(void) {                       \
        uint32_t value;                                                        \
        __asm__ volatile("mrs %0, " #reg : "=r"(value)::"memory");             \
        return value;                                                          \
    }
NX_ARCH_HW_READ(primask, primask)
NX_ARCH_HW_READ(ipsr, ipsr)
NX_ARCH_HW_READ(control, control)
#if NX_ARCH_CORTEX_M_PRIORITY_MASK
NX_ARCH_HW_READ(basepri, basepri)
NX_ARCH_HW_READ(faultmask, faultmask)
#endif
#undef NX_ARCH_HW_READ
NX_ARCH_HW_INLINE void nx_arch_hw_dmb(void) {
    __asm__ volatile("dmb" ::: "memory");
}
NX_ARCH_HW_INLINE void nx_arch_hw_dsb(void) {
    __asm__ volatile("dsb" ::: "memory");
}
NX_ARCH_HW_INLINE void nx_arch_hw_isb(void) {
    __asm__ volatile("isb" ::: "memory");
}
#if NEXUS_ARCH_HAS_DWT_CYCCNT
NX_ARCH_HW_INLINE uint32_t nx_arch_hw_demcr(void) {
    return *(volatile const uint32_t*)0xe000edfcu;
}
NX_ARCH_HW_INLINE uint32_t nx_arch_hw_dwt_control(void) {
    return *(volatile const uint32_t*)0xe0001000u;
}
NX_ARCH_HW_INLINE uint32_t nx_arch_hw_cyccnt(void) {
    return *(volatile const uint32_t*)0xe0001004u;
}
#endif
#endif
#undef NX_ARCH_HW_INLINE
#endif
