/**
 * \file            main.c
 * \brief           Link the exact public F103 I2C1 target and both vectors.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_i2c_target_request_t request;
#if defined(NX_F1_TARGET_TX_FIXTURE) || defined(NX_F1_TARGET_WRITE_READ_FIXTURE)
static const uint8_t transmit[] = {0x21U, 0xA2U};
#endif
#if !defined(NX_F1_TARGET_TX_FIXTURE)
static uint8_t receive[16];
#endif

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_i2c_target_port_t* const target =
        nx_factory_i2c_target_ref(NX_I2C_TARGET_REF_TARGET);
    const nx_io_capability_t* const capability =
        nx_factory_i2c_target_capability_ref(NX_I2C_TARGET_REF_TARGET);
#if defined(NX_F1_TARGET_WRITE_READ_FIXTURE)
    const uint32_t transfer_limit = 2U * NX_I2C_TARGET_MAX_BYTES;
#else
    const uint32_t transfer_limit = NX_I2C_TARGET_MAX_BYTES;
#endif
    if (target == NULL || capability == NULL ||
        capability->max_transfer_units != transfer_limit) {
        return 2;
    }
    nx_request_initialize(&request.base);
#if defined(NX_F1_TARGET_TX_FIXTURE)
    const nx_result_t prepared = nx_i2c_target_prepare(
        target, &request, NULL, 0U, transmit, sizeof(transmit),
        NX_I2C_TARGET_TRANSMIT, nx_time_now_us() + 10000U);
#elif defined(NX_F1_TARGET_WRITE_READ_FIXTURE)
    const nx_result_t prepared = nx_i2c_target_prepare(
        target, &request, receive, sizeof(receive), transmit, sizeof(transmit),
        NX_I2C_TARGET_WRITE_READ, nx_time_now_us() + 10000U);
#else
    const nx_result_t prepared = nx_i2c_target_prepare(
        target, &request, receive, sizeof(receive), NULL, 0U,
        NX_I2C_TARGET_RECEIVE, nx_time_now_us() + 10000U);
#endif
    if (prepared != NX_SUCCESS ||
        nx_i2c_target_submit(target, &request) != NX_SUCCESS) {
        return 3;
    }
    (void)nx_i2c_target_cancel(target, &request);
    nx_i2c_target_service(target);
    if (nx_request_state(&request.base) != NX_REQUEST_SETTLED ||
        nx_i2c_target_recover(target) != NX_SUCCESS ||
        nx_i2c_target_stop(target) != NX_SUCCESS) {
        return 4;
    }
    return nx_platform_stop() == NX_SUCCESS ? 0 : 5;
}
