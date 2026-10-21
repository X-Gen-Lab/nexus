/**
 * \file            u575-retained-time.c
 * \brief           Public independent retained-clock link fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/stm32u575_retained_time.h"
#include "nexus_bindings.h"
static nx_stm32u575_retained_time_storage_t s_clock;
/** \brief External boot prepares the exact retained source before entry. */
int main(void) {
    nx_clock_t clock;
    const nx_stm32u575_retained_time_config_t config = {
        NX_U575_RETAINED_TIME_RTC, 60U, NULL};
    nx_stm32u575_retained_time_snapshot_t sample;
    if (nx_stm32u575_retained_time_initialize(&s_clock, &config, &clock) !=
        NX_SUCCESS)
        return 1;
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS) {
        if (nx_platform_stop() != NX_SUCCESS)
            for (;;) {
            }
        (void)nx_stm32u575_retained_time_stop(&s_clock);
        return 2;
    }
    nx_time_us_t now = clock.read(clock.context);
    if (nx_platform_stop() != NX_SUCCESS)
        for (;;) {
        }
    if (nx_stm32u575_retained_time_sample(&s_clock, &sample) != NX_SUCCESS ||
        sample.identity != &s_clock || sample.now < now)
        return 2;
    return nx_stm32u575_retained_time_stop(&s_clock) == NX_SUCCESS ? 0 : 3;
}
