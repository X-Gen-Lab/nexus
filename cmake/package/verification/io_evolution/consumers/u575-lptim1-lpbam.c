/**
 * \file            u575-lptim1-lpbam.c
 * \brief           Public C11 same-owner finite LPTIM1/Stop2 link reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/stm32u575_stop2_lptim1.h"
#include "nexus/core/memory.h"
#include "nexus_bindings.h"

#define POLL_LIMIT 10000U
static nx_stm32u575_retained_time_storage_t s_clock;
static nx_stm32u575_lpdma_lptim1_storage_t s_receiver;
static nx_stm32u575_lpdma_lptim1_request_t s_request;
NX_MEMORY_SRAM4_DATA static nx_stm32u575_lptim1_tuple_t s_tuple = {37U, 0U,
                                                                   2047U};
NX_MEMORY_SRAM4_NOINIT static nx_stm32u575_lpdma_lptim1_workspace_t s_workspace;

/** \brief Retain static storage and the closed boundary on failed recovery. */
static _Noreturn void retain(void) {
    (void)nx_arch_irq_save();
    for (;;) {
    }
}
/** \brief Boot supplies the reviewed RTC/LSE and SRAM4/LPDMA run profile.
 * \note This source only exercises public interfaces in a real M33 link.
 *       An external reviewed boot owner must already prepare Range4, the
 *       retained LSE calendar, RTC/PWR/DBP access and the reset-only SRAM4
 *       startup prerequisites. It supplies no private MMIO or fabricated boot
 *       preparation. No hardware execution, GPIO route, PWM playback or
 *       physical Stop2/timing/power result is established by this fixture.
 */
int main(void) {
    nx_clock_t clock;
    nx_stm32u575_retained_time_config_t retained = {
        NX_U575_RETAINED_TIME_RTC, 60U, nx_stm32u575_stop2_identity()};
    nx_stm32u575_lpdma_lptim1_config_t config = {&s_clock,
                                                 nx_stm32u575_stop2_identity()};
    nx_stm32u575_lpdma_lptim1_t receiver;
    nx_stm32u575_retained_time_snapshot_t snapshot;
    nx_request_initialize(&s_request.base);
    if (nx_stm32u575_retained_time_initialize(&s_clock, &retained, &clock) !=
            NX_SUCCESS ||
        nx_stm32u575_stop2_initialize(1U, &s_clock) != NX_SUCCESS ||
        nx_stm32u575_lpdma_lptim1_initialize(&s_receiver, &config, &receiver) !=
            NX_SUCCESS)
        return 1;
    if (nx_stm32u575_lpdma_lptim1_start(&receiver, POLL_LIMIT) != NX_SUCCESS) {
        while (nx_stm32u575_lpdma_lptim1_stop(&receiver, POLL_LIMIT) !=
               NX_SUCCESS) {
        }
        return 2;
    }
    if (nx_stm32u575_retained_time_sample(&s_clock, &snapshot) != NX_SUCCESS ||
        nx_stm32u575_lpdma_lptim1_request_prepare(
            &s_request, &snapshot, &s_tuple, &s_workspace, NX_DEADLINE_NEVER) !=
            NX_SUCCESS)
        retain();
    nx_clock_domain_t* domain = nx_stm32u575_stop2_domain();
    nx_clock_lease_t platform = {0};
    if (nx_clock_domain_acquire(domain, &platform) != NX_SUCCESS)
        retain();
    nx_platform_start_result_t started = nx_platform_start();
    nx_clock_barrier_t owners;
    if (nx_clock_domain_close(domain, &owners) != NX_SUCCESS ||
        started.primary != NX_SUCCESS || nx_platform_stop() != NX_SUCCESS)
        retain();
    /* The empty reference has only the enrolled Platform producer. Product
     * code enrolls and joins every additional RUN/IRQ/DMA/deadline user. */
    if (nx_clock_domain_release(domain, &platform) != NX_SUCCESS)
        retain();
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stm32u575_stop2_lptim1_result_t entered =
        nx_stm32u575_stop2_lptim1_enter(owners, &receiver, &s_request, 4096U,
                                        POLL_LIMIT);
    nx_stm32u575_stop2_result_t drained = entered.transition;
    while (drained.remaining_effects != 0U)
        drained = nx_stm32u575_stop2_recover(owners, POLL_LIMIT);
    nx_result_t result = NX_ERROR_STATE;
    size_t transferred = 0U;
    nx_result_t acquired =
        nx_request_result(&s_request.base, &result, &transferred);
    /* Settlement joins the loan. Public stop still owns the access gates,
     * kernel mux and added LSESYS source until this separate observed join. */
    while (nx_stm32u575_lpdma_lptim1_stop(&receiver, POLL_LIMIT) !=
           NX_SUCCESS) {
    }
    nx_arch_irq_restore(saved);
    if (!entered.accepted || entered.transition.primary != NX_SUCCESS ||
        drained.cleanup != NX_SUCCESS || acquired != NX_SUCCESS ||
        result != NX_SUCCESS || transferred != sizeof(s_tuple))
        return 3;
    if (nx_clock_domain_reopen(owners) != NX_SUCCESS)
        return 4;
    if (nx_platform_start().primary != NX_SUCCESS)
        return 5;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 6;
}
