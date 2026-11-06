/**
 * \file            hardware.c
 * \brief           Official H747 HSEM leaves and observed shared MPU admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "hardware.h"
#include "nexus/arch/arch.h"
#include "stm32h7xx.h"
#ifdef NX_H747_AMP_MODEL
#include "h747_amp_hardware_model.h"
#define NX_H747_READ(reg)         nx_h747_amp_model_read(&(reg))
#define NX_H747_WRITE(reg, value) nx_h747_amp_model_write(&(reg), (value))
#undef WRITE_REG
#undef READ_REG
#undef READ_BIT
#define WRITE_REG(reg, value) NX_H747_WRITE(reg, value)
#define READ_REG(reg)         NX_H747_READ(reg)
#define READ_BIT(reg, mask)   (NX_H747_READ(reg) & (mask))
#else
#define NX_H747_READ(reg)         (reg)
#define NX_H747_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_hsem.h"
#if !defined(STM32H747xx) || !defined(DUAL_CORE)
#error "This port requires the exact STM32H747XI dual-core device header"
#endif
#if (defined(CORE_CM7) + defined(CORE_CM4)) != 1
#error "Select exactly one H747 execution core"
#endif
_Static_assert(HSEM1_IRQn == 125 && HSEM2_IRQn == 126,
               "Exact H747 notification vector identities");
_Static_assert(D3_SRAM_BASE == UINT32_C(0x38000000),
               "Exact H747 SRAM4 global window");

/** \brief Read one enabled, whole region without trusting a requested selector.
 */
static bool region_read(uint32_t region, uint32_t* base, uint32_t* rasr) {
    NX_H747_WRITE(MPU->RNR, region);
    nx_arch_dsb();
    if (NX_H747_READ(MPU->RNR) != region)
        return false;
    *base = NX_H747_READ(MPU->RBAR) & UINT32_C(0xFFFFFFE0);
    *rasr = NX_H747_READ(MPU->RASR);
    return true;
}
/** \brief A later MPU priority must not override any admitted shared byte. */
static bool overrides(uint32_t region, uint32_t first, uint32_t bytes) {
    uint32_t base, rasr;
    if (!region_read(region, &base, &rasr))
        return true;
    if ((rasr & MPU_RASR_ENABLE_Msk) == 0U)
        return false;
    uint32_t size = (rasr & (uint32_t)MPU_RASR_SIZE_Msk) >> MPU_RASR_SIZE_Pos;
    if (size < 4U || size >= 31U)
        return true;
    uint32_t region_bytes = UINT32_C(1) << (size + 1U);
    if ((base & (region_bytes - 1U)) != 0U)
        return true;
    /* Conservative: even disabled subregions do not authorize an overlapping
     * higher-priority mapping in this first exact noncacheable mode. */
    return (uint64_t)base < (uint64_t)first + bytes &&
           (uint64_t)first < (uint64_t)base + region_bytes;
}
bool nx_h747_amp_hw_ready(const void* address, size_t bytes, uint8_t region) {
#ifdef CORE_CM7
    /* Fresh MPU attributes do not prove that old dirty lines were cleaned.
     * First mode keeps D-cache disabled through both final joins, without
     * aliases or an MPU transition while either endpoint owns the window. */
    if ((NX_H747_READ(SCB->CCR) & SCB_CCR_DC_Msk) != 0U)
        return false;
#endif
    uintptr_t first = (uintptr_t)address;
    if (first < D3_SRAM_BASE || bytes == 0U || bytes > 65536U ||
        first - D3_SRAM_BASE > 65536U - bytes ||
        LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_HSEM) == 0U ||
        (NX_H747_READ(MPU->CTRL) & MPU_CTRL_ENABLE_Msk) == 0U)
        return false;
    uint32_t count =
        (NX_H747_READ(MPU->TYPE) & (uint32_t)MPU_TYPE_DREGION_Msk) >>
        MPU_TYPE_DREGION_Pos;
    if (region >= count)
        return false;
    uint32_t saved = NX_H747_READ(MPU->RNR);
    uint32_t base, rasr;
    bool valid = region_read(region, &base, &rasr);
    uint32_t size =
        valid ? (rasr & (uint32_t)MPU_RASR_SIZE_Msk) >> MPU_RASR_SIZE_Pos : 0U;
    const uint32_t attributes = MPU_RASR_XN_Msk | (3U << MPU_RASR_AP_Pos) |
                                (1U << MPU_RASR_TEX_Pos) | MPU_RASR_S_Msk;
    const uint32_t mask = MPU_RASR_XN_Msk | MPU_RASR_AP_Msk | MPU_RASR_TEX_Msk |
                          MPU_RASR_S_Msk | MPU_RASR_C_Msk | MPU_RASR_B_Msk |
                          MPU_RASR_SRD_Msk;
    valid = valid && (rasr & MPU_RASR_ENABLE_Msk) != 0U && size < 31U &&
            (UINT32_C(1) << (size + 1U)) == bytes && base == first &&
            (rasr & mask) == attributes;
    for (uint32_t higher = (uint32_t)region + 1U; valid && higher < count;
         ++higher)
        valid = !overrides(higher, (uint32_t)first, (uint32_t)bytes);
    NX_H747_WRITE(MPU->RNR, saved);
    nx_arch_dsb();
    return valid && NX_H747_READ(MPU->RNR) == saved;
}
bool nx_h747_amp_hw_take(uint8_t semaphore, uint8_t process) {
    if (semaphore > 31U || process == 0U ||
        LL_HSEM_IsSemaphoreLocked(HSEM, semaphore) != 0U)
        return false;
    return LL_HSEM_2StepLock(HSEM, semaphore, process) == 0U;
}
bool nx_h747_amp_hw_release(uint8_t semaphore, uint8_t process) {
    if (semaphore > 31U || process == 0U)
        return false;
    LL_HSEM_ReleaseLock(HSEM, semaphore, process);
    nx_arch_dsb();
    return NX_H747_READ(HSEM->R[semaphore]) !=
           (HSEM_R_LOCK | LL_HSEM_COREID | (uint32_t)process);
}
