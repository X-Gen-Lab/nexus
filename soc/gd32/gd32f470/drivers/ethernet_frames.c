/**
 * \file            ethernet_frames.c
 * \brief           Exact gd32f470 enhanced RMII and clock admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_eth_frames.h"
#include "gd32f4xx_enet.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
_Static_assert(ENET_TDES0_TTSEN == 0x02000000U, "FS timestamp request");
_Static_assert(ENET_TDES0_TTMSS == 0x00020000U, "LS timestamp status");
_Static_assert(ENET_RDES0_TSV == 0x80U, "LS enhanced capture validity");
_Static_assert(ENET_DMA_BCTL_DFM == 0x80U, "enhanced descriptor stride");
_Static_assert(ENET_PTP_TSCTL_SCROM == 0x200U, "digital rollover selection");
#ifndef NX_ETH_FRAMES_SOC_IRQ_CONTROL
static nx_result_t control_irq(nx_eth_frames_mac_t* mac, bool enabled) {
    unsigned bit = 1U << ((unsigned)ENET_IRQn % 32U);
    unsigned word = (unsigned)ENET_IRQn / 32U;
    if (enabled) {
        NVIC_ClearPendingIRQ(ENET_IRQn);
        NVIC_SetPriority(ENET_IRQn, mac->source.priority);
        NVIC_EnableIRQ(ENET_IRQn);
        nx_arch_dsb();
        return (NVIC->ISER[word] & bit) != 0U &&
                       NVIC_GetPriority(ENET_IRQn) == mac->source.priority
                   ? NX_SUCCESS
                   : NX_ERROR_IO;
    }
    NVIC_DisableIRQ(ENET_IRQn);
    NVIC_ClearPendingIRQ(ENET_IRQn);
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
    NVIC_SetPendingIRQ(ENET_IRQn);
}
#define NX_ETH_FRAMES_SOC_PEND_IRQ pend_irq
#endif
#ifndef NX_ETH_FRAMES_SOC_WAKE_VALIDATE
static nx_result_t validate_wake(nx_eth_frames_mac_t* mac,
                                 const nx_irq_wake_t* wake, uint8_t ceiling) {
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_irq_source_t actual = {ENET_IRQn, (uint8_t)NVIC_GetPriority(ENET_IRQn),
                              (uint8_t)NVIC_GetPriorityGrouping()};
    return nx_irq_wake_validate(wake, &mac->irq_policy, &actual);
}
#define NX_ETH_FRAMES_SOC_WAKE_VALIDATE validate_wake
#endif
nx_result_t nx_gd32_eth_frames_initialize(nx_eth_frames_mac_t* mac,
                                          nx_time_us_t deadline) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || mac->source.priority > 15U ||
        mac->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        mac->irq_policy.external_irq_count != 91U)
        return NX_ERROR_INVALID;
    if (mac->registers != UINT32_C(0x40028000) || mac->hclk_hz != 200000000U ||
        mac->source.irq_number != ENET_IRQn)
        return NX_ERROR_UNSUPPORTED;
    if (mac->regions == NULL || mac->region_count == 0U ||
        mac->region_count > SIZE_MAX / sizeof(*mac->regions) ||
        (uintptr_t)mac->regions >
            UINTPTR_MAX - mac->region_count * sizeof(*mac->regions))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < mac->region_count; ++i) {
        const nx_dma_memory_region_t* region = &mac->regions[i];
        if (region->begin < UINT32_C(0x20000000) || region->size == 0U ||
            region->size > 196608U ||
            region->begin - UINT32_C(0x20000000) > 196608U - region->size ||
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
            (25U | (400U << 6U) | 0x00400000U | (9U << 24U)) ||
        (cfgr & 0xfcffU) != 0x940aU)
        return NX_ERROR_STATE;
    mac->irq_control = NX_ETH_FRAMES_SOC_IRQ_CONTROL;
    mac->pend_irq = NX_ETH_FRAMES_SOC_PEND_IRQ;
    mac->validate_wake = NX_ETH_FRAMES_SOC_WAKE_VALIDATE;
    return nx_eth_frames_mac_initialize(mac, deadline);
}
