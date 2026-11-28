/**
 * \file            main.c
 * \brief           Exercise one reviewed F1 ADC scan via public factory
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Acquire one complete scan and verify its declared format before stop.
 */
int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }

#if defined(NX_ADC_REF_PAIR)
    const nx_adc_port_t* analog = nx_factory_adc_ref(NX_ADC_REF_PAIR);
    enum { CHANNELS = 2U, SCAN_TICKS = 41U };
#elif defined(NX_ADC_REF_SCAN)
    const nx_adc_port_t* analog = nx_factory_adc_ref(NX_ADC_REF_SCAN);
    enum { CHANNELS = 2U, SCAN_TICKS = 109U };
#elif defined(NX_ADC_REF_INTERNAL)
    const nx_adc_port_t* analog = nx_factory_adc_ref(NX_ADC_REF_INTERNAL);
    enum { CHANNELS = 2U, SCAN_TICKS = 168U };
#else
    const nx_adc_port_t* analog = nx_factory_adc_ref(NX_ADC_REF_ANALOG);
    enum { CHANNELS = 1U, SCAN_TICKS = 68U };
#endif
    nx_adc_info_t info;
    nx_adc_sequence_info_t timing;
    nx_time_us_t deadline;
    uint16_t raw[CHANNELS];
    size_t count;
    bool valid = nx_adc_port_info(analog, &info) == NX_SUCCESS &&
                 info.resolution_bits == 12U && info.reference_mv == 3300U &&
                 info.channel_count == CHANNELS &&
                 nx_adc_port_sequence_info(analog, &timing) == NX_SUCCESS &&
                 timing.channel_count == CHANNELS &&
                 timing.scan_ticks == SCAN_TICKS &&
                 timing.ticks_per_second == 4000000U &&
                 nx_deadline_finite_after(nx_time_now_us(), 1000U, &deadline) ==
                     NX_SUCCESS &&
                 nx_adc_port_sample(analog, raw, CHANNELS, deadline, &count) ==
                     NX_SUCCESS &&
                 count == CHANNELS;
    for (size_t i = 0U; valid && i < CHANNELS; ++i) {
        valid = raw[i] <= 0x0fffU;
    }
    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
