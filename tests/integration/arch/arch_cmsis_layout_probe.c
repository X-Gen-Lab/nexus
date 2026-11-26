/**
 * \file            arch_cmsis_layout_probe.c
 *
 * \brief           Independent pinned CMSIS layout checks for Arch facts.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-11-04
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include <stddef.h>
#include <stdint.h>

/* This is a CPU-only compile probe, not an invented Device or Board header. */
#define __NVIC_PRIO_BITS       4U
#define __Vendor_SysTickConfig 0U
#define __VTOR_PRESENT         1U
#define __MPU_PRESENT          1U
#define __SAUREGION_PRESENT    1U
#define __FPU_PRESENT          0U
#define __DSP_PRESENT          1U
#define __MVE_PRESENT          0U
#define __ICACHE_PRESENT       1U
#define __DCACHE_PRESENT       1U
typedef enum { Reset_IRQn = -15, SysTick_IRQn = -1 } IRQn_Type;
#include NX_ARCH_CMSIS_HEADER
#include "private/registers.h"

_Static_assert(__CM_CMSIS_VERSION_MAIN == 6 && __CM_CMSIS_VERSION_SUB == 4,
               "Pinned CMSIS-Core 6.4 required");
#define NX_LAYOUT(address, base, type, member)                                 \
    _Static_assert(address == (uintptr_t)(base) + offsetof(type, member),      \
                   "CMSIS register address: " #address)

NX_LAYOUT(NX_ARCH_REG_CPUID, SCB_BASE, SCB_Type, CPUID);
NX_LAYOUT(NX_ARCH_REG_SCR, SCB_BASE, SCB_Type, SCR);
NX_LAYOUT(NX_ARCH_REG_CCR, SCB_BASE, SCB_Type, CCR);
#if __CORTEX_M != 0U
NX_LAYOUT(NX_ARCH_REG_DWT_CTRL, DWT_BASE, DWT_Type, CTRL);
NX_LAYOUT(NX_ARCH_REG_DEMCR, DCB_BASE, DCB_Type, DEMCR);
#if __CORTEX_M != 23U
NX_LAYOUT(NX_ARCH_REG_DWT_COUNT, DWT_BASE, DWT_Type, CYCCNT);
#endif
#endif
#if __CORTEX_M != 0U || NX_ARCH_LAYOUT_M0PLUS
NX_LAYOUT(NX_ARCH_REG_MPU_TYPE, MPU_BASE, MPU_Type, TYPE);
NX_LAYOUT(NX_ARCH_REG_MPU_CTRL, MPU_BASE, MPU_Type, CTRL);
NX_LAYOUT(NX_ARCH_REG_MPU_RNR, MPU_BASE, MPU_Type, RNR);
NX_LAYOUT(NX_ARCH_REG_MPU_RBAR, MPU_BASE, MPU_Type, RBAR);
#if __CORTEX_M == 23U || __CORTEX_M >= 33U
NX_LAYOUT(NX_ARCH_REG_MPU_ATTR, MPU_BASE, MPU_Type, RLAR);
NX_LAYOUT(NX_ARCH_REG_MPU_MAIR, MPU_BASE, MPU_Type, MAIR0);
_Static_assert(MPU_RLAR_EN_Msk == 1U && MPU_RLAR_AttrIndx_Pos == 1U &&
                   MPU_RBAR_AP_Pos == 1U && MPU_RBAR_SH_Pos == 3U,
               "CMSIS MPUv8 encoded fields");
NX_LAYOUT(NX_ARCH_REG_SAU_CTRL, SAU_BASE, SAU_Type, CTRL);
NX_LAYOUT(NX_ARCH_REG_SAU_TYPE, SAU_BASE, SAU_Type, TYPE);
NX_LAYOUT(NX_ARCH_REG_SAU_RNR, SAU_BASE, SAU_Type, RNR);
NX_LAYOUT(NX_ARCH_REG_SAU_RBAR, SAU_BASE, SAU_Type, RBAR);
NX_LAYOUT(NX_ARCH_REG_SAU_RLAR, SAU_BASE, SAU_Type, RLAR);
_Static_assert(SAU_RLAR_ENABLE_Msk == 1U && SAU_RLAR_NSC_Msk == 2U,
               "CMSIS SAU encoded fields");
#else
NX_LAYOUT(NX_ARCH_REG_MPU_ATTR, MPU_BASE, MPU_Type, RASR);
_Static_assert(MPU_RASR_ENABLE_Msk == 1U && MPU_RASR_SIZE_Pos == 1U &&
                   MPU_RASR_AP_Pos == 24U && MPU_RASR_XN_Pos == 28U,
               "CMSIS MPUv7 encoded fields");
#endif
#endif
#if __CORTEX_M == 7U || __CORTEX_M >= 55U
NX_LAYOUT(NX_ARCH_REG_CLIDR, SCB_BASE, SCB_Type, CLIDR);
NX_LAYOUT(NX_ARCH_REG_CCSIDR, SCB_BASE, SCB_Type, CCSIDR);
NX_LAYOUT(NX_ARCH_REG_CSSELR, SCB_BASE, SCB_Type, CSSELR);
NX_LAYOUT(NX_ARCH_REG_ICIALLU, SCB_BASE, SCB_Type, ICIALLU);
NX_LAYOUT(NX_ARCH_REG_ICIMVAU, SCB_BASE, SCB_Type, ICIMVAU);
NX_LAYOUT(NX_ARCH_REG_DCIMVAC, SCB_BASE, SCB_Type, DCIMVAC);
NX_LAYOUT(NX_ARCH_REG_DCISW, SCB_BASE, SCB_Type, DCISW);
NX_LAYOUT(NX_ARCH_REG_DCCMVAC, SCB_BASE, SCB_Type, DCCMVAC);
NX_LAYOUT(NX_ARCH_REG_DCCIMVAC, SCB_BASE, SCB_Type, DCCIMVAC);
NX_LAYOUT(NX_ARCH_REG_DCCISW, SCB_BASE, SCB_Type, DCCISW);
_Static_assert(NX_ARCH_CCR_DC == SCB_CCR_DC_Msk &&
                   NX_ARCH_CCR_IC == SCB_CCR_IC_Msk,
               "CMSIS cache control fields");
#endif
