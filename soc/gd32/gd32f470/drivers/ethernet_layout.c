/**
 * \file            ethernet_layout.c
 * \brief           Compile-only C11 checks of exact GD32F470 SDK addresses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f4xx_enet.h"

/* Register macros expand REG32 lazily. Only this isolated translation unit
 * interprets them as integer addresses, after all SDK declarations are parsed.
 * It defines no runtime objects or functions and cannot change MMIO behavior.
 */
#undef REG32
#define REG32(address) (address)
#define NX_GD32_OFFSET(reg, base, offset)                                      \
    _Static_assert((reg) == (base) + (offset), #reg " SDK register offset")

NX_GD32_OFFSET(ENET_MAC_CFG, ENET_BASE, 0x0U);
NX_GD32_OFFSET(ENET_MAC_FRMF, ENET_BASE, 0x4U);
NX_GD32_OFFSET(ENET_MAC_PHY_CTL, ENET_BASE, 0x10U);
NX_GD32_OFFSET(ENET_MAC_PHY_DATA, ENET_BASE, 0x14U);
NX_GD32_OFFSET(ENET_MAC_DBG, ENET_BASE, 0x34U);
NX_GD32_OFFSET(ENET_MAC_INTMSK, ENET_BASE, 0x3CU);
NX_GD32_OFFSET(ENET_MAC_ADDR0H, ENET_BASE, 0x40U);
NX_GD32_OFFSET(ENET_MAC_ADDR0L, ENET_BASE, 0x44U);
NX_GD32_OFFSET(ENET_MAC_FCTL, ENET_BASE, 0x18U);
NX_GD32_OFFSET(ENET_MAC_VLT, ENET_BASE, 0x1CU);
NX_GD32_OFFSET(ENET_PTP_TSCTL, ENET_BASE, 0x700U);
NX_GD32_OFFSET(ENET_PTP_SSINC, ENET_BASE, 0x704U);
NX_GD32_OFFSET(ENET_PTP_TSH, ENET_BASE, 0x708U);
NX_GD32_OFFSET(ENET_PTP_TSL, ENET_BASE, 0x70CU);
NX_GD32_OFFSET(ENET_PTP_TSUH, ENET_BASE, 0x710U);
NX_GD32_OFFSET(ENET_PTP_TSUL, ENET_BASE, 0x714U);
NX_GD32_OFFSET(ENET_PTP_TSADDEND, ENET_BASE, 0x718U);
NX_GD32_OFFSET(ENET_DMA_BCTL, ENET_BASE, 0x1000U);
NX_GD32_OFFSET(ENET_DMA_TPEN, ENET_BASE, 0x1004U);
NX_GD32_OFFSET(ENET_DMA_RPEN, ENET_BASE, 0x1008U);
NX_GD32_OFFSET(ENET_DMA_RDTADDR, ENET_BASE, 0x100CU);
NX_GD32_OFFSET(ENET_DMA_TDTADDR, ENET_BASE, 0x1010U);
NX_GD32_OFFSET(ENET_DMA_STAT, ENET_BASE, 0x1014U);
NX_GD32_OFFSET(ENET_DMA_CTL, ENET_BASE, 0x1018U);
NX_GD32_OFFSET(ENET_DMA_INTEN, ENET_BASE, 0x101CU);

#undef NX_GD32_OFFSET
#undef REG32
