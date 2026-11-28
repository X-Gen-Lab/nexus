/**
 * \file            main.c
 * \brief           Retain and exercise the public F103 quadrature operations.
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
    const nx_encoder_t* const encoder =
        nx_factory_encoder_ref(NX_ENCODER_REF_POSITION);
    nx_encoder_sample_t sample;
    nx_encoder_state_t state;
    const bool valid = nx_encoder_start(encoder) == NX_SUCCESS &&
                       nx_encoder_read(encoder, &sample) == NX_SUCCESS &&
                       sample.count <= UINT16_MAX &&
                       nx_encoder_state(encoder, &state) == NX_SUCCESS &&
                       state.mode == NX_ENCODER_MODE_X4 &&
                       state.counter_bits == 16U && state.running;
    const nx_result_t stopped = nx_encoder_stop(encoder);
    const nx_result_t closed = nx_platform_stop();
    return valid && stopped == NX_SUCCESS && closed == NX_SUCCESS ? 0 : 2;
}
