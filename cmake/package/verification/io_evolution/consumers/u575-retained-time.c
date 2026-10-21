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
static const nx_clock_t s_clock_face = {nx_stm32u575_retained_time_now,
                                        &s_clock};
/** \brief A separate public caller actually consumes the injected clock. */
nx_time_us_t nx_installed_clock_read(const nx_clock_t* clock);

/** \brief External boot prepares the exact retained source before entry. */
int main(void) {
    nx_clock_t clock;
    const nx_stm32u575_retained_time_config_t config = {
        NX_U575_RETAINED_TIME_RTC, 60U, NULL};
    nx_stm32u575_retained_time_snapshot_t sample;
    if (nx_stm32u575_retained_time_initialize(&s_clock, &config, &clock) !=
        NX_SUCCESS)
        return 1;
    if (clock.read != s_clock_face.read ||
        clock.context != s_clock_face.context)
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
    nx_time_us_t face_now = nx_installed_clock_read(&s_clock_face);
    if (nx_platform_stop() != NX_SUCCESS)
        for (;;) {
        }
    if (nx_stm32u575_retained_time_sample(&s_clock, &sample) != NX_SUCCESS ||
        sample.identity != &s_clock || sample.now < now || face_now < now)
        return 2;
    return nx_stm32u575_retained_time_stop(&s_clock) == NX_SUCCESS ? 0 : 3;
}
