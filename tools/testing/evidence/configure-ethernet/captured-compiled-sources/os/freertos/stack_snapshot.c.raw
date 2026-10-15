/**
 * \file            stack_snapshot.c
 * \brief           Optional bounded static-task stack paint observations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "nexus/arch/arch.h"
#include "nexus/os/freertos_stack.h"

/** \brief Reject wrapped control spans without unrelated pointer ordering. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief Verify a live exact static binding before scanning caller capacity.
 */
nx_result_t
nx_freertos_stack_observe(const nx_freertos_stack_binding_t* binding,
                          nx_stack_observation_t* out) {
    if (binding == NULL || out == NULL || binding->handle == NULL ||
        binding->control == NULL || binding->stack == NULL ||
        binding->words == 0U ||
        binding->words > SIZE_MAX / sizeof(*binding->stack) ||
        binding->words > (UINTPTR_MAX - (uintptr_t)binding->stack) /
                             sizeof(*binding->stack) ||
        (uintptr_t)binding->stack % _Alignof(StackType_t) != 0U ||
        (uintptr_t)binding->control % _Alignof(StaticTask_t) != 0U ||
        overlaps(binding, sizeof(*binding), out, sizeof(*out)) ||
        overlaps(binding->control, sizeof(*binding->control), out,
                 sizeof(*out)) ||
        overlaps(binding->stack, binding->words * sizeof(*binding->stack), out,
                 sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask != 0U || masks.basepri != 0U || masks.faultmask != 0U ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    StackType_t* stack = NULL;
    StaticTask_t* control = NULL;
    if (xTaskGetStaticBuffers(binding->handle, &stack, &control) != pdTRUE ||
        stack != binding->stack || control != binding->control) {
        return NX_ERROR_STATE;
    }
    /* FreeRTOS-Kernel 11.2 tasks.c paints newly created static stack bytes
     * 0xA5. A bounded local scan avoids its unbounded free-space helper. The
     * caller supplies the original capacity; public kernel identity has no size
     * query. */
    return nx_stack_observe(stack, binding->words * sizeof(*stack),
                            UINT8_C(0xA5), out);
}
