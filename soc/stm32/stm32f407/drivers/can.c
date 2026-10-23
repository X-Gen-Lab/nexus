/**
 * \file            can.c
 * \brief           Actual STM32F407 SDK layout and static bxCAN resources
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_can.h"
#include "stm32f407xx.h"
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
#ifndef NX_CAN_SOC_IRQ_GROUP
#define NX_CAN_SOC_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif

/** \brief Primary controller owns filters and its clock even for CAN2-only. */
nx_result_t nx_stm32_can_domain_initialize(nx_can_filter_domain_t* domain) {
    if (domain == NULL || domain->active_mask != 0U || domain->retained ||
        (domain->primary != 0U && domain->primary != CAN1_BASE)) {
        return NX_ERROR_INVALID;
    }
    domain->primary = CAN1_BASE;
    domain->clock_enable = &RCC->APB1ENR;
    domain->primary_clock_mask = RCC_APB1ENR_CAN1EN;
    domain->secondary_clock_mask = RCC_APB1ENR_CAN2EN;
    return nx_can_filter_domain_initialize(domain);
}
/** \brief CAN runs from 42 MHz APB1, never the doubled timer clock. */
nx_result_t nx_stm32_can_initialize(nx_can_controller_t* controller) {
    if (controller == NULL || controller->domain == NULL ||
        controller->index > 1U ||
        controller->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        controller->irq_policy.external_irq_count != (uint16_t)FPU_IRQn + 1U ||
        controller->domain->primary != CAN1_BASE ||
        controller->domain->clock_enable != &RCC->APB1ENR ||
        controller->domain->primary_clock_mask != RCC_APB1ENR_CAN1EN ||
        controller->domain->secondary_clock_mask != RCC_APB1ENR_CAN2EN) {
        return NX_ERROR_INVALID;
    }
    uintptr_t address = controller->index == 0U ? CAN1_BASE : CAN2_BASE;
    if (controller->registers != 0U && controller->registers != address) {
        return NX_ERROR_INVALID;
    }
    controller->registers = address;
    controller->clock_hz = 42000000U;
    const int16_t vectors[2][4] = {
        {CAN1_TX_IRQn, CAN1_RX0_IRQn, CAN1_RX1_IRQn, CAN1_SCE_IRQn},
        {CAN2_TX_IRQn, CAN2_RX0_IRQn, CAN2_RX1_IRQn, CAN2_SCE_IRQn}};
    for (unsigned i = 0; i < 4U; ++i) {
        controller->sources[i].irq_number = vectors[controller->index][i];
        controller->sources[i].priority_group = (uint8_t)NX_CAN_SOC_IRQ_GROUP();
    }
    return nx_can_controller_initialize(controller);
}
