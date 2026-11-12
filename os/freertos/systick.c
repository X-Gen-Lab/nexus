/**
 * \file            systick.c
 * \brief           Complete kernel stores before the SysTick exception return
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "FreeRTOS.h"
#include "nexus/arch/arch.h"

void nx_freertos_kernel_tick_handler(void);
void SysTick_Handler(void) PRIVILEGED_FUNCTION;

/** \brief The selected kernel retains the actual tick and PendSV policy. */
void SysTick_Handler(void) {
    nx_freertos_kernel_tick_handler();
    nx_arch_dsb();
}
