/**
 * \file            h743_flash_ram_checker.S
 * \brief           Real ARM code fixture for the RAM closure checker tests
 * \author          Nexus Team
 *
 * This deliberately small checker input is not a Flash provider. GoogleTest
 * exercises the actual production protocol separately. Mutations below must
 * alter linked instructions, literals, vectors or load bytes, never a claimed
 * qualification flag in a JSON record.
 */
.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb
.eabi_attribute 28, 1

.macro function name, section
.section \section,"ax",%progbits
.balign 4
.global \name
.type \name,%function
.thumb_func
\name:
.endm
.macro endfunction name
.size \name,.-\name
.endm

.section .isr_vector,"a",%progbits
.balign 1024
.global g_pfnVectors
g_pfnVectors:
.word 0x24080000
#ifdef BAD_VECTOR_RESET
.word Flash_Helper
#else
.word Reset_Handler
#endif
#ifdef BAD_ORIGINAL_NMI
.word Flash_Helper
#else
.word Default_Handler
#endif
.word Default_Handler
.rept 162
.word Default_Handler
.endr

function Reset_Handler, .text.Reset_Handler
#if SELECTED
#ifndef MISSING_COPY_CALL
#ifdef COPY_SKIPPED_BY_REAL_BRANCH
movs r0,#0
cmp r0,#0
beq 1f
#endif
bl nx_stm32h743_flash_ram_initialize
#endif
#endif
1: b 1b
endfunction Reset_Handler
function Default_Handler, .text.Default_Handler
1: b 1b
endfunction Default_Handler
function Flash_Helper, .text.Flash_Helper
bx lr
endfunction Flash_Helper
.section .rodata.flash_value,"a",%progbits
.global flash_value
flash_value: .word 0x13579bdf

#if SELECTED
function nx_stm32h743_flash_ram_initialize, .text.ram_initialize
push {r4-r7,lr}
ldr r0,=__nx_flash_ram_load_start
#ifdef WRONG_COPY_SOURCE
adds r0,#4
#endif
ldr r1,=__nx_flash_ram_start
ldr r2,=__nx_flash_ram_end
1: cmp r1,r2
bhs 2f
ldrb r3,[r0],#1
strb r3,[r1],#1
b 1b
2: ldr r0,=__nx_flash_ram_load_start
ldr r1,=__nx_flash_ram_start
ldr r2,=__nx_flash_ram_end
3: cmp r1,r2
bhs 4f
ldrb r3,[r0],#1
ldrb r4,[r1],#1
#ifdef MASKED_SUCCESS_READBACK
bic r3,r3,#128
bic r4,r4,#128
#endif
cmp r3,r4
bne 9f
b 3b
4: ldr r0,=g_pfnVectors
ldr r1,=__nx_flash_ram_vector_start
movs r2,#166
5: ldr r3,[r0],#4
str r3,[r1],#4
subs r2,#1
bne 5b
ldr r1,=__nx_flash_ram_vector_start
#ifdef FLASH_NMI_VECTOR
ldr r0,=Flash_Helper
#else
ldr r0,=nx_stm32h743_flash_ram_nmi
#endif
str r0,[r1,#8]
ldr r0,=nx_stm32h743_flash_ram_hardfault
str r0,[r1,#12]
movs r0,#0
pop {r4-r7,pc}
9:
#ifdef ONLY_LSB_MISMATCH_REJECTS
bic r3,r3,#254
bic r4,r4,#254
cmp r3,r4
bne 8f
movs r0,#0
pop {r4-r7,pc}
8:
#endif
#ifdef ONLY_FIRST_READBACK_REJECTS
ldr r0,=__nx_flash_ram_start
adds r0,#1
cmp r1,r0
beq 8f
movs r0,#0
pop {r4-r7,pc}
8:
#endif
movs r0,#1
pop {r4-r7,pc}
.ltorg
endfunction nx_stm32h743_flash_ram_initialize

#ifndef EXTERNAL_CPU_BRIDGE
function nx_stm32h743_flash_ram_enter, .nx_flash_ram.bridge
#ifdef UNKNOWN_INCOMING_LR_PATH
push {lr}
pop {r3}
ldr.w r2,9f
cmp r3,r2
beq 8f
ldr r2,=flash_value
ldr r2,[r2]
8:
#endif
push {r4-r7,lr}
mrs r4,control
mrs r5,msp
mrs r6,psp
mrs r7,primask
cpsid i
dsb
isb
#ifdef BAD_STACK_TOP
addw r1,r0,#1692
#elif defined(SHORT_STACK)
addw r1,r0,#792
#elif defined(MARGINAL_STACK)
addw r1,r0,#920
#elif defined(FITTED_STACK)
addw r1,r0,#984
#else
addw r1,r0,#1688
#endif
msr msp,r1
isb
bic r1,r4,#2
#ifdef CLEAR_FPCA
bic r1,r1,#4
#endif
msr control,r1
isb
bl nx_stm32h743_flash_ram_transaction
#ifdef CORRUPT_SAVED_MSP_IDENTITY
bic r5,r5,#16
#endif
#ifdef WRONG_MSP_RESTORE
msr msp,r6
#else
msr msp,r5
#endif
isb
#ifdef CORRUPT_SAVED_PSP_IDENTITY
bic r6,r6,#16
#endif
#ifdef WRONG_PSP_RESTORE
msr psp,r5
#else
msr psp,r6
#endif
#ifdef WRONG_CONTROL_RESTORE
msr control,r1
#else
msr control,r4
#endif
isb
dsb
#ifdef WRONG_MASK_RESTORE
msr primask,r4
#else
msr primask,r7
#endif
isb
#ifdef FORGED_INCOMING_LR_RETURN
pop {r4-r7}
add sp,#4
ldr.w lr,9f
bx lr
.balign 4
9: .word 0xfffffffd
#else
pop {r4-r7,pc}
#endif
#ifdef UNKNOWN_INCOMING_LR_PATH
.balign 4
9: .word 0xfffffffd
.ltorg
#endif
endfunction nx_stm32h743_flash_ram_enter
#endif

function nx_stm32h743_flash_ram_transaction, .nx_flash_ram.transaction
push {r4-r7,lr}
sub sp,#4
ldr r4,=0xe000ed08
ldr r5,[r4]
ldr r6,=__nx_flash_ram_vector_start
str r6,[r4]
dsb
isb
#ifdef EARLY_VTOR_RESTORE
str r5,[r4]
dsb
isb
#endif
bl nx_stm32h743_flash_ram_pulse
str r5,[r4]
ldr r6,[r4]
cmp r5,r6
bne nx_stm32h743_flash_ram_hardfault
dsb
isb
add sp,#4
pop {r4-r7,pc}
.ltorg
endfunction nx_stm32h743_flash_ram_transaction

function nx_stm32h743_flash_ram_pulse, .nx_flash_ram.pulse
push {r4-r7,lr}
sub sp,#44
#ifdef LOAD_PC_INDIRECT
ldr r3,=9f
ldr.w pc,[r3],#4
#endif
#ifdef WIDE_SP_WRITE
addw sp,sp,#4
#endif
#ifdef CLOBBER_UNOBSERVED_R8
bic r8,r8,#128
#endif
#ifdef UNKNOWN_ARGUMENT_PATH
cmp r2,#0
bne 8f
ldr r4,=flash_value
ldr r4,[r4]
8:
#endif
#ifdef WRONG_ADD_CARRY_PATH
movs r0,#255
adds r0,#1
bcs 7f
#elif defined(WRONG_SUB_CARRY_PATH)
movs r0,#0
subs r0,#1
bcs 7f
#elif defined(WRONG_MOV_CARRY_PATH)
movs r0,#0
cmp r0,#1
movs r0,#0
bcs 7f
#elif defined(WRONG_ADD_OVERFLOW_PATH)
ldr r0,=0x7fffffff
adds r0,#1
bvc 7f
#endif
#if defined(WRONG_ADD_CARRY_PATH) || defined(WRONG_SUB_CARRY_PATH) || \
    defined(WRONG_MOV_CARRY_PATH) || defined(WRONG_ADD_OVERFLOW_PATH)
ldr r4,=flash_value
ldr r4,[r4]
7:
#endif
#ifdef FLASH_CALL
bl Flash_Helper
#elif defined(FLASH_VENEER)
bl Test_Veneer
#elif defined(INDIRECT_CALL)
blx r3
#else
bl Ram_Helper
#endif
#ifdef FLASH_LITERAL
ldr r4,=flash_value
ldr r4,[r4]
#else
ldr r4,=0x52002000
#endif
#ifdef FP_INSTRUCTION
vmov s0,r4
#endif
add sp,#44
pop {r4-r7,pc}
#ifdef LOAD_PC_INDIRECT
.balign 4
9: .word Flash_Helper
#endif
.ltorg
endfunction nx_stm32h743_flash_ram_pulse
function Ram_Helper, .nx_flash_ram.helper
movs r0,#0
bx lr
endfunction Ram_Helper
#ifndef MISSING_REQUESTED_ROOT
function Test_Veneer, .nx_flash_ram.veneer
#ifdef FLASH_VENEER
ldr pc,=Flash_Helper
.ltorg
#else
b Ram_Helper
#endif
endfunction Test_Veneer
#endif
function nx_stm32h743_flash_ram_nmi, .nx_flash_ram.nmi
1: b 1b
endfunction nx_stm32h743_flash_ram_nmi
function nx_stm32h743_flash_ram_hardfault, .nx_flash_ram.hardfault
1: b 1b
endfunction nx_stm32h743_flash_ram_hardfault

.section .bss.nx_flash_ram_workspace,"aw",%nobits
.balign 1024
.global g_nx_stm32h743_flash_ram_workspace
g_nx_stm32h743_flash_ram_workspace:
.global __nx_flash_ram_vector_start
__nx_flash_ram_vector_start: .space 664
.global __nx_flash_ram_vector_end
__nx_flash_ram_vector_end:
.global __nx_flash_ram_stack_start
__nx_flash_ram_stack_start:
#ifdef SHORT_STACK
.space 128
#elif defined(MARGINAL_STACK)
.space 256
#elif defined(FITTED_STACK)
.space 320
#else
.space 1024
#endif
.global __nx_flash_ram_stack_end
__nx_flash_ram_stack_end:
.space 128
.global __nx_flash_ram_workspace_end
__nx_flash_ram_workspace_end:
#endif
.section .data.fixture,"aw",%progbits
.global initialized_data
initialized_data: .word 0x2468ace0

#ifdef EXTERNAL_CPU_BRIDGE
function nx_stm32h743_flash_ram_terminal, .nx_flash_ram.terminal
1: b 1b
endfunction nx_stm32h743_flash_ram_terminal
#endif
