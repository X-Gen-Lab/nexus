/**
 * \file            f407-handoff.c
 * \brief           Public-only exact F407 masked image-transfer link scenario
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/stm32f407_handoff.h"
#include "nexus_bindings.h"

/** \brief Link one real public lifecycle; never infer physical execution. */
int main(void) {
    if (nx_stm32f407_handoff_initialize(1U) != NX_SUCCESS) {
        return 1;
    }
    nx_clock_domain_t* domain = nx_stm32f407_handoff_domain();
    nx_clock_lease_t platform_owner = {0};
    if (nx_clock_domain_acquire(domain, &platform_owner) != NX_SUCCESS) {
        return 2;
    }
    nx_platform_start_result_t started = nx_platform_start();
    nx_clock_barrier_t closed;
    if (nx_clock_domain_close(domain, &closed) != NX_SUCCESS) {
        return 3;
    }
    if (started.primary != NX_SUCCESS || nx_platform_stop() != NX_SUCCESS) {
        for (;;) {
        }
    }
    if (nx_clock_domain_release(domain, &platform_owner) != NX_SUCCESS) {
        return 3;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    /* This link fixture names physical Flash; an actual product owns image
     * selection/authentication and a target entry that accepts PRIMASK=1. */
    nx_result_t rejected = nx_stm32f407_handoff(closed, 0x08000000U);
    nx_arch_irq_restore(saved);
    return rejected == NX_SUCCESS ? 4 : 0;
}
