/**
 * \file            normal.h
 * \brief           F103RB external CAN transceiver and pad ownership.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F1_CAN_NORMAL_H
#define NEXUS_STM32F1_CAN_NORMAL_H

#include "drivers/can/instance.h"
#include "drivers/gpio/instance.h"

typedef struct {
    nx_stm32f1_can_t port;
    nx_stm32f1_pads_t can_pads;
    nx_stm32f1_pads_t rs_pad;
    bool afio_gate_added;
    bool rs_configured;
} nx_stm32f1_can_normal_t;

nx_result_t nx_stm32f1_can_normal_admit(const nx_stm32f1_can_normal_t* owner);
nx_result_t nx_stm32f1_can_normal_prepare(nx_stm32f1_can_normal_t* owner);
nx_result_t nx_stm32f1_can_normal_close(nx_stm32f1_can_normal_t* owner);
void nx_stm32f1_can_block(nx_stm32f1_can_t* port);
#endif
