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
    /* Protected captures survive C independently of its mutable guard. */
    ldr r12, =g_nx_stm32h743_flash_ram_workspace
    movs r2, #1
    str.w r2, [r12, #NX_H743_RAM_GUARD_EFFECT_OFFSET]
    ldr r1, =NX_H743_RAM_MPU_BASE
    ldr.w r8, [r1, #4]
    ldr.w r9, [r1, #8]
    movs r2, #15
    str r2, [r1, #8]
    ldr r3, [r1, #8]
    cmp r3, r2
    bne .Lentry_fault
    ldr.w r10, [r1, #12]
    bic r10, r10, #31
    ldr.w r11, [r1, #16]
    str.w r9, [r1, #8]
    ldr r2, [r1, #8]
    cmp r2, r9
    bne .Lentry_fault
    movs r2, #0
    str.w r2, [r12, #NX_H743_RAM_GUARD_EFFECT_OFFSET]
    dsb sy
    isb sy
    /* C publishes RAM vectors and owns protection, trigger and drain. */
    bl nx_stm32h743_flash_ram_transaction
    bl nx_stm32h743_flash_ram_return
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
.Lentry_fault:
    ldr r0, =g_nx_stm32h743_flash_ram_workspace
    movs r1, #4
    b nx_stm32h743_flash_ram_terminal
    .ltorg
.size nx_stm32h743_flash_ram_enter, .-nx_stm32h743_flash_ram_enter

/* Protected r8-r11 captures are independent of mutable C guard state.
 * Every refusal takes the nonreturning RAM diagnostic path. */
.balign 4
.global nx_stm32h743_flash_ram_return
.type nx_stm32h743_flash_ram_return,%function
.thumb_func
nx_stm32h743_flash_ram_return:
    ldr r1, =NX_H743_RAM_FLASH_BASE
    ldr r2, [r1, #16]
    ldr.w r3, [r1, #272]
    orrs r2, r3
    tst r2, #7
    bne .Lreturn_fault
    ldr r12, =g_nx_stm32h743_flash_ram_workspace
    ldr.w r2, [r12, #NX_H743_RAM_GUARD_EFFECT_OFFSET]
    cmp r2, #0
    bne .Lreturn_fault
    ldr.w r2, [r12, #NX_H743_RAM_BANK_OFFSET]
    cmp r2, #1
    bhi .Lreturn_fault
    add.w r1, r1, r2, lsl #8
    ldr.w r2, [r12, #NX_H743_RAM_SAVED_CR_OFFSET]
    bic r3, r2, #0x30
    cmp r3, #0
    bne .Lreturn_fault
    orr r2, r2, #1
    str r2, [r1, #12]
    dsb sy
    ldr r3, [r1, #12]
    cmp r3, r2
    bne .Lreturn_fault
    bic r2, r8, #4
    cmp r2, #0
    bne .Lreturn_fault
    mov r2, r9
    cmp r2, #15
    bhi .Lreturn_fault
    ldr r1, =NX_H743_RAM_MPU_BASE
    ldr r2, [r1, #4]
    cmp r2, r8
    bne .Lreturn_fault
    movs r2, #1
    str.w r2, [r12, #NX_H743_RAM_GUARD_EFFECT_OFFSET]
    movs r2, #15
    str r2, [r1, #8]
    ldr r3, [r1, #8]
    cmp r3, r2
    bne .Lreturn_fault
    ldr r2, [r1, #12]
    bic r2, r2, #31
    cmp r2, r10
    bne .Lreturn_fault
    ldr r2, [r1, #16]
    cmp r2, r11
    bne .Lreturn_fault
    str.w r9, [r1, #8]
    ldr r2, [r1, #8]
    cmp r2, r9
    bne .Lreturn_fault
    movs r2, #0
    str.w r2, [r12, #NX_H743_RAM_GUARD_EFFECT_OFFSET]
    dsb sy
    isb sy
    ldr r1, =NX_H743_RAM_VTOR_ADDRESS
    ldr r2, =g_pfnVectors
    str r2, [r1]
    dsb sy
    isb sy
    ldr r3, [r1]
    cmp r3, r2
    bne .Lreturn_fault
    bx lr
.Lreturn_fault:
    ldr r0, =g_nx_stm32h743_flash_ram_workspace
    movs r1, #4
    b nx_stm32h743_flash_ram_terminal
    .ltorg
.size nx_stm32h743_flash_ram_return, .-nx_stm32h743_flash_ram_return

/* The sole dynamic write base: eight fixed staged reads and eight stores. */
.balign 4
.global nx_stm32h743_flash_ram_program_words
.type nx_stm32h743_flash_ram_program_words,%function
.thumb_func
nx_stm32h743_flash_ram_program_words:
    ldr r0, =g_nx_stm32h743_flash_ram_workspace
    ldr.w r1, [r0, #NX_H743_RAM_DESTINATION_OFFSET]
    ldr.w r2, [r0, #NX_H743_RAM_WORDS_OFFSET + 0]
    str r2, [r1, #0]
    ldr.w r2, [r0, #NX_H743_RAM_WORDS_OFFSET + 4]
    str r2, [r1, #4]
    ldr.w r2, [r0, #NX_H743_RAM_WORDS_OFFSET + 8]
    str r2, [r1, #8]
    ldr.w r2, [r0, #NX_H743_RAM_WORDS_OFFSET + 12]
    str r2, [r1, #12]
    ldr.w r2, [r0, #NX_H743_RAM_WORDS_OFFSET + 16]
    str r2, [r1, #16]
    ldr.w r2, [r0, #NX_H743_RAM_WORDS_OFFSET + 20]
    str r2, [r1, #20]
    ldr.w r2, [r0, #NX_H743_RAM_WORDS_OFFSET + 24]
    str r2, [r1, #24]
    ldr.w r2, [r0, #NX_H743_RAM_WORDS_OFFSET + 28]
    str r2, [r1, #28]
    bx lr
    .ltorg
.size nx_stm32h743_flash_ram_program_words, .-nx_stm32h743_flash_ram_program_words
