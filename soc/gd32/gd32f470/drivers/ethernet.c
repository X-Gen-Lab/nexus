/**
 * \file            ethernet.c
 * \brief           Actual GD32F470 SDK layout and RMII lifecycle facts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_eth.h"
#include "gd32f4xx_enet.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
#include <stddef.h>
_Static_assert(ENET_DMA_BCTL_SWR == UINT32_C(0x00000001),
               "ENET_DMA_BCTL_SWR contract");
_Static_assert(ENET_DMA_BCTL_DFM == UINT32_C(0x00000080),
               "ENET_DMA_BCTL_DFM contract");
_Static_assert(ENET_MAC_CFG_REN == UINT32_C(0x00000004),
               "ENET_MAC_CFG_REN contract");
_Static_assert(ENET_MAC_CFG_TEN == UINT32_C(0x00000008),
               "ENET_MAC_CFG_TEN contract");
_Static_assert(ENET_MAC_CFG_DPM == UINT32_C(0x00000800),
               "ENET_MAC_CFG_DPM contract");
_Static_assert(ENET_MAC_CFG_SPD == UINT32_C(0x00004000),
               "ENET_MAC_CFG_SPD contract");
_Static_assert(ENET_MAC_CFG_APCD == UINT32_C(0x00000080),
               "ENET_MAC_CFG_APCD contract");
_Static_assert(ENET_MAC_CFG_TFCD == UINT32_C(0x02000000),
               "ENET_MAC_CFG_TFCD contract");
_Static_assert(ENET_MAC_PHY_CTL_PB == UINT32_C(0x00000001),
               "ENET_MAC_PHY_CTL_PB contract");
_Static_assert(ENET_MAC_PHY_CTL_PW == UINT32_C(0x00000002),
               "ENET_MAC_PHY_CTL_PW contract");
_Static_assert(ENET_MAC_PHY_CTL_CLR == UINT32_C(0x0000001C),
               "ENET_MAC_PHY_CTL_CLR contract");
_Static_assert(ENET_MAC_PHY_CTL_PA == UINT32_C(0x0000F800),
               "ENET_MAC_PHY_CTL_PA contract");
_Static_assert(ENET_MAC_PHY_CTL_PR == UINT32_C(0x000007C0),
               "ENET_MAC_PHY_CTL_PR contract");
_Static_assert(ENET_DMA_CTL_SRE == UINT32_C(0x00000002),
               "ENET_DMA_CTL_SRE contract");
_Static_assert(ENET_DMA_CTL_STE == UINT32_C(0x00002000),
               "ENET_DMA_CTL_STE contract");
_Static_assert(ENET_DMA_CTL_FTF == UINT32_C(0x00100000),
               "ENET_DMA_CTL_FTF contract");
_Static_assert(ENET_DMA_CTL_TSFD == UINT32_C(0x00200000),
               "ENET_DMA_CTL_TSFD contract");
_Static_assert(ENET_DMA_CTL_RSFD == UINT32_C(0x02000000),
               "ENET_DMA_CTL_RSFD contract");
_Static_assert(ENET_DMA_CTL_FERF == UINT32_C(0x00000080),
               "ENET_DMA_CTL_FERF contract");
_Static_assert(ENET_DMA_STAT_FBE == UINT32_C(0x00002000),
               "ENET_DMA_STAT_FBE contract");
_Static_assert(ENET_DMA_STAT_TP == UINT32_C(0x00700000),
               "ENET_DMA_STAT_TP contract");
_Static_assert(ENET_DMA_STAT_RP == UINT32_C(0x000E0000),
               "ENET_DMA_STAT_RP contract");
_Static_assert(ENET_MAC_DBG_MTNI == UINT32_C(0x00010000),
               "ENET_MAC_DBG_MTNI contract");
_Static_assert(ENET_MAC_DBG_SOMT == UINT32_C(0x00060000),
               "ENET_MAC_DBG_SOMT contract");
_Static_assert(ENET_MAC_DBG_TXFRS == UINT32_C(0x00300000),
               "ENET_MAC_DBG_TXFRS contract");
_Static_assert(ENET_MAC_DBG_TXFW == UINT32_C(0x00400000),
               "ENET_MAC_DBG_TXFW contract");
_Static_assert(ENET_MAC_DBG_TXFNE == UINT32_C(0x01000000),
               "ENET_MAC_DBG_TXFNE contract");
_Static_assert(ENET_MAC_DBG_MRNI == UINT32_C(0x00000001),
               "ENET_MAC_DBG_MRNI contract");
_Static_assert(ENET_MAC_DBG_RXAFS == UINT32_C(0x00000006),
               "ENET_MAC_DBG_RXAFS contract");
_Static_assert(ENET_MAC_DBG_RXFW == UINT32_C(0x00000010),
               "ENET_MAC_DBG_RXFW contract");
_Static_assert(ENET_MAC_DBG_RXFRS == UINT32_C(0x00000060),
               "ENET_MAC_DBG_RXFRS contract");
_Static_assert(ENET_TDES0_DAV == UINT32_C(0x80000000),
               "ENET_TDES0_DAV contract");
_Static_assert(ENET_TDES0_INTC == UINT32_C(0x40000000),
               "ENET_TDES0_INTC contract");
_Static_assert(ENET_TDES0_LSG == UINT32_C(0x20000000),
               "ENET_TDES0_LSG contract");
_Static_assert(ENET_TDES0_FSG == UINT32_C(0x10000000),
               "ENET_TDES0_FSG contract");
_Static_assert(ENET_TDES0_TCHM == UINT32_C(0x00100000),
               "ENET_TDES0_TCHM contract");
_Static_assert(ENET_TDES0_ES == UINT32_C(0x00008000), "ENET_TDES0_ES contract");
_Static_assert(ENET_TDES1_TB1S == UINT32_C(0x00001FFF),
               "ENET_TDES1_TB1S contract");
_Static_assert(ENET_RDES0_DAV == UINT32_C(0x80000000),
               "ENET_RDES0_DAV contract");
_Static_assert(ENET_RDES0_FRML == UINT32_C(0x3FFF0000),
               "ENET_RDES0_FRML contract");
_Static_assert(ENET_RDES0_FDES == UINT32_C(0x00000200),
               "ENET_RDES0_FDES contract");
_Static_assert(ENET_RDES0_LDES == UINT32_C(0x00000100),
               "ENET_RDES0_LDES contract");
_Static_assert(ENET_RDES0_ERRS == UINT32_C(0x00008000),
               "ENET_RDES0_ERRS contract");
_Static_assert(ENET_RDES0_CERR == UINT32_C(0x00000002),
               "ENET_RDES0_CERR contract");
_Static_assert(ENET_RDES0_VTAG == UINT32_C(0x00000400),
               "ENET_RDES0_VTAG contract");
_Static_assert(ENET_RDES0_LERR == UINT32_C(0x00001000),
               "ENET_RDES0_LERR contract");
_Static_assert(ENET_RDES1_RCHM == UINT32_C(0x00004000),
               "ENET_RDES1_RCHM contract");
_Static_assert(ENET_RDES1_RB1S == UINT32_C(0x00001FFF),
               "ENET_RDES1_RB1S contract");
_Static_assert(ENET_IRQn == 61, "exact Ethernet vector");
#ifndef NX_ETH_SOC_IRQ_CONTROL
static nx_result_t control_irq(nx_eth_mac_t* mac, bool enabled) {
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
#define NX_ETH_SOC_IRQ_CONTROL control_irq
#endif
#ifndef NX_ETH_SOC_PEND_IRQ
static void pend_irq(nx_eth_mac_t* mac) {
    (void)mac;
    NVIC_SetPendingIRQ(ENET_IRQn);
}
#define NX_ETH_SOC_PEND_IRQ pend_irq
#endif
#ifndef NX_ETH_SOC_WAKE_VALIDATE
static nx_result_t validate_wake(nx_eth_mac_t* mac, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_irq_source_t actual = {ENET_IRQn, (uint8_t)NVIC_GetPriority(ENET_IRQn),
                              (uint8_t)NVIC_GetPriorityGrouping()};
    return nx_irq_wake_validate(wake, &mac->irq_policy, &actual);
}
#define NX_ETH_SOC_WAKE_VALIDATE validate_wake
#endif
nx_result_t nx_gd32_eth_initialize(nx_eth_mac_t* mac, nx_time_us_t deadline) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || mac->source.priority > 15U ||
        mac->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        mac->irq_policy.external_irq_count != 91U)
        return NX_ERROR_INVALID;
    if (mac->registers != ENET_BASE || mac->hclk_hz != 200000000U ||
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
    if ((RCU_AHB1EN & (RCU_AHB1EN_ENETEN | RCU_AHB1EN_ENETTXEN |
                       RCU_AHB1EN_ENETRXEN)) != UINT32_C(0x0E000000) ||
        (SYSCFG_CFG1 & SYSCFG_CFG1_ENET_PHY_SEL) == 0U)
        return NX_ERROR_STATE;
    mac->irq_control = NX_ETH_SOC_IRQ_CONTROL;
    mac->pend_irq = NX_ETH_SOC_PEND_IRQ;
    mac->validate_wake = NX_ETH_SOC_WAKE_VALIDATE;
    return nx_eth_mac_initialize(mac, deadline);
}
