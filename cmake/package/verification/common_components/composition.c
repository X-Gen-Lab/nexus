/**
 * \file            composition.c
 * \brief           Explicit RTT and read-only Shell SDK composition
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include "nexus/components/rtt.h"
#include "nexus/components/shell_storage.h"
#include "nexus_bindings.h"
#if defined(NX_COMPONENTS_F407)
#include "nexus/components/cm_backtrace_adapter.h"
#endif

static nx_rtt_t rtt;
static uint8_t up[128];
static uint8_t down[16];
static nx_shell_t shell;
static char line_history[128];

/** \brief The finite software transport has no available input. */
static nx_result_t read_empty(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count) {
    (void)context;
    (void)bytes;
    (void)capacity;
    *count = 0U;
    return NX_ERROR_EMPTY;
}

/** \brief Consume the complete borrowed prompt without persistent output. */
static nx_result_t write_discard(void* context, const void* bytes, size_t count,
                                 nx_time_us_t deadline) {
    (void)context;
    (void)bytes;
    (void)count;
    (void)deadline;
    return NX_SUCCESS;
}

/** \brief Model execution on Native; target linkage does not establish HIL. */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS) {
        return 10;
    }
    int result = common_component_application();
    if (result != 0) {
        return 20 + result;
    }
    nx_shell_io_t io = {NULL, read_empty, write_discard};
    static const nx_shell_diagnostics_t diagnostics = {.version =
                                                           "sdk-components"};
    if (nx_rtt_init(&rtt, 1U, "sdk", up, sizeof(up), down, sizeof(down)) !=
            NX_SUCCESS ||
        nx_shell_init(&shell, io, line_history, sizeof(line_history),
                      &diagnostics, 100U) != NX_SUCCESS ||
        nx_shell_step(&shell, 100U) != NX_ERROR_EMPTY ||
        nx_shell_stop(&shell) != NX_SUCCESS ||
        nx_rtt_stop(&rtt) != NX_SUCCESS) {
        return 30;
    }
#if defined(NX_COMPONENTS_F407)
    size_t depth = 0U;
    if (nx_cm_backtrace_init("sdk", "f407", "common-components") !=
            NX_SUCCESS ||
        nx_cm_backtrace_record(NULL, NULL, NULL, 0U, 0U, &depth) !=
            NX_ERROR_INVALID) {
        return 40;
    }
#endif
    return nx_platform_stop() == NX_SUCCESS ? 0 : 50;
}
