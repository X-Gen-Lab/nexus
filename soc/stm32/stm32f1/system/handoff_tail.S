/**
 * \file            handoff_tail.S
 * \brief           Auditable Cortex-M3 handoff after all C admission.
 * \author          Nexus Team
 */
.syntax unified
.cpu cortex-m3
.thumb
.text
.align 2
.global nx_stm32f103rb_handoff_tail
.type nx_stm32f103rb_handoff_tail, %function
.thumb_func
nx_stm32f103rb_handoff_tail:
    /* Standard AAPCS r0=VTOR, r1=MSP, r2=Thumb reset PC. */
    ldr r3, =0xe000ed08
    str r0, [r3]
    dsb sy
    isb sy
    movs r0, #0
    msr psp, r0
    isb sy
    mvns r3, r0
    mov lr, r3
    /* No old-stack reference, call or unmask may follow installing MSP. */
    msr msp, r1
    bx r2
.size nx_stm32f103rb_handoff_tail, .-nx_stm32f103rb_handoff_tail
