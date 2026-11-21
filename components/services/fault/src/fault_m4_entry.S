/*
 * \file            fault_m4_entry.S
 * \brief           One selected M4F exception entry with an explicit panic stack
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
.syntax unified
.thumb
.section .text.nx_fault_m4_exception_entry, "ax", %progbits
.global nx_fault_m4_exception_entry
.type nx_fault_m4_exception_entry, %function
.thumb_func
nx_fault_m4_exception_entry:
    mov r0, lr
    mrs r1, msp
    mrs r2, psp
    mrs r3, ipsr
    ldr r12, =nx_fault_m4_emergency_stack_top
    ldr r12, [r12]
    cmp r12, #0
    beq .Lpark
    tst r12, #7
    bne .Lpark
    msr msp, r12
    b nx_fault_m4_dispatch
.Lpark:
    b .Lpark
.size nx_fault_m4_exception_entry, .-nx_fault_m4_exception_entry

/* Selecting this OBJECT target establishes one strong owner for these vectors.
 * Another strong handler correctly produces a link conflict. */
.global HardFault_Handler
.thumb_set HardFault_Handler, nx_fault_m4_exception_entry
.global MemManage_Handler
.thumb_set MemManage_Handler, nx_fault_m4_exception_entry
.global BusFault_Handler
.thumb_set BusFault_Handler, nx_fault_m4_exception_entry
.global UsageFault_Handler
.thumb_set UsageFault_Handler, nx_fault_m4_exception_entry

.section .note.GNU-stack, "", %progbits
