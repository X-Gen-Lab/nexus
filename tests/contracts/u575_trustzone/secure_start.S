/* Reference reset ownership; all attribution changes remain in the SoC owner. */
.syntax unified
.thumb
.section .isr_vector,"a",%progbits
.word __stack_top
.word Secure_Reset_Handler
.rept 140
.word Secure_Fault_Handler
.endr
.section .text.Secure_Reset_Handler,"ax",%progbits
.global Secure_Reset_Handler
.type Secure_Reset_Handler,%function
.thumb_func
Secure_Reset_Handler:
    cpsid i
    /* Establish the Secure vector and bounded stack before unmasking. */
    ldr r0, =0xe000ed08
    ldr r1, =0x0c000000
    str r1, [r0]
    ldr r0, =__stack_bottom
    msr msplim, r0
    dsb sy
    isb sy
    ldr r0, =__data_load
    ldr r1, =__data_start
    ldr r2, =__data_end
1:  cmp r1, r2
    bhs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:  ldr r1, =__bss_start
    ldr r2, =__bss_end
    movs r3, #0
3:  cmp r1, r2
    bhs 4f
    str r3, [r1], #4
    b 3b
4:  bl nx_reference_secure_main
    b Secure_Fault_Handler
.size Secure_Reset_Handler, .-Secure_Reset_Handler
.section .text.Secure_Fault_Handler,"ax",%progbits
.global Secure_Fault_Handler
.type Secure_Fault_Handler,%function
.thumb_func
Secure_Fault_Handler:
    cpsid i
5:  wfi
    b 5b
.size Secure_Fault_Handler, .-Secure_Fault_Handler
