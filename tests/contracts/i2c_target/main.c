/**
 * \file            main.c
 * \brief           Typed target/SMBus software link and static lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
static volatile unsigned s_command;
static nx_i2c_target_request_t s_request;
static uint8_t s_received[2];
static const uint8_t s_response[2] = {0x12U, 0x34U};

/** \brief Exercise the typed public surface; physical wire tests are external.
 */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        return 1;
    const nx_i2c_target_port_t* port =
        nx_factory_i2c_target(NX_I2C_TARGET_ID_TARGET);
    nx_request_initialize(&s_request.base);
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 1000000U);
    nx_i2c_target_direction_t direction = s_command == 1U
                                              ? NX_I2C_TARGET_ALERT_RESPONSE
                                              : NX_I2C_TARGET_WRITE_READ;
    nx_result_t result = nx_i2c_target_prepare(
        port, &s_request,
        direction == NX_I2C_TARGET_ALERT_RESPONSE ? NULL : s_received,
        direction == NX_I2C_TARGET_ALERT_RESPONSE ? 0U : sizeof(s_received),
        direction == NX_I2C_TARGET_ALERT_RESPONSE ? NULL : s_response,
        direction == NX_I2C_TARGET_ALERT_RESPONSE ? 0U : sizeof(s_response),
        direction, deadline);
    if (result != NX_SUCCESS)
        return 2;
    if (nx_i2c_target_submit(port, &s_request) != NX_SUCCESS)
        return 3;
    while (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        nx_i2c_target_service(port);
        if (s_command == 2U)
            (void)nx_i2c_target_cancel(port, &s_request);
    }
    (void)nx_i2c_target_stop(port);
    (void)nx_i2c_target_recover(port);
    (void)nx_platform_suspend();
    (void)nx_platform_resume();
    (void)nx_platform_stop();
    for (;;) {
    }
}
