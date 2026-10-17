/**
 * \file            flash_mpu.c
 * \brief           H743 RAM maintenance protection and exact MPU restoration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_flash_ram.h"
#include <stddef.h>

#define NX_H743_MPU_RAM        __attribute__((section(".nx_flash_ram.text"), noinline))
#define NX_H743_MPU_AXI_FIRST  UINT32_C(0x24000000)
#define NX_H743_MPU_AXI_LAST   UINT32_C(0x24080000)
#define NX_H743_MPU_ATTRIBUTES UINT32_C(0x11000039)
#define NX_H743_MPU_CONTROL    UINT32_C(7)
#define NX_H743_MPU_REGION     UINT32_C(15)
#define NX_H743_MPU_BASE_MASK  (~UINT32_C(0x1f))
#define NX_H743_MPU_BUSY                                                       \
    ((uint32_t)(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE))

extern uint8_t __nx_flash_ram_start[];
extern uint8_t __nx_flash_ram_end[];
#ifndef NX_H743_FLASH_RAM_SYMBOL
#define NX_H743_FLASH_RAM_SYMBOL(name) ((uintptr_t)(name))
#endif
#ifndef NX_H743_FLASH_RAM_ADDRESS
#define NX_H743_FLASH_RAM_ADDRESS(object) ((uintptr_t)(object))
#endif
#ifndef NX_H743_FLASH_RAM_CONTROL
#define NX_H743_FLASH_RAM_CONTROL() __get_CONTROL()
#endif
#ifndef NX_H743_FLASH_RAM_IPSR
#define NX_H743_FLASH_RAM_IPSR() __get_IPSR()
#endif
#ifndef NX_H743_FLASH_RAM_PRIMASK
#define NX_H743_FLASH_RAM_PRIMASK() __get_PRIMASK()
#endif
#ifndef NX_H743_FLASH_RAM_MSP
#define NX_H743_FLASH_RAM_MSP() __get_MSP()
#endif
#ifndef NX_H743_FLASH_RAM_BASEPRI
#define NX_H743_FLASH_RAM_BASEPRI() __get_BASEPRI()
#endif
#ifndef NX_H743_FLASH_RAM_FAULTMASK
#define NX_H743_FLASH_RAM_FAULTMASK() __get_FAULTMASK()
#endif
#ifndef NX_H743_FLASH_RAM_FPCCR
#define NX_H743_FLASH_RAM_FPCCR() (FPU->FPCCR)
#endif
#ifndef NX_H743_FLASH_MPU_REGISTERS
#define NX_H743_FLASH_MPU_REGISTERS() MPU
#endif
#ifndef NX_H743_FLASH_MPU_READ
#define NX_H743_FLASH_MPU_READ(reg) (*(reg))
#endif
#ifndef NX_H743_FLASH_MPU_WRITE
#define NX_H743_FLASH_MPU_WRITE(reg, value) (*(reg) = (value))
#endif
#ifndef NX_H743_FLASH_MPU_DSB
#define NX_H743_FLASH_MPU_DSB() __DSB()
#endif
#ifndef NX_H743_FLASH_MPU_ISB
#define NX_H743_FLASH_MPU_ISB() __ISB()
#endif
#ifndef NX_H743_FLASH_MPU_MODEL_TERMINAL
#define NX_H743_FLASH_MPU_MODEL_TERMINAL(execution, guard)                     \
    ((void)(execution), (void)(guard))
#endif
#ifndef NX_H743_FLASH_MPU_PC
static __attribute__((always_inline)) inline uintptr_t executing_pc(void) {
    uintptr_t address;
    __asm volatile("mov %0, pc" : "=r"(address));
    return address;
}
#define NX_H743_FLASH_MPU_PC() executing_pc()
#endif

static NX_H743_MPU_RAM bool span_valid(uintptr_t first, size_t bytes) {
    return bytes != 0U && bytes <= UINTPTR_MAX - first;
}

static NX_H743_MPU_RAM bool overlap(uintptr_t first, size_t bytes,
                                    uintptr_t other, size_t other_bytes) {
    return !span_valid(first, bytes) || !span_valid(other, other_bytes) ||
           (first < other + other_bytes && other < first + bytes);
}

static NX_H743_MPU_RAM bool axi_span(uintptr_t first, size_t bytes) {
    return first >= NX_H743_MPU_AXI_FIRST && first < NX_H743_MPU_AXI_LAST &&
           bytes <= NX_H743_MPU_AXI_LAST - first;
}

static NX_H743_MPU_RAM bool
aliases_object(const nx_stm32h743_flash_mpu_state_t* guard, uintptr_t cpu_guard,
               const void* object, size_t bytes) {
    return overlap((uintptr_t)guard, sizeof(*guard), (uintptr_t)object,
                   bytes) ||
           overlap(cpu_guard, sizeof(*guard), NX_H743_FLASH_RAM_ADDRESS(object),
                   bytes);
}

/** \brief Validate both host/CPU extents before the sidecar's first read. */
static NX_H743_MPU_RAM bool
control_storage(const nx_stm32h743_flash_ram_workspace_t* execution,
                const nx_stm32h743_flash_mpu_state_t* guard,
                const MPU_Type* registers) {
    if (execution == NULL || guard == NULL || registers == NULL ||
        !span_valid((uintptr_t)execution, sizeof(*execution)) ||
        !span_valid((uintptr_t)guard, sizeof(*guard)) ||
        ((uintptr_t)guard % _Alignof(nx_stm32h743_flash_mpu_state_t)) != 0U)
        return false;
    const uintptr_t cpu_execution = NX_H743_FLASH_RAM_ADDRESS(execution);
    const uintptr_t cpu_guard = NX_H743_FLASH_RAM_ADDRESS(guard);
    if (!axi_span(cpu_execution, sizeof(*execution)) ||
        !axi_span(cpu_guard, sizeof(*guard)) ||
        cpu_guard % _Alignof(nx_stm32h743_flash_mpu_state_t) != 0U)
        return false;
    const bool embedded = guard == &execution->guard;
    if (overlap((uintptr_t)guard, sizeof(*guard), (uintptr_t)execution,
                sizeof(*execution)) &&
        !embedded)
        return false;
    if (overlap(cpu_guard, sizeof(*guard), cpu_execution, sizeof(*execution)) &&
        (!embedded ||
         cpu_guard != cpu_execution +
                          offsetof(nx_stm32h743_flash_ram_workspace_t, guard)))
        return false;
    if (execution->port == NULL || execution->system == NULL ||
        execution->port->registers == NULL ||
        execution->system->timer == NULL || execution->system->core == NULL)
        return false;
    /* All are already-admitted private bindings. The sidecar may not overwrite
     * their backing objects even when a host address map is non-identity. */
    if (aliases_object(guard, cpu_guard, execution->port,
                       sizeof(*execution->port)) ||
        aliases_object(guard, cpu_guard, execution->system,
                       sizeof(*execution->system)) ||
        aliases_object(guard, cpu_guard, execution->port->registers,
                       sizeof(*execution->port->registers)) ||
        aliases_object(guard, cpu_guard, execution->system->timer,
                       sizeof(*execution->system->timer)) ||
        aliases_object(guard, cpu_guard, execution->system->core,
                       sizeof(*execution->system->core)) ||
        aliases_object(guard, cpu_guard, registers, sizeof(*registers)))
        return false;
    const uintptr_t first = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_start);
    const uintptr_t last = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_end);
    return first < last && axi_span(first, last - first) &&
           !overlap(cpu_guard, sizeof(*guard), first, last - first);
}

static NX_H743_MPU_RAM bool
both_idle(const nx_stm32h743_flash_ram_workspace_t* execution) {
    return ((NX_H743_FLASH_MPU_READ(&execution->port->registers->SR1) |
             NX_H743_FLASH_MPU_READ(&execution->port->registers->SR2)) &
            NX_H743_MPU_BUSY) == 0U;
}

static NX_H743_MPU_RAM _Noreturn void
retain(nx_stm32h743_flash_ram_workspace_t* execution,
       nx_stm32h743_flash_mpu_state_t* guard,
       nx_stm32h743_flash_mpu_fault_t reason) {
    guard->fault = reason;
    execution->active = true;
    execution->port->active = true;
    NX_H743_FLASH_MPU_DSB();
    NX_H743_FLASH_MPU_ISB();
    NX_H743_FLASH_MPU_MODEL_TERMINAL(execution, guard);
    for (;;) {
    }
}

/** \brief RNR is itself an owned effect, including a refused write. */
static NX_H743_MPU_RAM bool select_region(MPU_Type* registers,
                                          nx_stm32h743_flash_mpu_state_t* guard,
                                          uint32_t region) {
    guard->remaining_effects |= NX_H743_FLASH_MPU_EFFECT_SELECTOR;
    NX_H743_FLASH_MPU_WRITE(&registers->RNR, region);
    return NX_H743_FLASH_MPU_READ(&registers->RNR) == region;
}

static NX_H743_MPU_RAM void
restore_selector(nx_stm32h743_flash_ram_workspace_t* execution,
                 nx_stm32h743_flash_mpu_state_t* guard, MPU_Type* registers) {
    if ((guard->remaining_effects & NX_H743_FLASH_MPU_EFFECT_SELECTOR) == 0U)
        return;
    NX_H743_FLASH_MPU_WRITE(&registers->RNR, guard->saved_selector);
    if (NX_H743_FLASH_MPU_READ(&registers->RNR) != guard->saved_selector)
        retain(execution, guard, NX_H743_FLASH_MPU_FAULT_RESTORE);
    guard->remaining_effects &= ~(uint32_t)NX_H743_FLASH_MPU_EFFECT_SELECTOR;
}

/** \brief No CPU policy withdrawal precedes both-bank idle and target LOCK. */
static NX_H743_MPU_RAM void
restore_policy(nx_stm32h743_flash_ram_workspace_t* execution,
               nx_stm32h743_flash_mpu_state_t* guard, MPU_Type* registers) {
    if (!both_idle(execution))
        retain(execution, guard, NX_H743_FLASH_MPU_FAULT_RESTORE);
    const volatile uint32_t* control = execution->bank == 0U
                                           ? &execution->port->registers->CR1
                                           : &execution->port->registers->CR2;
    if (NX_H743_FLASH_MPU_READ(control) !=
        (execution->saved_control | FLASH_CR_LOCK))
        retain(execution, guard, NX_H743_FLASH_MPU_FAULT_LOCK);
    NX_H743_FLASH_MPU_DSB();
    if ((guard->remaining_effects & NX_H743_FLASH_MPU_EFFECT_CONTROL) != 0U) {
        NX_H743_FLASH_MPU_WRITE(&registers->CTRL, guard->saved_control);
        if (NX_H743_FLASH_MPU_READ(&registers->CTRL) != guard->saved_control)
            retain(execution, guard, NX_H743_FLASH_MPU_FAULT_RESTORE);
        guard->remaining_effects &= ~(uint32_t)NX_H743_FLASH_MPU_EFFECT_CONTROL;
    }
    if ((guard->remaining_effects & NX_H743_FLASH_MPU_EFFECT_REGION) != 0U) {
        if (!select_region(registers, guard, guard->region))
            retain(execution, guard, NX_H743_FLASH_MPU_FAULT_RESTORE);
        NX_H743_FLASH_MPU_WRITE(&registers->RASR, guard->saved_attributes);
        if (NX_H743_FLASH_MPU_READ(&registers->RASR) != guard->saved_attributes)
            retain(execution, guard, NX_H743_FLASH_MPU_FAULT_RESTORE);
        NX_H743_FLASH_MPU_WRITE(&registers->RBAR,
                                guard->saved_base & NX_H743_MPU_BASE_MASK);
        if ((NX_H743_FLASH_MPU_READ(&registers->RBAR) &
             NX_H743_MPU_BASE_MASK) !=
            (guard->saved_base & NX_H743_MPU_BASE_MASK))
            retain(execution, guard, NX_H743_FLASH_MPU_FAULT_RESTORE);
        guard->remaining_effects &= ~(uint32_t)NX_H743_FLASH_MPU_EFFECT_REGION;
    }
    restore_selector(execution, guard, registers);
    NX_H743_FLASH_MPU_DSB();
    NX_H743_FLASH_MPU_ISB();
}

static NX_H743_MPU_RAM nx_result_t
rollback(nx_stm32h743_flash_ram_workspace_t* execution,
         nx_stm32h743_flash_mpu_state_t* guard, MPU_Type* registers) {
    if (!both_idle(execution))
        retain(execution, guard, NX_H743_FLASH_MPU_FAULT_RESTORE);
    if (!nx_stm32h743_flash_ram_lock(execution))
        retain(execution, guard, NX_H743_FLASH_MPU_FAULT_LOCK);
    restore_policy(execution, guard, registers);
    return NX_ERROR_IO;
}

NX_H743_MPU_RAM nx_result_t nx_stm32h743_flash_ram_guarded_transaction(
    nx_stm32h743_flash_ram_workspace_t* execution,
    nx_stm32h743_flash_mpu_state_t* guard, nx_time_us_t deadline) {
    MPU_Type* registers = NX_H743_FLASH_MPU_REGISTERS();
    if (!control_storage(execution, guard, registers))
        return NX_ERROR_INVALID;
    if (NX_H743_FLASH_RAM_IPSR() != 0U ||
        (NX_H743_FLASH_RAM_CONTROL() & 1U) != 0U ||
        NX_H743_FLASH_RAM_PRIMASK() != 1U ||
        NX_H743_FLASH_RAM_BASEPRI() != 0U ||
        NX_H743_FLASH_RAM_FAULTMASK() != 0U)
        return NX_ERROR_CONTEXT;
    const uintptr_t stack = NX_H743_FLASH_RAM_MSP();
    const uintptr_t first = (uintptr_t)execution->stack;
    const uintptr_t code_first = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_start);
    const uintptr_t code_last = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_end);
    const uintptr_t pc = NX_H743_FLASH_MPU_PC();
    if ((NX_H743_FLASH_RAM_CONTROL() & 2U) != 0U || stack < first ||
        stack - first >= sizeof(execution->stack) || pc < code_first ||
        pc >= code_last ||
        NX_H743_FLASH_MPU_READ(&execution->system->core->VTOR) !=
            NX_H743_FLASH_RAM_ADDRESS(execution->vectors))
        return NX_ERROR_STATE;
    if ((NX_H743_FLASH_MPU_READ(&execution->system->core->CCR) &
         (SCB_CCR_IC_Msk | SCB_CCR_DC_Msk)) != 0U)
        return NX_ERROR_UNSUPPORTED;
    if ((NX_H743_FLASH_RAM_FPCCR() & 1U) != 0U)
        return NX_ERROR_CONTEXT;
    if (!execution->prepared || !execution->active ||
        !execution->port->active || !execution->system->started ||
        execution->bank > 1U || guard->remaining_effects != 0U)
        return NX_ERROR_STATE;
    const nx_result_t context = nx_stm32h743_flash_ram_context(execution);
    if (context != NX_SUCCESS)
        return context;
    if (NX_H743_FLASH_MPU_READ(&registers->TYPE) != (16U << 8U))
        return NX_ERROR_UNSUPPORTED;
    const uint32_t control = NX_H743_FLASH_MPU_READ(&registers->CTRL);
    if (control != 0U && control != 4U)
        return NX_ERROR_STATE;
    if (!both_idle(execution))
        return NX_ERROR_BUSY;

    guard->saved_control = control;
    guard->saved_selector = NX_H743_FLASH_MPU_READ(&registers->RNR);
    guard->region = NX_H743_MPU_REGION;
    guard->fault = NX_H743_FLASH_MPU_FAULT_NONE;
    guard->timed_out = false;
    /* A disabled CTRL does not disable retained descriptors. Never activate
     * another owner's policy, even when only one descriptor is enabled. */
    for (uint32_t region = 0U; region <= NX_H743_MPU_REGION; ++region) {
        if (!select_region(registers, guard, region))
            return rollback(execution, guard, registers);
        if ((NX_H743_FLASH_MPU_READ(&registers->RASR) & 1U) != 0U) {
            restore_selector(execution, guard, registers);
            NX_H743_FLASH_MPU_DSB();
            NX_H743_FLASH_MPU_ISB();
            return NX_ERROR_STATE;
        }
    }
    guard->saved_base = NX_H743_FLASH_MPU_READ(&registers->RBAR);
    guard->saved_attributes = NX_H743_FLASH_MPU_READ(&registers->RASR);
    guard->remaining_effects |= NX_H743_FLASH_MPU_EFFECT_REGION;
    NX_H743_FLASH_MPU_WRITE(&registers->RBAR, 0U);
    if ((NX_H743_FLASH_MPU_READ(&registers->RBAR) & NX_H743_MPU_BASE_MASK) !=
        0U)
        return rollback(execution, guard, registers);
    /* AP001 permits the eight explicit programming stores. Strongly-ordered
     * plus XN suppresses CPU Code-domain speculation and instruction fetch;
     * the independently checked RAM graph excludes explicit Flash reads. */
    NX_H743_FLASH_MPU_WRITE(&registers->RASR, NX_H743_MPU_ATTRIBUTES);
    if (NX_H743_FLASH_MPU_READ(&registers->RASR) != NX_H743_MPU_ATTRIBUTES)
        return rollback(execution, guard, registers);
    guard->remaining_effects |= NX_H743_FLASH_MPU_EFFECT_CONTROL;
    NX_H743_FLASH_MPU_WRITE(&registers->CTRL, NX_H743_MPU_CONTROL);
    if (NX_H743_FLASH_MPU_READ(&registers->CTRL) != NX_H743_MPU_CONTROL)
        return rollback(execution, guard, registers);
    NX_H743_FLASH_MPU_DSB();
    NX_H743_FLASH_MPU_ISB();

    const nx_stm32h743_flash_ram_pulse_outcome_t outcome =
        nx_stm32h743_flash_ram_pulse_until_idle(execution, deadline,
                                                &guard->timed_out);
    if (outcome == NX_H743_FLASH_RAM_PULSE_LOCK_UNPROVEN)
        retain(execution, guard, NX_H743_FLASH_MPU_FAULT_LOCK);
    restore_policy(execution, guard, registers);
    if (outcome != NX_H743_FLASH_RAM_PULSE_SUCCESS)
        return NX_ERROR_IO;
    return guard->timed_out ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}
