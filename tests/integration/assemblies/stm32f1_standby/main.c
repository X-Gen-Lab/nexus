/**
 * \file            main.c
 * \brief           Public F103RB Standby consumer and reset-boot evidence.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/clock_domain_owner.h"
#include "nexus/soc/stm32f103_standby.h"
#include "nexus_bindings.h"

static nx_clock_domain_t s_nx_domain;
static volatile bool s_nx_standby_requested;
static volatile nx_stm32f103_standby_boot_t s_nx_boot_evidence;

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    nx_stm32f103_standby_boot_t boot;
    if (nx_stm32f103_standby_boot(&boot) != NX_SUCCESS) {
        return 2;
    }
    s_nx_boot_evidence = boot;
    if (!s_nx_standby_requested) {
        return (int)nx_platform_stop();
    }

    nx_clock_barrier_t barrier;
    if (nx_clock_domain_construct(&s_nx_domain, 1U) != NX_SUCCESS ||
        nx_clock_domain_close(&s_nx_domain, &barrier) != NX_SUCCESS) {
        return 3;
    }
    const nx_stm32f1_standby_receipt_t attempt =
        nx_stm32f103_standby_enter(barrier, false);
    return (int)(attempt.cleanup != NX_SUCCESS ? attempt.cleanup
                                               : attempt.status);
}
