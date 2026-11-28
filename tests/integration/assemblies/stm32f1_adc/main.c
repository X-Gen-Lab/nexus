/**
 * \file            main.c
 * \brief           Exercise one F103RB regular ADC sample via public factory
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Acquire one raw result and verify its declared format before stop. */
int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }

    const nx_adc_port_t* analog = nx_factory_adc_ref(NX_ADC_REF_ANALOG);
    nx_adc_info_t info;
    nx_adc_sequence_info_t timing;
    nx_time_us_t deadline;
    uint16_t raw;
    size_t count;
    const bool valid =
        nx_adc_port_info(analog, &info) == NX_SUCCESS &&
        info.resolution_bits == 12U && info.reference_mv == 3300U &&
        info.channel_count == 1U &&
        nx_adc_port_sequence_info(analog, &timing) == NX_SUCCESS &&
        timing.channel_count == 1U && timing.scan_ticks == 68U &&
        timing.ticks_per_second == 4000000U &&
        nx_deadline_finite_after(nx_time_now_us(), 1000U, &deadline) ==
            NX_SUCCESS &&
        nx_adc_port_sample(analog, &raw, 1U, deadline, &count) == NX_SUCCESS &&
        count == 1U && raw <= 0x0fffU;
    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
