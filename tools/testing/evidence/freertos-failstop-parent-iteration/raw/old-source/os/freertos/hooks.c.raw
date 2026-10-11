/**
 * \file            hooks.c
 * \brief           Explicit weak fail-stop for kernel invariant violations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"
#include "nexus/os/freertos.h"

/* The test port disables stack checking; retain the exported hook prototype. */
void vApplicationStackOverflowHook(TaskHandle_t task, char* name);

/** \brief A returning observer cannot authorize corrupted-kernel progress. */
static _Noreturn void park(nx_fault_domain_t domain, const char* file,
                           unsigned line) {
    nx_fault_observe(domain, NX_ERROR_STATE, file, line);
    /* Mainline ports write BASEPRI directly. Never weaken a stronger incoming
     * mask; Baseline ports have BASEPRI=0 and take their normal PRIMASK path.
     * This is a read-only Arch leaf, not a Native IRQ lock acquisition. */
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask == 0U && masks.faultmask == 0U &&
        (masks.basepri == 0U ||
         masks.basepri > configMAX_SYSCALL_INTERRUPT_PRIORITY)) {
        taskDISABLE_INTERRUPTS();
    }
    for (;;) {
    }
}

/** \brief Preserve responsibility on stack/invariant corruption. */
__attribute__((weak, noreturn)) void nx_freertos_assert_failed(const char* file,
                                                               unsigned line) {
    park(NX_FAULT_FREERTOS_ASSERT, file, line);
}

/** \brief Observe stack failure without inspecting potentially corrupt names.
 */
__attribute__((weak)) void vApplicationStackOverflowHook(TaskHandle_t task,
                                                         char* name) {
    (void)task;
    (void)name;
    park(NX_FAULT_FREERTOS_STACK, __FILE__, __LINE__);
}
