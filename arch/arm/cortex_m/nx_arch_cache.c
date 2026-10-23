/**
 * \file            nx_arch_cache.c
 *
 * \brief           Owned CPU cache-line maintenance without range rounding.
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
#include "nexus/arch/cache.h"
#include "private/mechanisms.h"

#define NX_ARCH_CACHE_LINE_BYTES 32U
#define NX_ARCH_SCB_CCR          ((uintptr_t)0xe000ed14U)
#define NX_ARCH_ICIMVAU          ((uintptr_t)0xe000ef58U)
#define NX_ARCH_DCIMVAC          ((uintptr_t)0xe000ef5cU)
#define NX_ARCH_DCCMVAC          ((uintptr_t)0xe000ef68U)
#define NX_ARCH_DCCIMVAC         ((uintptr_t)0xe000ef70U)
#define NX_ARCH_CCR_DC           (1U << 16)
#define NX_ARCH_CCR_IC           (1U << 17)

static bool nx_arch_cache_range_valid(uintptr_t address, size_t bytes) {
    if (bytes == 0U || (address & (NX_ARCH_CACHE_LINE_BYTES - 1U)) != 0U ||
        (bytes & (NX_ARCH_CACHE_LINE_BYTES - 1U)) != 0U) {
        return false;
    }
#if UINTPTR_MAX > UINT32_MAX
    if (address > UINT32_MAX) {
        return false;
    }
#endif
    /* Subtract before comparing: the final CPU line ends at UINT32_MAX and
     * must not require an unrepresentable exclusive-end address. */
    return bytes - 1U <= UINT32_MAX - address;
}

#if NEXUS_ARCH_DCACHE_LINE_BYTES || NEXUS_ARCH_ICACHE_LINE_BYTES
static void nx_arch_cache_lines(uintptr_t operation, uintptr_t address,
                                size_t bytes) {
    do {
        nx_arch_hw_mmio_write(operation, (uint32_t)address);
        bytes -= NX_ARCH_CACHE_LINE_BYTES;
        if (bytes != 0U) {
            address += NX_ARCH_CACHE_LINE_BYTES;
        }
    } while (bytes != 0U);
}
#endif

static nx_arch_result_t
nx_arch_dcache_maintain(uintptr_t operation, uintptr_t address, size_t bytes) {
    if (!nx_arch_cache_range_valid(address, bytes)) {
        return NX_ARCH_INVALID;
    }
#if NEXUS_ARCH_DCACHE_LINE_BYTES
    if (!nx_arch_is_privileged()) {
        return NX_ARCH_CONTEXT;
    }
    if ((nx_arch_hw_mmio_read(NX_ARCH_SCB_CCR) & NX_ARCH_CCR_DC) == 0U) {
        return NX_ARCH_UNSUPPORTED;
    }
    nx_arch_dsb();
    nx_arch_cache_lines(operation, address, bytes);
    nx_arch_dsb();
    nx_arch_isb();
    return NX_ARCH_OK;
#else
    (void)operation;
    return NX_ARCH_UNSUPPORTED;
#endif
}

nx_arch_result_t nx_arch_dcache_clean(uintptr_t address, size_t bytes) {
    return nx_arch_dcache_maintain(NX_ARCH_DCCMVAC, address, bytes);
}

nx_arch_result_t nx_arch_dcache_invalidate(uintptr_t address, size_t bytes) {
    return nx_arch_dcache_maintain(NX_ARCH_DCIMVAC, address, bytes);
}

nx_arch_result_t nx_arch_dcache_clean_invalidate(uintptr_t address,
                                                 size_t bytes) {
    return nx_arch_dcache_maintain(NX_ARCH_DCCIMVAC, address, bytes);
}

nx_arch_result_t nx_arch_instruction_sync(uintptr_t address, size_t bytes) {
    if (!nx_arch_cache_range_valid(address, bytes)) {
        return NX_ARCH_INVALID;
    }
#if NEXUS_ARCH_DCACHE_LINE_BYTES || NEXUS_ARCH_ICACHE_LINE_BYTES
    if (!nx_arch_is_privileged()) {
        return NX_ARCH_CONTEXT;
    }
    uint32_t enabled = nx_arch_hw_mmio_read(NX_ARCH_SCB_CCR);
    uint32_t required = 0U;
#if NEXUS_ARCH_DCACHE_LINE_BYTES
    required |= NX_ARCH_CCR_DC;
#endif
#if NEXUS_ARCH_ICACHE_LINE_BYTES
    required |= NX_ARCH_CCR_IC;
#endif
    /* Reject before the first maintenance operation, including when only
     * one of two reviewed caches is disabled. Cache state stays caller-owned.
     */
    if ((enabled & required) != required) {
        return NX_ARCH_UNSUPPORTED;
    }
    nx_arch_dsb();
#if NEXUS_ARCH_DCACHE_LINE_BYTES
    nx_arch_cache_lines(NX_ARCH_DCCMVAC, address, bytes);
#if NEXUS_ARCH_ICACHE_LINE_BYTES
    /* Complete data publication before discarding the instruction copy. */
    nx_arch_dsb();
#endif
#endif
#if NEXUS_ARCH_ICACHE_LINE_BYTES
    nx_arch_cache_lines(NX_ARCH_ICIMVAU, address, bytes);
#endif
    nx_arch_dsb();
    nx_arch_isb();
    return NX_ARCH_OK;
#else
    return NX_ARCH_UNSUPPORTED;
#endif
}
