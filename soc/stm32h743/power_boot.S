/**
 * \file            power_boot.S
 * \brief           H743 pre-RAM bounded LDO power entry
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "boot_registers.h"
.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.eabi_attribute 28, 1
.thumb
.section .text.ExitRun0Mode,"ax",%progbits
.global ExitRun0Mode
.type ExitRun0Mode,%function
.thumb_func
ExitRun0Mode:
    /* Run-star forbids RAM writes, including pushes/spills to MSP. Only
     * scratch registers r0-r2 and the fixed PWR MMIO addresses are used. */
    movw r0, #(NX_H743_BOOT_PWR_CR3 & 0xffff)
    movt r0, #(NX_H743_BOOT_PWR_CR3 >> 16)
    ldr r1, [r0]
    bic r1, r1, #NX_H743_BOOT_BYPASS
    orr r1, r1, #NX_H743_BOOT_LDO
    str r1, [r0]
    dsb sy
    ldr r1, [r0]
    and r1, r1, #(NX_H743_BOOT_LDO | NX_H743_BOOT_BYPASS)
    cmp r1, #NX_H743_BOOT_LDO
    bne .Lbad_supply
    movw r0, #(NX_H743_BOOT_PWR_CSR1 & 0xffff)
    movt r0, #(NX_H743_BOOT_PWR_CSR1 >> 16)
    movw r2, #(NX_H743_BOOT_POLLS & 0xffff)
    movt r2, #(NX_H743_BOOT_POLLS >> 16)
.Lpoll:
    ldr r1, [r0]
    tst r1, #NX_H743_BOOT_READY
    bne .Lready
    subs r2, r2, #1
    bne .Lpoll
    movs r0, #1
    b .Lfailed
.Lbad_supply:
    movs r0, #2
.Lfailed:
    cpsid i
    dsb sy
    isb sy
.Lhalt:
    b .Lhalt
.Lready:
    dsb sy
    isb sy
    bx lr
.size ExitRun0Mode, .-ExitRun0Mode
