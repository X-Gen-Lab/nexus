/**
 * \file            ethernet_frames.c
 * \brief           Exact stm32f407 enhanced RMII and clock admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_eth_frames.h"
#include "stm32f4xx_hal_eth.h"
#include <stddef.h>
_Static_assert(offsetof(ETH_DMADescTypeDef, DESC7) == 28U,
               "HAL hardware prefix excludes software backup addresses");
_Static_assert(offsetof(ETH_TypeDef, PTPTSCR) == 0x700U, "PTP control offset");
_Static_assert(offsetof(ETH_TypeDef, PTPTSHR) == 0x708U, "PTP seconds offset");
_Static_assert(ETH_DMATXDESC_TTSE == 0x02000000U, "FS timestamp request");
_Static_assert(ETH_DMATXDESC_TTSS == 0x00020000U, "LS timestamp status");
/* HAL names bit 7 for the normal descriptor. RM0090 enhanced RDES0
 * reinterprets this position as timestamp available on LS. */
_Static_assert(ETH_DMARXDESC_IPV4HCE == 0x80U, "RDES0 bit 7 position");
_Static_assert(ETH_DMABMR_EDE == 0x80U, "enhanced descriptor stride");
#ifndef NX_ETH_FRAMES_SOC_IRQ_CONTROL
static nx_result_t control_irq(nx_eth_frames_mac_t* mac, bool enabled) {
    unsigned bit = 1U << ((unsigned)ETH_IRQn % 32U);
    unsigned word = (unsigned)ETH_IRQn / 32U;
    if (enabled) {
        NVIC_ClearPendingIRQ(ETH_IRQn);
        NVIC_SetPriority(ETH_IRQn, mac->source.priority);
        NVIC_EnableIRQ(ETH_IRQn);
        nx_arch_dsb();
        return (NVIC->ISER[word] & bit) != 0U &&
                       NVIC_GetPriority(ETH_IRQn) == mac->source.priority
                   ? NX_SUCCESS
                   : NX_ERROR_IO;
    }
    NVIC_DisableIRQ(ETH_IRQn);
    NVIC_ClearPendingIRQ(ETH_IRQn);
    nx_arch_dsb();
    nx_arch_isb();
    return (NVIC->ISER[word] & bit) == 0U && (NVIC->ISPR[word] & bit) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
#define NX_ETH_FRAMES_SOC_IRQ_CONTROL control_irq
#endif
#ifndef NX_ETH_FRAMES_SOC_PEND_IRQ
static void pend_irq(nx_eth_frames_mac_t* mac) {
    (void)mac;
    NVIC_SetPendingIRQ(ETH_IRQn);
}
#define NX_ETH_FRAMES_SOC_PEND_IRQ pend_irq
#endif
#ifndef NX_ETH_FRAMES_SOC_WAKE_VALIDATE
static nx_result_t validate_wake(nx_eth_frames_mac_t* mac,
                                 const nx_irq_wake_t* wake, uint8_t ceiling) {
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_irq_source_t actual = {ETH_IRQn, (uint8_t)NVIC_GetPriority(ETH_IRQn),
                              (uint8_t)NVIC_GetPriorityGrouping()};
    return nx_irq_wake_validate(wake, &mac->irq_policy, &actual);
}
#define NX_ETH_FRAMES_SOC_WAKE_VALIDATE validate_wake
#endif
nx_result_t nx_stm32_eth_frames_initialize(nx_eth_frames_mac_t* mac,
                                           nx_time_us_t deadline) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || mac->source.priority > 15U ||
        mac->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        mac->irq_policy.external_irq_count != 82U)
        return NX_ERROR_INVALID;
    if (mac->registers != UINT32_C(0x40028000) || mac->hclk_hz != 168000000U ||
        mac->source.irq_number != ETH_IRQn)
        return NX_ERROR_UNSUPPORTED;
    if (mac->regions == NULL || mac->region_count == 0U ||
        mac->region_count > SIZE_MAX / sizeof(*mac->regions) ||
        (uintptr_t)mac->regions >
            UINTPTR_MAX - mac->region_count * sizeof(*mac->regions))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < mac->region_count; ++i) {
        const nx_dma_memory_region_t* region = &mac->regions[i];
        if (region->begin < UINT32_C(0x20000000) || region->size == 0U ||
            region->size > 131072U ||
            region->begin - UINT32_C(0x20000000) > 131072U - region->size ||
            (region->permissions & ~3U) != 0U)
            return NX_ERROR_PERMISSION;
    }
    /* Platform leases the established high clock, pads and all four gates.
     * Requested SW is insufficient: fresh SWS/PLL/AHB facts must agree.
     * These cold reads precede every MAC/descriptor access and reset. */
    volatile const uint32_t* rcc =
        (volatile const uint32_t*)UINT32_C(0x40023800);
    uint32_t cr = rcc[0U], pll = rcc[1U], cfgr = rcc[2U];
    if ((rcc[0x30U / 4U] & 0x1e000000U) != 0x1e000000U ||
        (*(volatile const uint32_t*)UINT32_C(0x40013804) & 0x00800000U) == 0U ||
        (cr & 0x03030000U) != 0x03030000U ||
        (pll & 0x0f437fffU) !=
            (8U | (336U << 6U) | 0x00400000U | (7U << 24U)) ||
        (cfgr & 0xfcffU) != 0x940aU)
        return NX_ERROR_STATE;
    mac->irq_control = NX_ETH_FRAMES_SOC_IRQ_CONTROL;
    mac->pend_irq = NX_ETH_FRAMES_SOC_PEND_IRQ;
    mac->validate_wake = NX_ETH_FRAMES_SOC_WAKE_VALIDATE;
    return nx_eth_frames_mac_initialize(mac, deadline);
}
