/**
 * \file            u575-lpdma-copy.c
 * \brief           Public exact SRAM4 copy and same-owner Stop2 link fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/stm32u575_stop2_copy.h"
#include "nexus/core/memory.h"
#include "nexus_bindings.h"
#include <string.h>

static nx_stm32u575_retained_time_storage_t s_clock;
static const nx_clock_t s_clock_face = {nx_stm32u575_retained_time_now,
                                        &s_clock};
/** \brief A separate public caller actually consumes the injected clock. */
nx_time_us_t nx_installed_clock_read(const nx_clock_t* clock);

static nx_stm32u575_lpdma_copy_storage_t s_copy;
static const nx_stm32u575_lpdma_copy_t s_component_face = {
    &nx_stm32u575_lpdma_copy_ops, &s_copy};
static nx_stm32u575_lpdma_copy_request_t s_request;
NX_MEMORY_SRAM4_DATA static unsigned char s_source[17] = {
    1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U};
NX_MEMORY_SRAM4_BSS static unsigned char s_destination[17];
NX_MEMORY_SRAM4_NOINIT static nx_stm32u575_lpdma_copy_workspace_t s_workspace;

/** \brief Boot supplies the reviewed LSE/RTC and selected SRAM4 access profile.
 * \note This source compiles and links the maintained public path. Software
 *       qualification does not execute it or establish a physical copy result.
 */
int main(void) {
    nx_clock_t clock;
    nx_stm32u575_retained_time_config_t retained = {
        NX_U575_RETAINED_TIME_RTC, 60U, nx_stm32u575_stop2_identity()};
    nx_stm32u575_lpdma_copy_config_t config = {&s_clock,
                                               nx_stm32u575_stop2_identity()};
    nx_stm32u575_lpdma_copy_t copy;
    nx_stm32u575_retained_time_snapshot_t snapshot;
    nx_request_initialize(&s_request.base);
    if (nx_stm32u575_retained_time_initialize_alarm_a_copy(
            &s_clock, &retained, &clock) != NX_SUCCESS ||
        nx_stm32u575_stop2_initialize(1U, &s_clock) != NX_SUCCESS ||
        nx_stm32u575_lpdma_copy_initialize(&s_copy, &config, &copy) !=
            NX_SUCCESS ||
        nx_stm32u575_retained_time_sample(&s_clock, &snapshot) != NX_SUCCESS ||
        nx_stm32u575_lpdma_copy_request_prepare(
            &s_request, &snapshot, s_source, s_destination, sizeof(s_source),
            &s_workspace, NX_DEADLINE_NEVER) != NX_SUCCESS)
        return 1;
    if (clock.read != s_clock_face.read ||
        clock.context != s_clock_face.context)
        return 1;
    nx_time_us_t now = clock.read(clock.context);
    if (nx_installed_clock_read(&s_clock_face) < now)
        return 1;
    if (copy.ops != s_component_face.ops ||
        copy.context != s_component_face.context)
        return 1;
    nx_clock_domain_t* domain = nx_stm32u575_stop2_domain();
    nx_clock_lease_t platform = {0};
    if (nx_clock_domain_acquire(domain, &platform) != NX_SUCCESS)
        return 2;
    nx_platform_start_result_t started = nx_platform_start();
    nx_clock_barrier_t owners;
    if (nx_clock_domain_close(domain, &owners) != NX_SUCCESS ||
        started.primary != NX_SUCCESS || nx_platform_stop() != NX_SUCCESS)
        for (;;) {
        }
    if (nx_clock_domain_release(domain, &platform) != NX_SUCCESS)
        return 3;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stm32u575_stop2_copy_result_t entered = nx_stm32u575_stop2_copy_enter(
        owners, &s_component_face, &s_request, 4096U, 10000U);
    nx_stm32u575_stop2_result_t drained = entered.transition;
    while (drained.remaining_effects != 0U)
        drained = nx_stm32u575_stop2_recover(owners, 10000U);
    nx_result_t result = NX_ERROR_STATE;
    size_t transferred = 0U;
    nx_result_t acquired =
        nx_request_result(&s_request.base, &result, &transferred);
    nx_result_t stopped =
        nx_stm32u575_lpdma_copy_stop(&s_component_face, 10000U);
    nx_arch_irq_restore(saved);
    if (!entered.accepted || entered.transition.primary != NX_SUCCESS ||
        drained.cleanup != NX_SUCCESS || stopped != NX_SUCCESS ||
        acquired != NX_SUCCESS || result != NX_SUCCESS ||
        transferred != sizeof(s_source) ||
        memcmp(s_source, s_destination, sizeof(s_source)) != 0)
        return 4;
    if (nx_clock_domain_reopen(owners) != NX_SUCCESS)
        return 5;
    if (nx_platform_start().primary != NX_SUCCESS)
        return 6;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 7;
}
