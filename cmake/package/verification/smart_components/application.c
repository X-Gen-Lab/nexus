/**
 * \file            application.c
 * \brief           Public C11 faces keep optional supplier storage private
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include "nexus/algorithms/cmsis_nn.h"
#include "nexus/components/atecc608b.h"
#include "nexus/components/canopen.h"
#include "nexus/components/lsm6dsox.h"
#include "nexus/components/lvgl.h"

#if __has_include("arm_nnfunctions.h") ||                                      \
                  __has_include(                                               \
                      "cryptoauthlib.h") ||                                    \
                      __has_include("lsm6dsox_reg.h") ||                       \
                                    __has_include("301/CO_NMT_Heartbeat.h") || \
                                                  __has_include("lvgl.h")
#error Supplier headers leaked into the generic application
#endif

const nx_atecc608b_operation_t* nx_atecc608b_operation_get(void) {
    return NULL;
}

int smart_component_application(void) {
    static const int8_t weights[] = {1, 2, -1, -2, 0, 1};
    static const int32_t bias[] = {2, 1};
    static const int8_t input[] = {3, -2, 5};
    const nx_cmsis_nn_dense_s8_t layer = {3U, 2U,        weights, bias, 0,
                                          0,  INT32_MAX, 0,       -128, 127};
    int8_t output[2] = {99, 98};
    if (nx_cmsis_nn_dense_s8(&layer, input, sizeof(input), output,
                             sizeof(output)) != NX_SUCCESS ||
        output[0] != -4 || output[1] != 0) {
        return 1;
    }
    if (nx_atecc608b_workspace_bytes() == 0U ||
        nx_canopen_workspace_bytes(1U) == 0U ||
        nx_lvgl_workspace_bytes() == 0U) {
        return 2;
    }
    const nx_lsm6dsox_transport_port_t transport = {0};
    const nx_lsm6dsox_config_t sensor = {1U, 1U, NX_LSM6DSOX_DIRECT};
    if (nx_atecc608b_init(NULL, NULL, NULL, 0U, 100U) != NX_ERROR_INVALID ||
        nx_lsm6dsox_init(NULL, transport, &sensor, 100U) != NX_ERROR_INVALID ||
        nx_canopen_init(NULL, NULL, NULL, 0U) != NX_ERROR_INVALID ||
        nx_lvgl_init(NULL, NULL, NULL, 0U) != NX_ERROR_INVALID) {
        return 3;
    }
    return 0;
}
