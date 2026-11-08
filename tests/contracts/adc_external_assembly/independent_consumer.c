/**
 * \file            independent_consumer.c
 * \brief           Three typed ADC factories linked through the public C API
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Linkage fixture, without a physical execution claim. */
int main(void) {
    const nx_adc_ref_t references[] = {NX_ADC_REF_A, NX_ADC_REF_B,
                                       NX_ADC_REF_C};
    const uint8_t resolutions[] = {12U, 10U, 6U};
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 1;
    }
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 10000U);
    int result = 0;
    for (size_t index = 0U; index < 3U; ++index) {
        const nx_adc_port_t* port = nx_factory_adc_ref(references[index]);
        nx_adc_info_t info;
        uint16_t sample;
        size_t count;
        if (nx_adc_port_info(port, &info) != NX_SUCCESS ||
            info.resolution_bits != resolutions[index] ||
            info.channel_count != 1U ||
            nx_adc_port_sample(port, &sample, 1U, deadline, &count) !=
                NX_SUCCESS ||
            count != 1U) {
            result = 2;
            break;
        }
    }
    return nx_platform_stop() == NX_SUCCESS ? result : 3;
}
