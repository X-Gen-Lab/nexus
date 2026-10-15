/**
 * \file            mechanisms.h
 *
 * \brief           Private current-domain CPU register access boundary.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_CORTEX_M_MECHANISMS_H
#define NEXUS_ARCH_CORTEX_M_MECHANISMS_H
#include "nexus/arch/features.h"
#include <stdint.h>

#ifndef NEXUS_ARCH_DCACHE_LINE_BYTES
#error "Nexus requires reviewed data-cache line geometry"
#endif
#ifndef NEXUS_ARCH_ICACHE_LINE_BYTES
#error "Nexus requires reviewed instruction-cache line geometry"
#endif
#ifndef NEXUS_ARCH_MPU_VERSION
#error "Nexus requires a reviewed MPU version"
#endif
#ifndef NEXUS_ARCH_SECURITY_STATE
#error "Nexus requires an explicit firmware security state"
#endif
#ifndef NEXUS_ARCH_HAS_SAU
#error "Nexus requires an explicit SAU capability"
#endif
#ifndef NEXUS_ARCH_HAS_DWT_CYCCNT
#error "Nexus requires an explicit DWT capability"
#endif
_Static_assert(NEXUS_ARCH_HAS_DWT_CYCCNT == 0 || NEXUS_ARCH_HAS_DWT_CYCCNT == 1,
               "Nexus DWT capability must be zero or one");
_Static_assert(NEXUS_ARCH_DCACHE_LINE_BYTES == 0 ||
                   NEXUS_ARCH_DCACHE_LINE_BYTES == 32,
               "Nexus maintains zero or 32-byte data-cache lines");
_Static_assert(NEXUS_ARCH_ICACHE_LINE_BYTES == 0 ||
                   NEXUS_ARCH_ICACHE_LINE_BYTES == 32,
               "Nexus maintains zero or 32-byte instruction-cache lines");
_Static_assert(NEXUS_ARCH_MPU_VERSION == 0 || NEXUS_ARCH_MPU_VERSION == 7 ||
                   NEXUS_ARCH_MPU_VERSION == 8,
               "Nexus maintains no MPU, MPUv7 or MPUv8");
_Static_assert(NEXUS_ARCH_SECURITY_STATE >= 0 && NEXUS_ARCH_SECURITY_STATE <= 2,
               "Nexus requires single, Secure or NonSecure state");
_Static_assert(NEXUS_ARCH_HAS_SAU == 0 || NEXUS_ARCH_HAS_SAU == 1,
               "Nexus SAU capability must be zero or one");
#if NEXUS_ARCH_HAS_SAU && NEXUS_ARCH_SECURITY_STATE != 1
#error "Nexus SAU programming requires the Secure firmware state"
#endif
#if NEXUS_ARCH_SECURITY_STATE == 1 &&                                          \
    (!defined(__ARM_FEATURE_CMSE) || (__ARM_FEATURE_CMSE & 2) == 0)
#error "Nexus Secure mechanisms require the compiler Secure CMSE state"
#endif
#if NEXUS_ARCH_SECURITY_STATE == 2 && defined(__ARM_FEATURE_CMSE) &&           \
    (__ARM_FEATURE_CMSE & 2) != 0
#error "Nexus NonSecure mechanisms cannot use the compiler Secure CMSE state"
#endif

#ifndef NEXUS_ARCH_MECHANISM_MODEL
#if defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) ||                    \
    defined(__ARM_ARCH_7EM__)
#if NEXUS_ARCH_MPU_VERSION == 8 || NEXUS_ARCH_SECURITY_STATE != 0 ||           \
    NEXUS_ARCH_HAS_SAU
#error "Nexus v6/v7-M mechanisms require MPUv7 and single security state"
#endif
#if !defined(__ARM_ARCH_7EM__) &&                                              \
    (NEXUS_ARCH_DCACHE_LINE_BYTES != 0 || NEXUS_ARCH_ICACHE_LINE_BYTES != 0)
#error                                                                         \
    "Nexus v6-M/v7-M profiles do not provide the reviewed SCB cache interface"
#endif
#elif defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__) ||        \
    defined(__ARM_ARCH_8_1M_MAIN__)
#if NEXUS_ARCH_MPU_VERSION == 7
#error "Nexus v8-M mechanisms require MPUv8"
#endif
#if defined(__ARM_ARCH_8M_BASE__) &&                                           \
    (NEXUS_ARCH_DCACHE_LINE_BYTES != 0 || NEXUS_ARCH_ICACHE_LINE_BYTES != 0)
#error "Nexus v8-M Baseline does not provide the reviewed SCB cache interface"
#endif
#else
#if NEXUS_ARCH_HAS_DWT_CYCCNT || NEXUS_ARCH_DCACHE_LINE_BYTES != 0 ||          \
    NEXUS_ARCH_ICACHE_LINE_BYTES != 0 || NEXUS_ARCH_MPU_VERSION != 0 ||        \
    NEXUS_ARCH_SECURITY_STATE != 0 || NEXUS_ARCH_HAS_SAU
#error "Nexus host mechanisms require absent CPU hardware capabilities"
#endif
#endif
#endif

#ifdef NEXUS_ARCH_MECHANISM_MODEL
#include "arch_mechanism_model.h"
static inline uint32_t nx_arch_hw_mmio_read(uintptr_t address) {
    return nx_arch_model_mmio_read(address);
}
static inline void nx_arch_hw_mmio_write(uintptr_t address, uint32_t value) {
    nx_arch_model_mmio_write(address, value);
}
#else
#ifdef _MSC_VER
#define NX_ARCH_MMIO_INLINE static __forceinline
#else
#define NX_ARCH_MMIO_INLINE static inline __attribute__((always_inline))
#endif
NX_ARCH_MMIO_INLINE uint32_t nx_arch_hw_mmio_read(uintptr_t address) {
    return *(volatile const uint32_t*)address;
}
NX_ARCH_MMIO_INLINE void nx_arch_hw_mmio_write(uintptr_t address,
                                               uint32_t value) {
    *(volatile uint32_t*)address = value;
}
#undef NX_ARCH_MMIO_INLINE
#endif
#endif
