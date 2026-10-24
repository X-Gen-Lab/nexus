/**
 * \file            nx_arch_cache_control.c
 * \brief           Explicit M7 cache admission and bounded cold withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/cache_control.h"
#include "private/mechanisms.h"
#include <stddef.h>

#ifndef NEXUS_ARCH_CACHE_CONTROL_M7
#define NEXUS_ARCH_CACHE_CONTROL_M7 0
#endif
_Static_assert(NEXUS_ARCH_CACHE_CONTROL_M7 == 0 ||
                   NEXUS_ARCH_CACHE_CONTROL_M7 == 1,
               "Reviewed M7 cache control selection must be zero or one");
#define NX_CACHE_BITS (NX_ARCH_CACHE_DATA | NX_ARCH_CACHE_INSTRUCTION)
#if NEXUS_ARCH_CACHE_CONTROL_M7 && NEXUS_ARCH_DCACHE_LINE_BYTES == 32 &&       \
    NEXUS_ARCH_ICACHE_LINE_BYTES == 32 && NEXUS_ARCH_SECURITY_STATE == 0
#include "private/cache_control.h"
#define NX_CACHE_CONTROL_REVIEWED 1
#define NX_CACHE_CPUID            ((uintptr_t)0xe000ed00U)
#define NX_CACHE_CCR              ((uintptr_t)0xe000ed14U)
#define NX_CACHE_CLIDR            ((uintptr_t)0xe000ed78U)
#define NX_CACHE_CCSIDR           ((uintptr_t)0xe000ed80U)
#define NX_CACHE_CSSELR           ((uintptr_t)0xe000ed84U)
#define NX_CACHE_ICIALLU          ((uintptr_t)0xe000ef50U)
#define NX_CACHE_DCISW            ((uintptr_t)0xe000ef60U)
#define NX_CACHE_DC               (1U << 16)
#define NX_CACHE_IC               (1U << 17)
#define NX_CACHE_CCR_BITS         (NX_CACHE_DC | NX_CACHE_IC)

static bool cache_context(void) {
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U)
        return false;
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    return masks.primask == 1U && masks.basepri == 0U && masks.faultmask == 0U;
}

static bool restore_selector(nx_arch_cache_control_t* control) {
    if (!control->selector_pending)
        return true;
    nx_arch_hw_mmio_write(NX_CACHE_CSSELR, control->saved_selector);
    nx_arch_dsb();
    nx_arch_isb();
    if (nx_arch_hw_mmio_read(NX_CACHE_CSSELR) != control->saved_selector)
        return false;
    control->selector_pending = false;
    return true;
}

static nx_arch_cache_control_result_t
cache_geometry(nx_arch_cache_control_t* control, uint32_t mask,
               uint32_t* last_set) {
    /* M7 has one cache level with separate instruction/data caches. Do not
     * interpret M55/M85 or another implementer's optional register layout. */
    if ((nx_arch_hw_mmio_read(NX_CACHE_CPUID) & 0xff0ffff0U) != 0x410fc270U ||
        nx_arch_hw_mmio_read(NX_CACHE_CLIDR) != 0x09000003U)
        return NX_ARCH_CACHE_UNSUPPORTED;
    if (!control->selector_pending)
        control->saved_selector = nx_arch_hw_mmio_read(NX_CACHE_CSSELR);
    nx_arch_cache_control_result_t result = NX_ARCH_CACHE_OK;
    for (uint32_t selector = 0U; selector != 2U; ++selector) {
        uint32_t selected_bit =
            selector == 0U ? NX_ARCH_CACHE_DATA : NX_ARCH_CACHE_INSTRUCTION;
        if ((mask & selected_bit) == 0U)
            continue;
        control->selector_pending = true;
        nx_arch_hw_mmio_write(NX_CACHE_CSSELR, selector);
        nx_arch_dsb();
        nx_arch_isb();
        if (nx_arch_hw_mmio_read(NX_CACHE_CSSELR) != selector) {
            result = NX_ARCH_CACHE_IO;
            break;
        }
        uint32_t geometry = nx_arch_hw_mmio_read(NX_CACHE_CCSIDR);
        uint32_t ways = ((geometry >> 3) & 0x3ffU) + 1U;
        uint32_t sets = ((geometry >> 13) & 0x7fffU) + 1U;
        uint32_t required_ways = selector == 0U ? 4U : 2U;
        uint32_t minimum_sets = selector == 0U ? 32U : 64U;
        uint32_t maximum_sets = selector == 0U ? 512U : 1024U;
        if ((geometry & 7U) != 1U || ways != required_ways ||
            sets < minimum_sets || sets > maximum_sets ||
            (sets & (sets - 1U)) != 0U) {
            result = NX_ARCH_CACHE_UNSUPPORTED;
            break;
        }
        if (selector == 0U)
            *last_set = sets - 1U;
    }
    /* Preserve the original selector even after failed selection/validation.
     * A failed restore retains its original value for withdrawal retry. */
    return restore_selector(control) ? result : NX_ARCH_CACHE_IO;
}

static bool write_ccr(uint32_t value) {
    nx_arch_hw_mmio_write(NX_CACHE_CCR, value);
    nx_arch_dsb();
    nx_arch_isb();
    return nx_arch_hw_mmio_read(NX_CACHE_CCR) == value;
}

static uint32_t hardware_bits(uint32_t mask) {
    return ((mask & NX_ARCH_CACHE_DATA) != 0U ? NX_CACHE_DC : 0U) |
           ((mask & NX_ARCH_CACHE_INSTRUCTION) != 0U ? NX_CACHE_IC : 0U);
}
#endif

nx_arch_cache_control_result_t
nx_arch_cache_cold_enable(nx_arch_cache_control_t* control, uint32_t mask) {
    if (control == NULL || mask == 0U || (mask & ~NX_CACHE_BITS) != 0U)
        return NX_ARCH_CACHE_INVALID;
#ifdef NX_CACHE_CONTROL_REVIEWED
    if (control->owned_mask != 0U || control->selector_pending)
        return NX_ARCH_CACHE_STATE;
    if (!cache_context())
        return NX_ARCH_CACHE_CONTEXT;
    uint32_t ccr = nx_arch_hw_mmio_read(NX_CACHE_CCR);
    if ((ccr & NX_CACHE_CCR_BITS) != 0U)
        return NX_ARCH_CACHE_STATE;
    uint32_t last_set = 0U;
    control->owned_mask = mask;
    nx_arch_cache_control_result_t result =
        cache_geometry(control, mask, &last_set);
    if (result != NX_ARCH_CACHE_OK) {
        if (!control->selector_pending)
            control->owned_mask = 0U;
        return result;
    }
    if ((mask & NX_ARCH_CACHE_DATA) != 0U) {
        nx_arch_dsb();
        for (uint32_t sets = last_set + 1U; sets-- != 0U;) {
            for (uint32_t ways = 4U; ways-- != 0U;)
                nx_arch_hw_mmio_write(NX_CACHE_DCISW,
                                      (sets << 5) | (ways << 30));
        }
        nx_arch_dsb();
        ccr |= NX_CACHE_DC;
        if (!write_ccr(ccr))
            return NX_ARCH_CACHE_IO;
    }
    if ((mask & NX_ARCH_CACHE_INSTRUCTION) != 0U) {
        nx_arch_dsb();
        nx_arch_isb();
        nx_arch_hw_mmio_write(NX_CACHE_ICIALLU, 0U);
        nx_arch_dsb();
        nx_arch_isb();
        ccr |= NX_CACHE_IC;
        if (!write_ccr(ccr))
            return NX_ARCH_CACHE_IO;
    }
    return NX_ARCH_CACHE_OK;
#else
    return NX_ARCH_CACHE_UNSUPPORTED;
#endif
}

nx_arch_cache_control_result_t
nx_arch_cache_cold_disable(nx_arch_cache_control_t* control) {
    if (control == NULL || (control->owned_mask & ~NX_CACHE_BITS) != 0U)
        return NX_ARCH_CACHE_INVALID;
#ifdef NX_CACHE_CONTROL_REVIEWED
    if (control->owned_mask == 0U && !control->selector_pending)
        return NX_ARCH_CACHE_OK;
    if (!cache_context())
        return NX_ARCH_CACHE_CONTEXT;
    uint32_t ccr = nx_arch_hw_mmio_read(NX_CACHE_CCR);
    if ((ccr & NX_CACHE_CCR_BITS & ~hardware_bits(control->owned_mask)) != 0U)
        return NX_ARCH_CACHE_STATE;
    uint32_t active_mask =
        ((ccr & NX_CACHE_DC) != 0U ? NX_ARCH_CACHE_DATA : 0U) |
        ((ccr & NX_CACHE_IC) != 0U ? NX_ARCH_CACHE_INSTRUCTION : 0U);
    uint32_t last_set = 0U;
    nx_arch_cache_control_result_t result =
        cache_geometry(control, active_mask, &last_set);
    if (result != NX_ARCH_CACHE_OK)
        return result;
    uint32_t cold_ccr = ccr & ~hardware_bits(control->owned_mask);
    if ((ccr & NX_CACHE_DC) != 0U) {
        /* A C loop could dirty its own stack after cleaning that line. This
         * leaf keeps every counter in caller-saved registers until DC is off.
         */
        if (nx_arch_cache_clean_disable_leaf(last_set << 5, cold_ccr) !=
            cold_ccr)
            return NX_ARCH_CACHE_IO;
    } else if ((ccr & NX_CACHE_IC) != 0U) {
        nx_arch_dsb();
        nx_arch_isb();
        if (!write_ccr(cold_ccr))
            return NX_ARCH_CACHE_IO;
    }
    if ((ccr & NX_CACHE_IC) != 0U) {
        nx_arch_hw_mmio_write(NX_CACHE_ICIALLU, 0U);
        nx_arch_dsb();
        nx_arch_isb();
    }
    control->owned_mask = 0U;
    return NX_ARCH_CACHE_OK;
#else
    return NX_ARCH_CACHE_UNSUPPORTED;
#endif
}
