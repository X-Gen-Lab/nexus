/**
 * \file            selected.c
 * \brief           Unexecuted typed device graph over an unconnected I2C port
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/tmp117_i2c.h"
#include "nexus_bindings.h"

nx_tmp117_t cost_device;
nx_tmp117_sample_t cost_sample;
static unsigned unconnected_context;

/** \brief Software link fixture only; no physical I2C/PCB is claimed. */
static nx_result_t unconnected(void* context, nx_i2c_message_t* messages,
                               size_t count, nx_time_us_t deadline,
                               size_t* transferred) {
    (void)context;
    (void)messages;
    (void)count;
    (void)deadline;
    *transferred = 0U;
    return NX_ERROR_UNSUPPORTED;
}

static nx_time_us_t now(void* context) {
    (void)context;
    return nx_time_now_us();
}

static const nx_i2c_endpoint_ops_t operations = {.transaction = unconnected};
static const nx_i2c_endpoint_t endpoint = {&operations, &unconnected_context};

int main(void) {
    nx_platform_start_result_t boot = nx_platform_start();
    if (boot.primary != NX_SUCCESS || boot.cleanup != NX_SUCCESS ||
        boot.remaining_effects != 0U) {
        return 1;
    }
    const nx_tmp117_config_t config = {1U, 1U, 0};
    nx_tmp117_transport_port_t transport =
        nx_tmp117_i2c_transport(&endpoint, (nx_clock_t){now, NULL});
    nx_result_t result =
        nx_tmp117_init(&cost_device, transport, &config,
                       nx_deadline_after(nx_time_now_us(), 100000U));
    if (result == NX_SUCCESS) {
        result = nx_tmp117_begin(&cost_device,
                                 nx_deadline_after(nx_time_now_us(), 100000U));
        if (result == NX_SUCCESS) {
            do {
                result = nx_tmp117_poll(&cost_device, &cost_sample);
            } while (result == NX_ERROR_BUSY);
        }
        nx_result_t cleanup = nx_tmp117_cancel(
            &cost_device, nx_deadline_after(nx_time_now_us(), 100000U));
        if (cleanup != NX_SUCCESS) {
            result = cleanup;
        }
    }
    nx_tmp117_stop(&cost_device);
    nx_result_t stopped = nx_platform_stop();
    return result == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}
