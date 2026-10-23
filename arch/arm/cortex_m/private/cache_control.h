/**
 * \file            cache_control.h
 * \brief           Stack-free M7 data-cache clean and CCR transition leaf
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_PRIVATE_CACHE_CONTROL_H
#define NEXUS_ARCH_PRIVATE_CACHE_CONTROL_H
#include <stdint.h>
#ifdef NEXUS_ARCH_MECHANISM_MODEL
#include "arch_cache_control_model.h"
static inline uint32_t nx_arch_cache_clean_disable_leaf(uint32_t last_set,
                                                        uint32_t ccr) {
    return nx_arch_cache_control_model_clean_disable(last_set, ccr);
}
#else
#if !defined(__ARM_ARCH_7EM__) || !defined(__GNUC__)
#error "Reviewed M7 cache control requires the maintained GNU ARM profile"
#endif
/**
 * \brief Clean every verified set/way, then disable with no dirty stack gap.
 * \note Inputs are in r0/r1, scratch is caller-saved r2/r3/r12. No stack, C
 *       call or data-memory access other than SCB occurs before CCR readback.
 *       DCCISW publishes dirty lines while DC is enabled; no CPU store can
 *       dirty a line between the final maintenance write and the CCR clear.
 *       The caller excludes DMA and all asynchronous memory users.
 */
__attribute__((naked, noinline)) static uint32_t
nx_arch_cache_clean_disable_leaf(uint32_t last_set __attribute__((unused)),
                                 uint32_t ccr __attribute__((unused))) {
    __asm volatile("dsb 0xf\n"
                   "mov r12, r0\n"
                   "movw r2, #0xef74\n"
                   "movt r2, #0xe000\n"
                   "1: mov r3, #0xc0000000\n"
                   "2: orr r0, r12, r3\n"
                   "str r0, [r2]\n"
                   "subs r3, r3, #0x40000000\n"
                   "bcs 2b\n"
                   "subs r12, r12, #32\n"
                   "bpl 1b\n"
                   "dsb 0xf\n"
                   "movw r2, #0xed14\n"
                   "movt r2, #0xe000\n"
                   "str r1, [r2]\n"
                   "dsb 0xf\n"
                   "isb 0xf\n"
                   "ldr r0, [r2]\n"
                   "bx lr\n");
}
#endif
#endif /* NEXUS_ARCH_PRIVATE_CACHE_CONTROL_H */
