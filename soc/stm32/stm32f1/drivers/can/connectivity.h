/**
 * \file            connectivity.h
 * \brief           Static F105/F107 bxCAN and shared filter-domain connection.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F1_CONNECTIVITY_CAN_H
#define NEXUS_STM32F1_CONNECTIVITY_CAN_H
#include "can_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_can_filter_domain_t* domain;
    nx_can_event_t* events;
    size_t capacity;
    nx_can_timing_t timing;
    nx_irq_policy_t irq_policy;
    uint32_t drain_timeout_us;
    uint32_t poll_limit;
    uint8_t index;
    uint8_t priority;
    bool fifo_order;
} nx_stm32f1_connectivity_can_config_t;

nx_result_t nx_stm32f1_connectivity_can_prepare(
    nx_can_controller_t* port,
    const nx_stm32f1_connectivity_can_config_t* config);
nx_result_t nx_stm32f1_connectivity_can_close(nx_can_controller_t* port);
nx_result_t
nx_stm32f1_connectivity_can_domain_close(nx_can_filter_domain_t* domain);

#ifdef __cplusplus
}
#endif
#endif
