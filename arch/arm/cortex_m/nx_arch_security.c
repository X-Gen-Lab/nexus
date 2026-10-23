/**
 * \file            nx_arch_security.c
 *
 * \brief           Validated current-domain SAU mechanisms without policy.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/security.h"
#include "private/mechanisms.h"

nx_arch_features_t nx_arch_features(void) {
    const nx_arch_features_t features = {
        NEXUS_ARCH_DCACHE_LINE_BYTES,
        NEXUS_ARCH_ICACHE_LINE_BYTES,
        NEXUS_ARCH_MPU_VERSION,
        NEXUS_ARCH_HAS_DWT_CYCCNT != 0,
        NEXUS_ARCH_HAS_SAU != 0,
        (nx_arch_security_state_t)NEXUS_ARCH_SECURITY_STATE};
    return features;
}

nx_arch_security_state_t nx_arch_security_state(void) {
    return (nx_arch_security_state_t)NEXUS_ARCH_SECURITY_STATE;
}

nx_arch_result_t nx_arch_sau_encode(const nx_arch_sau_region_t* region,
                                    nx_arch_sau_words_t* words) {
    if (region == 0 || words == 0 || (region->base & 31u) != 0 ||
        (region->limit & 31u) != 31u || region->base > region->limit) {
        return NX_ARCH_INVALID;
    }
    nx_arch_sau_words_t encoded = {
        region->base,
        (region->limit & ~31u) | (region->nonsecure_callable ? 2u : 0u) | 1u};
    *words = encoded;
    return NX_ARCH_OK;
}

#if NEXUS_ARCH_HAS_SAU
/* CMSIS core_cm23/33/55/85: SAU CTRL/TYPE/RNR/RBAR/RLAR at SCS+0xdd0.
 * The compiler and reviewed profile both require Secure state. There is no
 * NonSecure alias or write to global security attribution policy here. */
#define NX_SAU_CONTROL ((uintptr_t)0xe000edd0u)
#define NX_SAU_TYPE    (NX_SAU_CONTROL + 4u)
#define NX_SAU_RNR     (NX_SAU_CONTROL + 8u)
#define NX_SAU_RBAR    (NX_SAU_CONTROL + 12u)
#define NX_SAU_RLAR    (NX_SAU_CONTROL + 16u)

static nx_arch_result_t sau_validate(uint32_t index) {
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0 ||
        (nx_arch_irq_masks().primask & 1u) == 0) {
        return NX_ARCH_CONTEXT;
    }
    if (index >= (nx_arch_hw_mmio_read(NX_SAU_TYPE) & 255u)) {
        return NX_ARCH_INVALID;
    }
    if ((nx_arch_hw_mmio_read(NX_SAU_CONTROL) & 1u) != 0) {
        return NX_ARCH_CONTEXT;
    }
    return NX_ARCH_OK;
}

static uint32_t sau_select(uint32_t index) {
    uint32_t previous = nx_arch_hw_mmio_read(NX_SAU_RNR);
    nx_arch_dmb();
    nx_arch_hw_mmio_write(NX_SAU_RNR, index);
    nx_arch_hw_mmio_write(NX_SAU_RLAR, 0);
    return previous;
}

static void sau_complete(uint32_t previous) {
    nx_arch_hw_mmio_write(NX_SAU_RNR, previous);
    nx_arch_dsb();
    nx_arch_isb();
}
#endif

nx_arch_result_t nx_arch_sau_write(uint32_t index,
                                   const nx_arch_sau_region_t* region) {
#if NEXUS_ARCH_HAS_SAU
    nx_arch_sau_words_t words;
    nx_arch_result_t result = nx_arch_sau_encode(region, &words);
    if (result != NX_ARCH_OK) {
        return result;
    }
    result = sau_validate(index);
    if (result != NX_ARCH_OK) {
        return result;
    }
    uint32_t previous = sau_select(index);
    nx_arch_hw_mmio_write(NX_SAU_RBAR, words.rbar);
    nx_arch_hw_mmio_write(NX_SAU_RLAR, words.rlar);
    sau_complete(previous);
    return NX_ARCH_OK;
#else
    (void)index;
    (void)region;
    return NX_ARCH_UNSUPPORTED;
#endif
}

nx_arch_result_t nx_arch_sau_clear(uint32_t index) {
#if NEXUS_ARCH_HAS_SAU
    nx_arch_result_t result = sau_validate(index);
    if (result != NX_ARCH_OK) {
        return result;
    }
    uint32_t previous = sau_select(index);
    sau_complete(previous);
    return NX_ARCH_OK;
#else
    (void)index;
    return NX_ARCH_UNSUPPORTED;
#endif
}
