/**
 * \file            consumer.c
 * \brief           Link every selected public DAC method through its factory
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Linkage fixture only; physical execution is not qualified. */
int main(void) {
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS || result.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_dac_port_t* port = nx_factory_dac_ref(NX_DAC_REF_OUTPUT);
    nx_dac_info_t info;
    nx_dac_state_t state;
    nx_result_t status = nx_dac_port_info(port, &info);
    if (status == NX_SUCCESS) {
        status = nx_dac_port_write(port, 1U, 123U);
    }
    if (status == NX_SUCCESS) {
        status = nx_dac_port_write_pair(port, 123U, 231U);
    }
    if (status == NX_SUCCESS) {
        status = nx_dac_port_start(port, 0U, NX_DEADLINE_NEVER);
    }
    if (status == NX_SUCCESS) {
        status = nx_dac_port_state(port, &state);
    }
    if (status == NX_SUCCESS && info.software_trigger) {
        status = nx_dac_port_trigger(port, info.channel_mask);
    }
    nx_result_t stop = nx_dac_port_stop(port, NX_DEADLINE_NEVER);
    nx_result_t close = nx_platform_stop();
    return status == NX_SUCCESS && stop == NX_SUCCESS && close == NX_SUCCESS
               ? 0
               : 2;
}
