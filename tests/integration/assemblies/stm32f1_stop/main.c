/**
 * \file            main.c
 * \brief           Exact F103RB STOP through public Platform and ClockDomain.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/clock_domain_owner.h"
#include "nexus/soc/stm32f103_stop.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_clock_domain_t wake_domain;
static volatile bool stop_requested;

/** \brief Single-thread consumer holds no other time or EXTI borrows. */
static bool try_stop(const nx_exti_port_t* button) {
    nx_clock_barrier_t barrier;
    if (button == NULL ||
        nx_clock_domain_construct(&wake_domain, 1U) != NX_SUCCESS ||
        nx_clock_domain_close(&wake_domain, &barrier) != NX_SUCCESS)
        return false;
    const nx_stm32f1_stop_receipt_t stopped =
        nx_stm32f103_stop_enter(barrier, button);
    if (!stopped.resume_ready || nx_clock_domain_reopen(barrier) != NX_SUCCESS)
        return false;
    const nx_platform_start_result_t resumed = nx_platform_resume();
    return resumed.primary == NX_SUCCESS && resumed.cleanup == NX_SUCCESS &&
           stopped.status == NX_SUCCESS && stopped.wait_issued &&
           stopped.wake_pending &&
           stopped.current_epoch == stopped.previous_epoch + 1U;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_exti_port_t* wake = nx_factory_exti_ref(NX_EXTI_REF_WAKE);
    const bool observed = !stop_requested || try_stop(wake);
    return observed && nx_platform_stop() == NX_SUCCESS ? 0 : 2;
}
