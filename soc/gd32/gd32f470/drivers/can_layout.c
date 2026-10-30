/**
 * \file            can_layout.c
 * \brief           Compile-only C11 checks of exact GD32F470 SDK addresses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f4xx_can.h"

/* Register macros expand REG32 lazily. Only this isolated translation unit
 * interprets them as integer addresses, after all SDK declarations are parsed.
 * It defines no runtime objects or functions and cannot change MMIO behavior.
 */
#undef REG32
#define REG32(address) (address)
#define NX_GD32_OFFSET(reg, base, offset)                                      \
    _Static_assert((reg) == (base) + (offset), #reg " SDK register offset")

NX_GD32_OFFSET(CAN_CTL(CAN0), CAN0, 0x00U);
NX_GD32_OFFSET(CAN_STAT(CAN0), CAN0, 0x04U);
NX_GD32_OFFSET(CAN_TSTAT(CAN0), CAN0, 0x08U);
NX_GD32_OFFSET(CAN_RFIFO0(CAN0), CAN0, 0x0CU);
NX_GD32_OFFSET(CAN_RFIFO1(CAN0), CAN0, 0x10U);
NX_GD32_OFFSET(CAN_INTEN(CAN0), CAN0, 0x14U);
NX_GD32_OFFSET(CAN_ERR(CAN0), CAN0, 0x18U);
NX_GD32_OFFSET(CAN_BT(CAN0), CAN0, 0x1CU);
NX_GD32_OFFSET(CAN_TMI(CAN0, 0U), CAN0, 0x180U);
NX_GD32_OFFSET(CAN_TMP(CAN0, 2U), CAN0, 0x1A4U);
NX_GD32_OFFSET(CAN_TMDATA1(CAN0, 2U), CAN0, 0x1ACU);
NX_GD32_OFFSET(CAN_RFIFOMI(CAN0, 0U), CAN0, 0x1B0U);
NX_GD32_OFFSET(CAN_RFIFOMDATA1(CAN0, 1U), CAN0, 0x1CCU);
NX_GD32_OFFSET(CAN_FCTL(CAN0), CAN0, 0x200U);
NX_GD32_OFFSET(CAN_FMCFG(CAN0), CAN0, 0x204U);
NX_GD32_OFFSET(CAN_FSCFG(CAN0), CAN0, 0x20CU);
NX_GD32_OFFSET(CAN_FAFIFO(CAN0), CAN0, 0x214U);
NX_GD32_OFFSET(CAN_FW(CAN0), CAN0, 0x21CU);
NX_GD32_OFFSET(CAN_FDATA0(CAN0, 0U), CAN0, 0x240U);
NX_GD32_OFFSET(CAN_FDATA1(CAN0, 27U), CAN0, 0x31CU);

#undef NX_GD32_OFFSET
#undef REG32
