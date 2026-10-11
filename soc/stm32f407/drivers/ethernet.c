/**
 * \file            ethernet.c
 * \brief           Actual STM32F407 SDK layout and RMII lifecycle facts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_eth.h"
#include <stddef.h>
_Static_assert(offsetof(ETH_TypeDef, MACCR) == 0x0U, "ETH MACCR offset");
_Static_assert(offsetof(ETH_TypeDef, MACFFR) == 0x4U, "ETH MACFFR offset");
_Static_assert(offsetof(ETH_TypeDef, MACMIIAR) == 0x10U, "ETH MACMIIAR offset");
_Static_assert(offsetof(ETH_TypeDef, MACMIIDR) == 0x14U, "ETH MACMIIDR offset");
_Static_assert(offsetof(ETH_TypeDef, MACDBGR) == 0x34U, "ETH MACDBGR offset");
_Static_assert(offsetof(ETH_TypeDef, MACIMR) == 0x3CU, "ETH MACIMR offset");
_Static_assert(offsetof(ETH_TypeDef, MACA0HR) == 0x40U, "ETH MACA0HR offset");
_Static_assert(offsetof(ETH_TypeDef, MACA0LR) == 0x44U, "ETH MACA0LR offset");
_Static_assert(offsetof(ETH_TypeDef, DMABMR) == 0x1000U, "ETH DMABMR offset");
_Static_assert(offsetof(ETH_TypeDef, DMATPDR) == 0x1004U, "ETH DMATPDR offset");
_Static_assert(offsetof(ETH_TypeDef, DMARPDR) == 0x1008U, "ETH DMARPDR offset");
_Static_assert(offsetof(ETH_TypeDef, DMARDLAR) == 0x100CU,
               "ETH DMARDLAR offset");
_Static_assert(offsetof(ETH_TypeDef, DMATDLAR) == 0x1010U,
               "ETH DMATDLAR offset");
_Static_assert(offsetof(ETH_TypeDef, DMASR) == 0x1014U, "ETH DMASR offset");
_Static_assert(offsetof(ETH_TypeDef, DMAOMR) == 0x1018U, "ETH DMAOMR offset");
_Static_assert(offsetof(ETH_TypeDef, DMAIER) == 0x101CU, "ETH DMAIER offset");
_Static_assert(ETH_DMABMR_SR == UINT32_C(0x00000001), "ETH_DMABMR_SR contract");
_Static_assert(ETH_DMABMR_EDE == UINT32_C(0x00000080),
               "ETH_DMABMR_EDE contract");
_Static_assert(ETH_MACCR_RE == UINT32_C(0x00000004), "ETH_MACCR_RE contract");
_Static_assert(ETH_MACCR_TE == UINT32_C(0x00000008), "ETH_MACCR_TE contract");
_Static_assert(ETH_MACCR_DM == UINT32_C(0x00000800), "ETH_MACCR_DM contract");
_Static_assert(ETH_MACCR_FES == UINT32_C(0x00004000), "ETH_MACCR_FES contract");
_Static_assert(ETH_MACCR_APCS == UINT32_C(0x00000080),
               "ETH_MACCR_APCS contract");
_Static_assert(ETH_MACCR_CSTF == UINT32_C(0x02000000),
               "ETH_MACCR_CSTF contract");
_Static_assert(ETH_MACMIIAR_MB == UINT32_C(0x00000001),
               "ETH_MACMIIAR_MB contract");
_Static_assert(ETH_MACMIIAR_MW == UINT32_C(0x00000002),
               "ETH_MACMIIAR_MW contract");
_Static_assert(ETH_MACMIIAR_CR == UINT32_C(0x0000001C),
               "ETH_MACMIIAR_CR contract");
_Static_assert(ETH_MACMIIAR_PA == UINT32_C(0x0000F800),
               "ETH_MACMIIAR_PA contract");
_Static_assert(ETH_MACMIIAR_MR == UINT32_C(0x000007C0),
               "ETH_MACMIIAR_MR contract");
_Static_assert(ETH_DMAOMR_SR == UINT32_C(0x00000002), "ETH_DMAOMR_SR contract");
_Static_assert(ETH_DMAOMR_ST == UINT32_C(0x00002000), "ETH_DMAOMR_ST contract");
_Static_assert(ETH_DMAOMR_FTF == UINT32_C(0x00100000),
               "ETH_DMAOMR_FTF contract");
_Static_assert(ETH_DMAOMR_TSF == UINT32_C(0x00200000),
               "ETH_DMAOMR_TSF contract");
_Static_assert(ETH_DMAOMR_RSF == UINT32_C(0x02000000),
               "ETH_DMAOMR_RSF contract");
_Static_assert(ETH_DMAOMR_FEF == UINT32_C(0x00000080),
               "ETH_DMAOMR_FEF contract");
_Static_assert(ETH_DMASR_FBES == UINT32_C(0x00002000),
               "ETH_DMASR_FBES contract");
_Static_assert(ETH_DMASR_TPS == UINT32_C(0x00700000), "ETH_DMASR_TPS contract");
_Static_assert(ETH_DMASR_RPS == UINT32_C(0x000E0000), "ETH_DMASR_RPS contract");
_Static_assert(ETH_MACDBGR_MMTEA == UINT32_C(0x00010000),
               "ETH_MACDBGR_MMTEA contract");
_Static_assert(ETH_MACDBGR_MTFCS == UINT32_C(0x00060000),
               "ETH_MACDBGR_MTFCS contract");
_Static_assert(ETH_MACDBGR_TFRS == UINT32_C(0x00300000),
               "ETH_MACDBGR_TFRS contract");
_Static_assert(ETH_MACDBGR_TFWA == UINT32_C(0x00400000),
               "ETH_MACDBGR_TFWA contract");
_Static_assert(ETH_MACDBGR_TFNE == UINT32_C(0x01000000),
               "ETH_MACDBGR_TFNE contract");
_Static_assert(ETH_MACDBGR_MMRPEA == UINT32_C(0x00000001),
               "ETH_MACDBGR_MMRPEA contract");
_Static_assert(ETH_MACDBGR_MSFRWCS == UINT32_C(0x00000006),
               "ETH_MACDBGR_MSFRWCS contract");
_Static_assert(ETH_MACDBGR_RFWRA == UINT32_C(0x00000010),
               "ETH_MACDBGR_RFWRA contract");
_Static_assert(ETH_MACDBGR_RFRCS == UINT32_C(0x00000060),
               "ETH_MACDBGR_RFRCS contract");
_Static_assert(ETH_IRQn == 61, "exact Ethernet vector");
#ifndef NX_ETH_SOC_IRQ_CONTROL
static nx_result_t control_irq(nx_eth_mac_t* mac, bool enabled) {
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
#define NX_ETH_SOC_IRQ_CONTROL control_irq
#endif
#ifndef NX_ETH_SOC_PEND_IRQ
static void pend_irq(nx_eth_mac_t* mac) {
    (void)mac;
    NVIC_SetPendingIRQ(ETH_IRQn);
}
#define NX_ETH_SOC_PEND_IRQ pend_irq
#endif
#ifndef NX_ETH_SOC_WAKE_VALIDATE
static nx_result_t validate_wake(nx_eth_mac_t* mac, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_irq_source_t actual = {ETH_IRQn, (uint8_t)NVIC_GetPriority(ETH_IRQn),
                              (uint8_t)NVIC_GetPriorityGrouping()};
    return nx_irq_wake_validate(wake, &mac->irq_policy, &actual);
}
#define NX_ETH_SOC_WAKE_VALIDATE validate_wake
#endif
nx_result_t nx_stm32_eth_initialize(nx_eth_mac_t* mac, nx_time_us_t deadline) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || mac->source.priority > 15U ||
        mac->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        mac->irq_policy.external_irq_count != 82U)
        return NX_ERROR_INVALID;
    if (mac->registers != ETH_BASE || mac->hclk_hz != 168000000U ||
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
    if ((RCC->AHB1ENR & (RCC_AHB1ENR_ETHMACEN | RCC_AHB1ENR_ETHMACTXEN |
                         RCC_AHB1ENR_ETHMACRXEN)) != UINT32_C(0x0E000000) ||
        (SYSCFG->PMC & SYSCFG_PMC_MII_RMII_SEL) == 0U)
        return NX_ERROR_STATE;
    mac->irq_control = NX_ETH_SOC_IRQ_CONTROL;
    mac->pend_irq = NX_ETH_SOC_PEND_IRQ;
    mac->validate_wake = NX_ETH_SOC_WAKE_VALIDATE;
    return nx_eth_mac_initialize(mac, deadline);
}
