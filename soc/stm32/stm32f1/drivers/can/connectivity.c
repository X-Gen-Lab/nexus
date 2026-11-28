/**
 * \file            connectivity.c
 * \brief           Exact F105/F107 bxCAN clock, register and vector binding.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/can/connectivity.h"
#include "private/hardware.h"
#include "system/platform.h"

#include "cmsis_layout.h"

#if !defined(STM32F105xC) && !defined(STM32F107xC)
#error "The STM32F1 dual bxCAN binding requires a reviewed Connectivity part"
#endif
#ifndef NEXUS_CLOCK_APB1_HZ
#include "nexus_config.h"
#endif
_Static_assert(NEXUS_CLOCK_APB1_HZ == 8000000U,
               "Connectivity CAN silent transport requires 8 MHz APB1");

#ifndef NX_STM32F1_CONN_CAN_IRQ_BUSY
#define NX_STM32F1_CONN_CAN_IRQ_BUSY(irq)                                      \
    (NVIC_GetEnableIRQ(irq) != 0U || NVIC_GetPendingIRQ(irq) != 0U ||          \
     NVIC_GetActive(irq) != 0U)
#endif
#ifndef NX_STM32F1_CONN_CAN_IRQ_GROUP
#define NX_STM32F1_CONN_CAN_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif

#define CONN_CAN_GATES  (RCC_APB1ENR_CAN1EN | RCC_APB1ENR_CAN2EN)
#define CONN_CAN_RESETS (RCC_APB1RSTR_CAN1RST | RCC_APB1RSTR_CAN2RST)

static const IRQn_Type vectors[2][4] = {
    {CAN1_TX_IRQn, CAN1_RX0_IRQn, CAN1_RX1_IRQn, CAN1_SCE_IRQn},
    {CAN2_TX_IRQn, CAN2_RX0_IRQn, CAN2_RX1_IRQn, CAN2_SCE_IRQn},
};

/** \brief The primary gate and filters have one owner even in CAN2-only. */
static nx_result_t bind_domain(nx_can_filter_domain_t* domain) {
    if (domain->primary != 0U &&
        (domain->primary != CAN1_BASE ||
         domain->clock_enable != &RCC->APB1ENR ||
         domain->primary_clock_mask != RCC_APB1ENR_CAN1EN ||
         domain->secondary_clock_mask != RCC_APB1ENR_CAN2EN)) {
        return NX_ERROR_INVALID;
    }
    if (domain->retained || (RCC->APB1RSTR & CONN_CAN_RESETS) != 0U ||
        (!domain->configured && (RCC->APB1ENR & CONN_CAN_GATES) != 0U)) {
        return NX_ERROR_BUSY;
    }
    domain->primary = CAN1_BASE;
    domain->clock_enable = &RCC->APB1ENR;
    domain->primary_clock_mask = RCC_APB1ENR_CAN1EN;
    domain->secondary_clock_mask = RCC_APB1ENR_CAN2EN;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_connectivity_can_prepare(
    nx_can_controller_t* port,
    const nx_stm32f1_connectivity_can_config_t* config) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->domain == NULL ||
        config->events == NULL || config->capacity == 0U ||
        config->index > 1U || config->priority >= (1U << __NVIC_PRIO_BITS) ||
        config->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        config->irq_policy.external_irq_count != (uint16_t)OTG_FS_IRQn + 1U)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if (port->snapshot.started || port->publishers != 0U ||
        (config->domain->active_mask & (1U << config->index)) != 0U)
        return NX_ERROR_BUSY;
    for (unsigned role = 0U; role < 4U; ++role) {
        if (NX_STM32F1_CONN_CAN_IRQ_BUSY(vectors[config->index][role]))
            return NX_ERROR_BUSY;
    }
    nx_result_t bound = bind_domain(config->domain);
    if (bound != NX_SUCCESS)
        return bound;
    port->registers = config->index == 0U ? CAN1_BASE : CAN2_BASE;
    port->domain = config->domain;
    port->events = config->events;
    port->capacity = config->capacity;
    port->timing = config->timing;
    port->irq_policy = config->irq_policy;
    port->clock_hz = NEXUS_CLOCK_APB1_HZ;
    port->drain_timeout_us = config->drain_timeout_us;
    port->poll_limit = config->poll_limit;
    port->mode = NX_CAN_SILENT_LOOPBACK;
    port->index = config->index;
    port->fifo_order = config->fifo_order;
    port->max_extended_id_mailbox0 = false;
    for (unsigned role = 0U; role < 4U; ++role) {
        port->sources[role] = (nx_irq_source_t){
            .irq_number = (int16_t)vectors[config->index][role],
            .priority = config->priority,
            .priority_group = (uint8_t)NX_STM32F1_CONN_CAN_IRQ_GROUP(),
        };
    }
    return nx_can_controller_initialize(port);
}

nx_result_t nx_stm32f1_connectivity_can_close(nx_can_controller_t* port) {
    return nx_classic_can_ops.stop(port);
}

nx_result_t
nx_stm32f1_connectivity_can_domain_close(nx_can_filter_domain_t* domain) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (domain == NULL)
        return NX_ERROR_INVALID;
    if (domain->primary == 0U)
        return NX_SUCCESS;
    return nx_can_filter_domain_stop(domain);
}
