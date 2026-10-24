/**
 * \file            flash_ram_transaction.c
 * \brief           H743 RAM vector publication and guarded return boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_flash_ram.h"

#define NX_H743_RAM  __attribute__((section(".nx_flash_ram.text"), noinline))
#define NX_H743_BUSY ((uint32_t)(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE))
#ifndef NX_H743_FLASH_RAM_CURRENT
#define NX_H743_FLASH_RAM_CURRENT()                                            \
    ((nx_stm32h743_flash_ram_workspace_t*)(uintptr_t)SCB->VTOR)
#endif
#ifndef NX_H743_FLASH_RAM_VTOR
#define NX_H743_FLASH_RAM_VTOR(execution) ((uintptr_t)(execution)->vectors)
#endif
#ifndef NX_H743_FLASH_RAM_BARRIER
#define NX_H743_FLASH_RAM_BARRIER()                                            \
    do {                                                                       \
        __DSB();                                                               \
        __ISB();                                                               \
    } while (0)
#endif

static NX_H743_RAM bool
busy(const nx_stm32h743_flash_ram_workspace_t* execution) {
    NX_H743_RAM_BIND(execution);
    return ((NX_H743_READ(&NX_H743_RAM_FLASH(execution)->SR1) |
             NX_H743_READ(&NX_H743_RAM_FLASH(execution)->SR2)) &
            NX_H743_BUSY) != 0U;
}

/* One RAM boundary closes every returning transaction, including refusals
 * before a physical pulse. Keep the captured vector/result in the call ABI
 * so extracting this tail does not reread mutable command state. */
#if defined(NX_H743_MODEL)
NX_H743_RAM nx_result_t
nx_stm32h743_flash_ram_return(nx_stm32h743_flash_ram_workspace_t* execution,
                              uint32_t original, nx_result_t result) {
    NX_H743_RAM_BIND(execution);
    if (busy(execution))
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_DRAIN);
    if (execution->guard.remaining_effects != 0U)
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_EXCEPTION);
    volatile uint32_t* control = execution->bank == 0U
                                     ? &NX_H743_RAM_FLASH(execution)->CR1
                                     : &NX_H743_RAM_FLASH(execution)->CR2;
    /* Guard rejection can precede its pulse/LOCK phase. This RAM owner still
     * closes its idle target before granting a normal CPU return. */
    const uint32_t locked = execution->saved_control | FLASH_CR_LOCK;
    if (NX_H743_READ(control) != locked &&
        !nx_stm32h743_flash_ram_lock(execution))
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_LOCK);
    /* Every path remains on the private RAM stack until protection is back
     * and this original vector value has actually been read back. */
    NX_H743_WRITE(&NX_H743_RAM_CORE(execution)->VTOR, original);
    NX_H743_FLASH_RAM_BARRIER();
    if (NX_H743_READ(&NX_H743_RAM_CORE(execution)->VTOR) != original)
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_EXCEPTION);
    return result;
}

#endif

NX_H743_RAM nx_result_t nx_stm32h743_flash_ram_transaction(
    nx_stm32h743_flash_ram_workspace_t* execution) {
    NX_H743_RAM_BIND(execution);
#if defined(NX_H743_MODEL)
    const uint32_t original = execution->saved_vtor;
#else
    extern const uint32_t g_pfnVectors[];
    if (NX_H743_READ(&SCB->VTOR) != (uint32_t)(uintptr_t)g_pfnVectors)
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_EXCEPTION);
#endif
    const uint32_t private_vector = (uint32_t)NX_H743_FLASH_RAM_VTOR(execution);
    if (busy(execution))
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_DRAIN);
    NX_H743_WRITE(&NX_H743_RAM_CORE(execution)->VTOR, private_vector);
    NX_H743_FLASH_RAM_BARRIER();
    nx_result_t result = NX_ERROR_IO;
    if (NX_H743_READ(&NX_H743_RAM_CORE(execution)->VTOR) == private_vector) {
        NX_H743_RAM_BIND(execution);
        result = nx_stm32h743_flash_ram_context(execution);
        if (result == NX_SUCCESS)
            result = nx_stm32h743_flash_ram_guarded_transaction(
                execution, &execution->guard, execution->deadline);
    }
#if defined(NX_H743_MODEL)
    return nx_stm32h743_flash_ram_return(execution, original, result);
#else
    /* The assembly bridge checks hardware against its protected captures. */
    return result;
#endif
}

static NX_H743_RAM _Noreturn void exception(uint32_t number) {
#if defined(NX_H743_MODEL)
    nx_stm32h743_flash_ram_workspace_t* execution = NX_H743_FLASH_RAM_CURRENT();
#else
    nx_stm32h743_flash_ram_workspace_t* execution =
        &g_nx_stm32h743_flash_ram_workspace;
#endif
    if (execution != NULL) {
        execution->fault_exception = number;
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_EXCEPTION);
    }
    for (;;) {
    }
}

NX_H743_RAM void nx_stm32h743_flash_ram_nmi(void) {
    exception(2U);
}

NX_H743_RAM void nx_stm32h743_flash_ram_hardfault(void) {
    exception(3U);
}
