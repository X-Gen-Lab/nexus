/**
 * \file            program.c
 * \brief           Actual C and Thumb mutations for the static RAM auditor
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define RAM __attribute__((section(".nx_flash_ram.text"), noinline, used))
typedef unsigned int word_t;
unsigned char g_nx_stm32h743_flash_ram_workspace[1800]
    __attribute__((section(".bss.workspace"), aligned(1024)));
volatile word_t diagnostic;
const word_t g_pfnVectors[166] = {0};
void Reset_Handler(void) {
    for (;;) {
    }
}
__attribute__((noinline)) word_t Flash_Helper(word_t value) {
    return value + 7U;
}
#ifdef BAD_FLASH_READ_CALL_RETURN
RAM word_t RamConstantPointer(void) {
    return 0x08000000U;
}
#endif
RAM word_t RamLeaf(word_t value) {
#ifdef DYNAMIC_STACK
    volatile word_t* lane = __builtin_alloca(value * sizeof(word_t));
#elif defined(LARGE_STACK)
    volatile word_t lane[240];
#else
    volatile word_t lane[4];
#endif
    lane[0] = value;
#ifdef VALID_UNKNOWN_DATA_READ
    /* An unresolved argument is deliberately not an owned-address proof. */
    lane[0] = *(const volatile word_t*)(unsigned long)value;
#endif
#if defined(BAD_FLASH_READ_JOIN) || defined(BAD_FLASH_READ_UNKNOWN_JOIN)
    __asm__ volatile("cmp r0,#0\nbeq 1f\n"
#ifdef BAD_FLASH_READ_UNKNOWN_JOIN
                     "nop\n"
#else
                     "movs r3,#0\n"
#endif
                     "b 2f\n1: movw r3,#0\nmovt r3,#0x0800\n2: ldr r2,[r3]" ::
                         : "r2", "r3", "cc", "memory");
#endif
#ifdef BAD_CONSTANT_LOOP
    __asm__ volatile("movs r3,#0\n1: adds r3,#1\ncmp r0,#0\nbne 1b" ::
                         : "r3", "cc");
#endif
#ifdef BAD_FLASH_DATA_READ
    __asm__ volatile("movw r3,#0\nmovt r3,#0x0800\nldr r2,[r3]" ::
                         : "r2", "r3", "memory");
#endif
#ifdef BAD_FLASH_DATA_LITERAL
    __asm__ volatile("ldr r3,1f\nldr r2,[r3]\nb 2f\n.balign 4\n"
                     "1: .word 0x08000000\n2:" ::
                         : "r2", "r3", "memory");
#endif
#ifdef BAD_FLASH_READ_PARTIAL_BOUNDARY
    __asm__ volatile("movw r3,#0xfffe\nmovt r3,#0x07ff\nldr r2,[r3]" ::
                         : "r2", "r3", "memory");
#endif
#ifdef BAD_FLASH_READ_CALL_RETURN
    lane[0] = *(volatile word_t*)(unsigned long)RamConstantPointer();
#endif
#ifdef VALID_FLASH_CONSTANT_ONLY
    __asm__ volatile("movw r3,#0\nmovt r3,#0x0800" ::: "r3");
#endif
#ifdef VALID_SP_FRAME_STORES
    __asm__ volatile("str r0,[sp,#12]\nstrh.w r0,[sp,#14]\n"
                     "strb.w r0,[sp,#15]" ::
                         : "memory");
#endif
#ifdef BAD_CALLER_SP_STORE
    __asm__ volatile("str r0,[sp,#20]" ::: "memory");
#endif
#ifdef BAD_SP_FRAME_END_WORD
    __asm__ volatile("str r0,[sp,#16]" ::: "memory");
#endif
#ifdef BAD_SP_FRAME_END_BYTE
    __asm__ volatile("strb.w r0,[sp,#16]" ::: "memory");
#endif
#ifdef BAD_SP_FRAME_PARTIAL_HALF
    __asm__ volatile("strh.w r0,[sp,#15]" ::: "memory");
#endif
#ifdef BAD_SP_FRAME_PARTIAL_WORD
    __asm__ volatile("str.w r0,[sp,#14]" ::: "memory");
#endif
#ifdef BAD_SP_FRAME_NEGATIVE
    __asm__ volatile("str.w r0,[sp,#-4]" ::: "memory");
#endif
#ifdef BAD_POP_LR_PC
    __asm__ volatile("add sp,#16\npush {lr}\npush {lr}\n"
                     ".inst.w 0xe8bdc000" ::
                         : "memory");
#endif
#ifdef BAD_FP
    __asm__ volatile("vmov.f32 s0,s1");
#endif
#ifdef BAD_BLX
    __asm__ volatile("blx r3" ::: "lr");
#endif
#ifdef BAD_BX
    __asm__ volatile("bx r3");
#endif
#ifdef BAD_BX_RESERVED
    __asm__ volatile(".inst.n 0x4701");
#endif
#ifdef BAD_BLX_RESERVED
    __asm__ volatile(".inst.n 0x4781");
#endif
#ifdef BAD_LR_LOAD_WRITEBACK
    __asm__ volatile("ldr r0,[lr],#4");
#endif
#ifdef BAD_LR_STORE_WRITEBACK
    __asm__ volatile("str r0,[lr],#4");
#endif
#ifdef BAD_LR_PRE_LOAD_WRITEBACK
    __asm__ volatile("ldr r0,[lr,#4]!");
#endif
#ifdef BAD_LR_PRE_STORE_WRITEBACK
    __asm__ volatile("str r0,[lr,#4]!");
#endif
#ifdef BAD_LR_DUAL_LOAD_WRITEBACK
    __asm__ volatile("ldrd r0,r1,[lr],#8");
#endif
#ifdef BAD_LR_DUAL_STORE_WRITEBACK
    __asm__ volatile("strd r0,r1,[lr],#8");
#endif
#ifdef BAD_LR_DUAL_PRE_LOAD_WRITEBACK
    __asm__ volatile("ldrd r0,r1,[lr,#8]!");
#endif
#ifdef BAD_LR_DUAL_PRE_STORE_WRITEBACK
    __asm__ volatile("strd r0,r1,[lr,#8]!");
#endif
#ifdef BAD_PC_WRITE
    __asm__ volatile("mov pc,r3");
#endif
#ifdef BAD_TABLE
    __asm__ volatile("tbb [r0,r1]");
#endif
#ifdef BAD_UNKNOWN
    __asm__ volatile("bkpt #0");
#endif
#ifdef BAD_CPSIE
    __asm__ volatile("cpsie i");
#endif
#ifdef BAD_CONTROL
    __asm__ volatile("msr control,r0");
#endif
#ifdef BAD_MSP
    __asm__ volatile("msr msp,r0");
#endif
#ifdef BAD_PSP
    __asm__ volatile("msr psp,r0");
#endif
#ifdef BAD_PRIMASK
    __asm__ volatile("msr primask,r0");
#endif
#ifdef BAD_BASEPRI
    __asm__ volatile("msr basepri,r0");
#endif
#ifdef BAD_FAULTMASK
    __asm__ volatile("msr faultmask,r0");
#endif
#ifdef FLASH_CALL
    return Flash_Helper(lane[0]) + 1U;
#endif
#ifdef FLASH_BRANCH
    __asm__ volatile("b.w Flash_Helper");
#endif
#ifdef BAD_VENEER
    __asm__ volatile("ldr.w pc,1f\n.balign 4\n1: .word RamLeaf + 1\n");
#endif
#ifdef RECURSION
    __asm__ volatile("bl RamLeaf" ::: "r0", "r1", "r2", "r3", "lr");
#endif
#ifdef BRANCH_DATA
    __asm__ volatile("b 1f\n.balign 4\n1: .word 0x47702000\n");
#endif
    return lane[0] + 1U;
}
#if defined(BAD_CALLEE_STACK_RETURN) || defined(BAD_CALLEE_SPILL_RETURN) ||    \
    defined(BAD_CALLEE_NONRETURN_STORE) ||                                     \
    defined(VALID_CALLEE_CLEARS_STACK_ARGUMENT)
#ifdef BAD_CALLEE_SPILL_RETURN
RAM word_t RamPointer(word_t value) {
    volatile word_t lane[2];
    word_t address;
    lane[0] = value;
    __asm__ volatile("mov %0,sp\nstr %0,[sp]\nldr %0,[sp]"
                     : "=r"(address)
                     :
                     : "memory");
    return address;
}
#else
RAM __attribute__((naked)) word_t RamPointer(word_t value) {
#ifdef BAD_CALLEE_STACK_RETURN
    __asm__ volatile("mov r0,sp\nbx lr");
#elif defined(BAD_CALLEE_NONRETURN_STORE)
    __asm__ volatile("str r1,[r0]\n1: b 1b");
#else
    __asm__ volatile("ldr r0,1f\nbx lr\n.balign 4\n"
                     "1: .word diagnostic\n");
#endif
}
#endif
#endif
#ifdef BAD_SP_REGISTER_LOAD_ARGUMENT
RAM __attribute__((naked)) word_t RamIndexedArgument(word_t value) {
    __asm__ volatile("mov.w r12,#0\nldr.w r0,[sp,r12]\n"
                     "str r1,[r0]\nbx lr");
}
RAM word_t RamPushStackArgument(word_t value) {
    word_t result = RamLeaf(value);
    /* The compiler preserves incoming r4 in its aligned frame. */
    __asm__ volatile("bl RamIndexedArgument" ::
                         : "r0", "r1", "r2", "r3", "r4", "r12", "lr", "memory");
    return result;
}
#endif
RAM word_t RamNested(word_t value) {
    word_t result = RamLeaf(value);
#ifdef BAD_SP_REGISTER_LOAD_ARGUMENT
    __asm__ volatile("mov r4,sp\nbl RamPushStackArgument" ::
                         : "r0", "r1", "r2", "r3", "r4", "r12", "lr", "memory");
#endif
#if defined(BAD_CALLEE_STACK_RETURN) || defined(BAD_CALLEE_SPILL_RETURN)
    __asm__ volatile("bl RamPointer\nstr r1,[r0]" ::
                         : "r0", "r1", "r2", "r3", "r12", "lr", "memory");
#endif
#ifdef BAD_CALLEE_NONRETURN_STORE
    __asm__ volatile("mov r0,sp\nbl RamPointer" ::
                         : "r0", "r1", "r2", "r3", "r12", "lr", "memory");
#endif
#ifdef VALID_CALLEE_CLEARS_STACK_ARGUMENT
    __asm__ volatile("mov r0,sp\nbl RamPointer\nstr r1,[r0]" ::
                         : "r0", "r1", "r2", "r3", "r12", "lr", "memory");
#endif
#ifdef BAD_C_CALL_SP_ALIGNMENT
    __asm__ volatile("add sp,#4\nbl RamLeaf\nsub sp,#4" ::: "memory");
#endif
#ifdef BAD_SAVED_LR
    __asm__ volatile("str r0,[sp,#4]");
#endif
#ifdef BAD_SAVED_LR_WIDE
    __asm__ volatile("str.w r0,[sp,#4]");
#endif
#ifdef BAD_SAVED_LR_DUAL
    __asm__ volatile("strd r0,r1,[sp,#0]");
#endif
#ifdef BAD_SAVED_LR_BYTE
    __asm__ volatile("strb.w r0,[sp,#5]");
#endif
#ifdef BAD_SP_INDEX
    __asm__ volatile("str.w r0,[sp,r1]");
#endif
#ifdef BAD_SP_ALIAS
    __asm__ volatile("mov r2,sp\nstr r0,[r2,#4]" ::: "r2", "memory");
#endif
#ifdef BAD_SP_ADD_ALIAS
    __asm__ volatile("add r2,sp,#4\nstr r0,[r2]" ::: "r2", "memory");
#endif
#ifdef BAD_MSP_ALIAS
    __asm__ volatile("mrs r2,msp\nstr r0,[r2,#4]" ::: "r2", "memory");
#endif
#ifdef BAD_SP_COPY_ALIAS
    __asm__ volatile("mov r2,sp\nmov r3,r2\nstr r0,[r3,#4]" ::
                         : "r2", "r3", "memory");
#endif
#ifdef BAD_SP_ARITH_ALIAS
    __asm__ volatile("mov r2,sp\nadds r2,#4\nstr r0,[r2]" ::: "r2", "memory");
#endif
#ifdef BAD_SP_WIDE_ARITH_ALIAS
    __asm__ volatile("mov r2,sp\nadd.w r3,r2,#4\nstr r0,[r3]" ::
                         : "r2", "r3", "memory");
#endif
#ifdef BAD_SP_BYTE_ALIAS
    __asm__ volatile("mov r2,sp\nstrb.w r0,[r2,#5]" ::: "r2", "memory");
#endif
    return result + 3U;
}
RAM word_t RamTail(word_t value) {
    return RamNested(value);
}
RAM word_t nx_stm32h743_flash_ram_transaction(void* workspace) {
    return RamTail((word_t)workspace) + 1U;
}
RAM __attribute__((noreturn)) void RamTerminal(word_t number) {
    diagnostic = number;
    for (;;) {
        __asm__ volatile("nop");
    }
}
RAM void nx_stm32h743_flash_ram_nmi(void) {
#ifdef RETURNING_NMI
    if (diagnostic == 0U)
        return;
#endif
    RamTerminal(2U);
}
RAM void nx_stm32h743_flash_ram_hardfault(void) {
    RamTerminal(3U);
}
#ifdef ORPHAN_FP
RAM void UnusedRamEntry(void) {
    __asm__ volatile("vmov.f32 s0,s1");
}
#endif
__asm__(".section .outside,\"a\",%progbits\n"
        ".balign 4\n.global outside_literal\noutside_literal: .word 0\n");

/* Only the CPU/MMIO boundary symbol; this fixture never executes a device. */
RAM __attribute__((noreturn)) void
nx_stm32h743_flash_ram_terminal(void* workspace, word_t reason) {
    (void)workspace;
    RamTerminal(reason);
}
