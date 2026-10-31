/**
 * \file            can.c
 * \brief           Actual GD32F470 SDK layout and static classic CAN resources
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_can.h"
#include "gd32f4xx_can.h"
#include "gd32f4xx_rcu.h"

/* can_layout.c checks SDK addresses; masks and vectors stay with this binding.
 */
_Static_assert(CAN_CTL_IWMOD == 1U && CAN_STAT_IWS == 1U &&
                   CAN_CTL_ARD == 16U && CAN_CTL_RFOD == 8U &&
                   CAN_CTL_TFO == 4U && CAN_TSTAT_MTF0 == 1U &&
                   CAN_TSTAT_MTFNERR0 == 2U && CAN_TSTAT_MAL0 == 4U &&
                   CAN_TSTAT_MTE0 == 8U && CAN_TSTAT_MST0 == 128U &&
                   CAN_TSTAT_TME0 == (1U << 26U) && CAN_RFIFO0_RFD0 == 32U &&
                   CAN_RFIFO0_RFO0 == 16U && CAN_RFIFOMI_FF == 4U &&
                   CAN_RFIFOMI_FT == 2U && CAN_ERR_BOERR == 4U &&
                   CAN_FCTL_FLD == 1U && CAN_FCTL_HBC1F == 0x3F00U,
               "CAN mechanism masks must match actual SDK");
_Static_assert(CAN0_TX_IRQn == 19 && CAN0_RX0_IRQn == 20 &&
                   CAN0_RX1_IRQn == 21 && CAN0_EWMC_IRQn == 22 &&
                   CAN1_TX_IRQn == 63 && CAN1_RX0_IRQn == 64 &&
                   CAN1_RX1_IRQn == 65 && CAN1_EWMC_IRQn == 66 &&
                   CAN1 - CAN0 == 0x400U,
               "CAN exact vector/controller identities");
#ifndef NX_CAN_SOC_IRQ_GROUP
#define NX_CAN_SOC_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif

/** \brief GD filters/split live in CAN0, including CAN1-only selections. */
nx_result_t nx_gd32_can_domain_initialize(nx_can_filter_domain_t* domain) {
    if (domain == NULL || domain->active_mask != 0U || domain->retained ||
        (domain->primary != 0U && domain->primary != CAN0)) {
        return NX_ERROR_INVALID;
    }
    domain->primary = CAN0;
    domain->clock_enable = &RCU_APB1EN;
    domain->primary_clock_mask = RCU_APB1EN_CAN0EN;
    domain->secondary_clock_mask = RCU_APB1EN_CAN1EN;
    return nx_can_filter_domain_initialize(domain);
}
/** \brief CAN uses 50 MHz APB1; timer's doubled 100 MHz is a different domain.
 */
nx_result_t nx_gd32_can_initialize(nx_can_controller_t* controller) {
    if (controller == NULL || controller->domain == NULL ||
        controller->index > 1U ||
        controller->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        controller->irq_policy.external_irq_count != (uint16_t)IPA_IRQn + 1U ||
        controller->domain->primary != CAN0 ||
        controller->domain->clock_enable != &RCU_APB1EN ||
        controller->domain->primary_clock_mask != RCU_APB1EN_CAN0EN ||
        controller->domain->secondary_clock_mask != RCU_APB1EN_CAN1EN) {
        return NX_ERROR_INVALID;
    }
    uintptr_t address = controller->index == 0U ? CAN0 : CAN1;
    if (controller->registers != 0U && controller->registers != address) {
        return NX_ERROR_INVALID;
    }
    controller->registers = address;
    controller->clock_hz = 50000000U;
    const int16_t vectors[2][4] = {
        {CAN0_TX_IRQn, CAN0_RX0_IRQn, CAN0_RX1_IRQn, CAN0_EWMC_IRQn},
        {CAN1_TX_IRQn, CAN1_RX0_IRQn, CAN1_RX1_IRQn, CAN1_EWMC_IRQn}};
    for (unsigned i = 0; i < 4U; ++i) {
        controller->sources[i].irq_number = vectors[controller->index][i];
        controller->sources[i].priority_group = (uint8_t)NX_CAN_SOC_IRQ_GROUP();
    }
    return nx_can_controller_initialize(controller);
}
