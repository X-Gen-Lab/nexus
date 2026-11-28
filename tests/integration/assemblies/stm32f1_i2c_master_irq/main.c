/**
 * \file            main.c
 * \brief           Link the F103RB typed I2C1 IRQ writer assembly.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static const uint8_t payload[] = {0x20U, 0x42U};
static nx_i2c_request_t request;
static nx_i2c_message_t write;

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_i2c_port_t* const bus = nx_factory_i2c_ref(NX_I2C_REF_I2C1);
    const nx_i2c_endpoint_t* const endpoint =
        nx_factory_i2c_device_ref(NX_I2C_DEVICE_REF_WRITER);
    if (bus == NULL || endpoint == NULL) {
        return 2;
    }
    nx_request_initialize(&request.base);
    write = nx_i2c_message_write(payload, sizeof(payload));
    if (nx_i2c_request_prepare(&request, &write, 1U,
                               nx_time_now_us() + 10000U) != NX_SUCCESS ||
        nx_i2c_endpoint_submit(endpoint, &request) != NX_SUCCESS) {
        return 3;
    }
    if (nx_i2c_port_cancel(bus, &request) != NX_SUCCESS) {
        return 4;
    }
    nx_i2c_port_service(bus);
    if (nx_request_state(&request.base) != NX_REQUEST_SETTLED) {
        return 5;
    }
    return nx_platform_stop() == NX_SUCCESS ? 0 : 6;
}
