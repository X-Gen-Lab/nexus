/**
 * \file            main.c
 * \brief           Observe F103 EXTI13 and exercise one shallow wait.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/sleep.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Park once while an empty ring and the wake IRQs share PRIMASK. */
static bool wait_for_event(const nx_exti_port_t* button) {
    nx_exti_event_t event;
    size_t count = 0U;
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    const nx_result_t read = nx_exti_port_read(button, &event, 1U, &count);
    const bool ready = read == NX_SUCCESS && count == 1U;
    const bool waited = read == NX_ERROR_EMPTY && count == 0U &&
                        nx_arch_wait_for_interrupt() == NX_ARCH_OK;
    nx_arch_irq_restore(saved);
    if (!waited) {
        return ready;
    }
    const nx_result_t after = nx_exti_port_read(button, &event, 1U, &count);
    /* TIM2 can wake the CPU with the button ring still empty. */
    return (after == NX_SUCCESS && count == 1U) ||
           (after == NX_ERROR_EMPTY && count == 0U);
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_exti_port_t* button = nx_factory_exti_ref(NX_EXTI_REF_BUTTON);
    bool high = false;
    const nx_result_t level = nx_exti_port_level(button, &high);
    const bool observed = wait_for_event(button);
    const nx_result_t stopped = nx_platform_stop();
    return level == NX_SUCCESS && observed && stopped == NX_SUCCESS ? 0 : 2;
}
