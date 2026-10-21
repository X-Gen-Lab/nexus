/**
 * \file            u575-stop2.c
 * \brief           Public retained-clock and stopped-Platform Stop2 link
 * fixture \author          Nexus Team \version         1.0.0 \date 2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/stm32u575_stop2.h"
#include "nexus_bindings.h"
static nx_stm32u575_retained_time_storage_t s_clock;
/** \brief Boot supplies the reviewed retained source; this is a link fixture.
 */
int main(void) {
    nx_clock_t clock;
    nx_stm32u575_retained_time_config_t config = {
        NX_U575_RETAINED_TIME_RTC, 60U, nx_stm32u575_stop2_identity()};
    if (nx_stm32u575_retained_time_initialize(&s_clock, &config, &clock) !=
            NX_SUCCESS ||
        nx_stm32u575_stop2_initialize(1U, &s_clock) != NX_SUCCESS)
        return 1;
    nx_clock_domain_t* domain = nx_stm32u575_stop2_domain();
    nx_clock_lease_t platform_owner = {0};
    if (nx_clock_domain_acquire(domain, &platform_owner) != NX_SUCCESS)
        return 2;
    nx_platform_start_result_t started = nx_platform_start();
    nx_clock_barrier_t owners;
    if (nx_clock_domain_close(domain, &owners) != NX_SUCCESS ||
        started.primary != NX_SUCCESS || nx_platform_stop() != NX_SUCCESS)
        for (;;) {
        }
    if (nx_clock_domain_release(domain, &platform_owner) != NX_SUCCESS)
        return 3;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stm32u575_stop2_result_t result =
        nx_stm32u575_stop2_enter(owners, 2048U, 10000U);
    nx_result_t primary = result.primary;
    while (result.remaining_effects != 0U)
        result = nx_stm32u575_stop2_recover(owners, 10000U);
    nx_arch_irq_restore(saved);
    if (primary != NX_SUCCESS || result.cleanup != NX_SUCCESS)
        return 4;
    /* A fresh System start creates a new TIM2 domain. The retained clock
     * snapshots are separate and never reinterpret an old Core deadline. */
    if (nx_clock_domain_reopen(owners) != NX_SUCCESS)
        return 5;
    if (nx_platform_start().primary != NX_SUCCESS)
        return 6;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 7;
}
