/**
 * \file            main.c
 * \brief           Link the opt-in F103RB first-fault record
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"

#if defined(NX_F103RB_FAULT_REFERENCE)
#include "nexus/io/watchdog.h"
#include "nexus/soc/stm32f103rb_fault.h"

static nx_fault_record_t sample;

/** \brief Exercise observation from a known-good C execution path. */
static bool check_record(void) {
    const uint32_t reason = 42U;
    if (nx_stm32f103rb_fault_capture(&sample, 1U, reason, 0U) != NX_SUCCESS ||
        nx_stm32f103rb_fault_capture(&sample, 2U, 0U, 0U) != NX_ERROR_BUSY) {
        return false;
    }
    const uint32_t causes = nx_reset_cause();
    nx_reset_record_t boot;
    return nx_reset_record_read(&sample, causes, &boot) == NX_SUCCESS &&
           boot.reset_causes == causes && boot.has_fault &&
           boot.fault.sequence == 1U && boot.fault.reason == reason;
}
#endif

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
#if defined(NX_F103RB_FAULT_REFERENCE)
    if (!check_record()) {
        return 2;
    }
#endif
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
