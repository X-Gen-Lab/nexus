/**
 * \file            instance.h
 * \brief           Private F103xB silent loopback state.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_CAN_INSTANCE_H
#define NEXUS_STM32F1_CAN_INSTANCE_H
#include "nexus/io/can.h"

/** \brief One borrowed request and a bounded cancellation deadline. */
typedef struct {
    nx_can_tx_request_t* tx;
    nx_time_us_t drain_deadline;
    nx_result_t abort_result;
    uint8_t effects;
} nx_stm32f1_can_t;

extern const nx_can_ops_t nx_stm32f1_can_ops;
nx_result_t nx_stm32f1_can_admit(const nx_stm32f1_can_t* port);
nx_result_t nx_stm32f1_can_prepare(nx_stm32f1_can_t* port);
nx_result_t nx_stm32f1_can_close(nx_stm32f1_can_t* port);
#endif
