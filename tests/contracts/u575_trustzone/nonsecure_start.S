/* NS is already unprivileged: initialize only its own ordinary RAM. */
.syntax unified
.thumb
.section .isr_vector,"a",%progbits
.word __stack_top
.word NonSecure_Reset_Handler
.rept 140
.word 0
.endr
.section .text.NonSecure_Reset_Handler,"ax",%progbits
.global NonSecure_Reset_Handler
.type NonSecure_Reset_Handler,%function
.thumb_func
NonSecure_Reset_Handler:
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
4:  bl nx_reference_nonsecure_main
5:  wfi
    b 5b
.size NonSecure_Reset_Handler, .-NonSecure_Reset_Handler
