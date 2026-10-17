/**
 * \file            flash_ram_enter.S
 * \brief           H743 private MSP bridge with exact caller CPU restoration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_flash_ram_layout.h"
.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.eabi_attribute 28, 1
.thumb
.section .nx_flash_ram.enter,"ax",%progbits
.balign 4
.global nx_stm32h743_flash_ram_enter
.type nx_stm32h743_flash_ram_enter,%function
.thumb_func
nx_stm32h743_flash_ram_enter:
    /* Save the incoming mask before withdrawing configurable interrupts.
     * The original Thread stack is touched only while both banks are idle. */
    mrs r12, primask
    cpsid i
    push.w {r4-r11, lr}
    mrs r4, msp
    mrs r5, psp
    mrs r6, control
    mov r7, r12
    addw r1, r0, #NX_H743_FLASH_RAM_STACK_TOP_BYTES
    msr msp, r1
    /* Preserve FPCA and privilege. Only Thread's stack selector changes. */
    bic r1, r6, #2
    msr control, r1
    isb sy
    /* C owns VTOR publication/readback, MPU and the physical pulse. It can
     * return only after physical drain and proven protection restoration. */
    bl nx_stm32h743_flash_ram_transaction
    mov r9, r0
    msr psp, r5
    msr msp, r4
    msr control, r6
    isb sy
    mov r0, r9
    mov r12, r7
    pop.w {r4-r11, lr}
    /* Restore the exact incoming mask after all stack and callee state. */
    msr primask, r12
    bx lr
.size nx_stm32h743_flash_ram_enter, .-nx_stm32h743_flash_ram_enter
