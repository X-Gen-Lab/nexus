/**
 * \file            can.c
 * \brief           Actual STM32F407 SDK layout and static bxCAN resources
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/can/stm32f407_can.h"
#include "stm32f4xx.h"
#include "system/stm32f407_clock.h"

#include "cmsis_layout.h"

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
    controller->clock_hz = NEXUS_APB1_HZ;
    controller->max_extended_id_mailbox0 = false;
    const int16_t vectors[2][4] = {
        {CAN1_TX_IRQn, CAN1_RX0_IRQn, CAN1_RX1_IRQn, CAN1_SCE_IRQn},
        {CAN2_TX_IRQn, CAN2_RX0_IRQn, CAN2_RX1_IRQn, CAN2_SCE_IRQn}};
    for (unsigned i = 0; i < 4U; ++i) {
        controller->sources[i].irq_number = vectors[controller->index][i];
        controller->sources[i].priority_group = (uint8_t)NX_CAN_SOC_IRQ_GROUP();
    }
    return nx_can_controller_initialize(controller);
}
