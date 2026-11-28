/**
 * \file            cmsis_layout.h
 * \brief           Compile-time contract for the shared dual bxCAN register
 * map. \author          Nexus Team \version         1.0.0 \date 2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32_BXCAN_CMSIS_LAYOUT_H
#define NEXUS_STM32_BXCAN_CMSIS_LAYOUT_H
#include <stddef.h>

_Static_assert(offsetof(CAN_TypeDef, MCR) == 0x00U, "CAN control layout");
_Static_assert(offsetof(CAN_TypeDef, MSR) == 0x04U, "CAN status layout");
_Static_assert(offsetof(CAN_TypeDef, TSR) == 0x08U, "CAN TX status layout");
_Static_assert(offsetof(CAN_TypeDef, RF0R) == 0x0CU, "CAN FIFO0 layout");
_Static_assert(offsetof(CAN_TypeDef, RF1R) == 0x10U, "CAN FIFO1 layout");
_Static_assert(offsetof(CAN_TypeDef, IER) == 0x14U, "CAN interrupt layout");
_Static_assert(offsetof(CAN_TypeDef, ESR) == 0x18U, "CAN error layout");
_Static_assert(offsetof(CAN_TypeDef, BTR) == 0x1CU, "CAN timing layout");
_Static_assert(offsetof(CAN_TypeDef, sTxMailBox) == 0x180U,
               "CAN TX mailboxes layout");
_Static_assert(sizeof(CAN_TxMailBox_TypeDef) == 16U, "CAN TX mailbox stride");
_Static_assert(offsetof(CAN_TypeDef, sFIFOMailBox) == 0x1B0U,
               "CAN RX mailboxes layout");
_Static_assert(sizeof(CAN_FIFOMailBox_TypeDef) == 16U, "CAN RX mailbox stride");
_Static_assert(offsetof(CAN_TypeDef, FMR) == 0x200U, "CAN filter control");
_Static_assert(offsetof(CAN_TypeDef, FM1R) == 0x204U, "CAN filter mode");
_Static_assert(offsetof(CAN_TypeDef, FS1R) == 0x20CU, "CAN filter scale");
_Static_assert(offsetof(CAN_TypeDef, FFA1R) == 0x214U, "CAN filter FIFO");
_Static_assert(offsetof(CAN_TypeDef, FA1R) == 0x21CU, "CAN filter enable");
_Static_assert(offsetof(CAN_TypeDef, sFilterRegister) == 0x240U,
               "CAN filter banks layout");
_Static_assert(sizeof(((CAN_TypeDef*)0)->sFilterRegister) /
                       sizeof(CAN_FilterRegister_TypeDef) ==
                   28U,
               "CAN shared filter count");
_Static_assert(CAN_MCR_INRQ == 1U && CAN_MSR_INAK == 1U &&
                   CAN_MCR_NART == 16U && CAN_MCR_RFLM == 8U &&
                   CAN_MCR_TXFP == 4U && CAN_TSR_RQCP0 == 1U &&
                   CAN_TSR_TXOK0 == 2U && CAN_TSR_ALST0 == 4U &&
                   CAN_TSR_TERR0 == 8U && CAN_TSR_ABRQ0 == 128U &&
                   CAN_TSR_TME0 == (1U << 26U) && CAN_RF0R_RFOM0 == 32U &&
                   CAN_RF0R_FOVR0 == 16U && CAN_RI0R_IDE == 4U &&
                   CAN_RI0R_RTR == 2U && CAN_ESR_BOFF == 4U &&
                   CAN_FMR_FINIT == 1U && CAN_FMR_CAN2SB == 0x3F00U,
               "CAN mechanism masks must match actual SDK");
_Static_assert(CAN1_TX_IRQn == 19 && CAN1_RX0_IRQn == 20 &&
                   CAN1_RX1_IRQn == 21 && CAN1_SCE_IRQn == 22 &&
                   CAN2_TX_IRQn == 63 && CAN2_RX0_IRQn == 64 &&
                   CAN2_RX1_IRQn == 65 && CAN2_SCE_IRQn == 66,
               "CAN exact vector identities");

#endif
