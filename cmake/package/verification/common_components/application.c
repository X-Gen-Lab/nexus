/**
 * \file            application.c
 * \brief           Public C11 consumer without supplier implementation headers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include "nexus/algorithms/cmsis_dsp.h"
#include "nexus/algorithms/numeric.h"
#include "nexus/components/fatfs.h"
#include "nexus/components/flashdb.h"
#include "nexus/components/littlefs.h"
#include "nexus/components/periodic.h"
#include "nexus/components/sfud.h"

#if __has_include("flashdb.h") ||                                              \
                  __has_include(                                               \
                      "lfs.h") ||                                              \
                      __has_include(                                           \
                          "ff.h") ||                                           \
                          __has_include(                                       \
                              "sfud.h") ||                                     \
                              __has_include(                                   \
                                  "arm_math.h") ||                             \
                                  __has_include(                               \
                                      "SEGGER_RTT.h") ||                       \
                                      __has_include(                           \
                                          "shell.h") ||                        \
                                          __has_include("cm_backtrace.h")
#error Supplier headers leaked into the generic application
#endif

/** \brief This consumer performs no database calls requiring an active scope.
 */
const nx_flashdb_operation_t* nx_flashdb_operation_get(void) {
    return NULL;
}

/** \brief No filesystem operation scope is published by this admission probe.
 */
const nx_fatfs_operation_t* nx_fatfs_operation_get(void) {
    return NULL;
}

/** \brief Pure success and rejected admission retain selected implementations.
 */
int common_component_application(void) {
    float output = 0.0F;
    const float first[] = {1.0F, 2.0F};
    const float second[] = {3.0F, 4.0F};
    nx_periodic_t periodic;
    nx_periodic_release_t release;
    if (nx_clamp_f32(2.0F, -1.0F, 1.0F, &output) != NX_SUCCESS ||
        output != 1.0F ||
        nx_cmsis_dot_f32(first, second, 2U, &output) != NX_SUCCESS ||
        output != 11.0F ||
        nx_periodic_reset(&periodic, 0U, 10U, NX_PERIODIC_SKIP) != NX_SUCCESS ||
        nx_periodic_poll(&periodic, 20U, 1U, &release) != NX_SUCCESS ||
        release.first_due != 20U || release.skipped != 1U) {
        return 1;
    }
    if (nx_flashdb_workspace_bytes() == 0U ||
        nx_littlefs_workspace_bytes() == 0U ||
        nx_fatfs_workspace_bytes() == 0U || nx_sfud_workspace_bytes() == 0U ||
        nx_flashdb_open(NULL, NX_FLASHDB_EXISTING_ONLY) != NX_ERROR_STATE ||
        nx_littlefs_mount(NULL, 100U) != NX_ERROR_INVALID ||
        nx_fatfs_mount(NULL) != NX_ERROR_STATE) {
        return 2;
    }
    nx_clock_t clock = {0};
    if (nx_sfud_init(NULL, NULL, clock, NX_SFUD_W25Q128JV, 0U, NULL, 0U,
                     100U) != NX_ERROR_INVALID) {
        return 3;
    }
    return 0;
}
