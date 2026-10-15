/**
 * \file            m4_vectors.S
 * \brief           Cortex-M4 kernel wiring without a C exception frame
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
.syntax unified
.thumb
.text
.align 2
/* The locked official schedule object owns both strong PendSV aliases.
 * Do not define a competing C handler or a forwarding assembly wrapper. */
#include "nexus_config.h"
#if !NEXUS_OS_EXTERNAL_TICK
.global SysTick_Handler
.type SysTick_Handler, %function
.thumb_func
SysTick_Handler:
    push {r0, lr}
    bl _tx_timer_interrupt
    pop {r0, lr}
    bx lr
.size SysTick_Handler, .-SysTick_Handler
#endif
