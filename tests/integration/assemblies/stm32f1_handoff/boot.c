/**
 * \file            boot.c
 * \brief           Launch the exact linked F103RB offset app on a closed epoch.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/soc/stm32f103rb_handoff.h"
#include "nexus_bindings.h"
#include "target_contract.h"

static nx_stm32f103rb_handoff_t owner;

int main(void) {
    if (nx_clock_domain_construct(&owner.domain, 1U) != NX_SUCCESS)
        return 1;
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 2;
    /* The reference has no external application publishers to enroll. */
    nx_clock_barrier_t barrier;
    if (nx_clock_domain_close(&owner.domain, &barrier) != NX_SUCCESS ||
        nx_stm32f103rb_handoff_prepare(&owner, barrier) != NX_SUCCESS)
        return 3;
    const nx_stm32f103rb_executable_t executable = {NX_TARGET_EXEC_BEGIN,
                                                    NX_TARGET_EXEC_END};
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    (void)nx_stm32f103rb_handoff_enter(&owner, barrier, executable);
    nx_arch_irq_restore(saved);
    return 4;
}
