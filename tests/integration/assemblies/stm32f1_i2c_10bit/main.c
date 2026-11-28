/**
 * \file            main.c
 * \brief           Link seven- and ten-bit endpoints on one F103RB I2C1
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS || start.cleanup != NX_SUCCESS)
        return 1;
    const nx_i2c_port_t* bus = nx_factory_i2c_ref(NX_I2C_REF_I2C1);
    const nx_i2c_endpoint_t* short_address =
        nx_factory_i2c_device_ref(NX_I2C_DEVICE_REF_SHORT_ADDRESS);
    const nx_i2c_endpoint_t* long_address =
        nx_factory_i2c_device_ref(NX_I2C_DEVICE_REF_LONG_ADDRESS);
    uint8_t received = 0U;
    nx_i2c_message_t message = nx_i2c_message_read(&received, 1U);
    size_t moved = 0U;
    if (!nx_i2c_endpoint_on_port(short_address, bus) ||
        !nx_i2c_endpoint_on_port(long_address, bus) ||
        nx_i2c_endpoint_transaction(long_address, &message, 1U, 0U, &moved) ==
            NX_SUCCESS)
        return 2;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
