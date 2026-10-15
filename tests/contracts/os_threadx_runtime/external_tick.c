/**
 * \file            external_tick.c
 * \brief           Explicit negative provider for software link acceptance
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/threadx.h"
/** \brief No physical source is bound; successful link does not enable Tick. */
nx_result_t nx_threadx_external_tick_setup(uint32_t tick_hz) {
    (void)tick_hz;
    return NX_ERROR_UNSUPPORTED;
}
