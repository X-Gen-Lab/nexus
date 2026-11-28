/**
 * \file            main.c
 * \brief           Consume the F1 direct DAC through the typed factory.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_dac_port_t* output = nx_factory_dac_ref(NX_DAC_REF_OUTPUT);
    nx_dac_info_t info;
    nx_dac_state_t state;
    const nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 1000U);
    const bool operation =
        nx_dac_port_info(output, &info) == NX_SUCCESS &&
        info.code_bits == 12U && info.maximum_code == 4095U &&
        nx_dac_port_start(output, 0U, deadline) == NX_SUCCESS &&
        nx_dac_port_write(output, NX_DAC_CHANNEL_1, 512U) == NX_SUCCESS &&
        nx_dac_port_state(output, &state) == NX_SUCCESS &&
        state.output_codes[0] == 512U;
    const nx_result_t parked = nx_dac_port_stop(output, deadline);
    const nx_result_t stopped = nx_platform_stop();
    return operation && parked == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}
