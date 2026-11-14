/**
 * \file            services.c
 * \brief           Public packet PEC composition on a generated I2C binding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_smbus_factory.h"

static nx_smbus_t owner;
static nx_smbus_request_t request;
static uint8_t wire[] = {0x10U, 0x20U, 0U};

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 1;
    }
    const nx_smbus_binding_t* binding =
        nx_factory_smbus_binding_ref(NX_I2C_DEVICE_REF_SENSOR);
    if (binding == NULL || binding->address != 0x2AU ||
        binding->endpoint !=
            nx_factory_i2c_device_ref(NX_I2C_DEVICE_REF_SENSOR) ||
        binding->controller != nx_factory_i2c_ref(NX_I2C_REF_BUS)) {
        return 2;
    }
    nx_request_initialize(&request.base);
    nx_result_t result = nx_smbus_initialize(&owner, binding);
    nx_time_us_t deadline = 0U;
    if (result == NX_SUCCESS) {
        result = nx_deadline_finite_after(nx_time_now_us(), 10000U, &deadline);
    }
    if (result == NX_SUCCESS) {
        result = nx_smbus_prepare(&owner, &request,
                                  (nx_smbus_tx_t){wire, 2U, wire + 2U},
                                  (nx_smbus_rx_t){NULL, 0U, 0U}, deadline);
    }
    if (result == NX_SUCCESS) {
        result = nx_smbus_submit(&owner, &request);
        if (result == NX_SUCCESS) {
            /* Retain static control and wire storage even after quarantine. */
            while (nx_request_state(&request.base) != NX_REQUEST_SETTLED) {
                (void)nx_smbus_service(&owner);
            }
            result = request.base.result;
        }
    }
    nx_result_t stopped = nx_smbus_stop(&owner);
    if (stopped != NX_SUCCESS) {
        return 3;
    }
    nx_result_t released = nx_platform_stop();
    return result == NX_SUCCESS && released == NX_SUCCESS ? 0 : 4;
}
