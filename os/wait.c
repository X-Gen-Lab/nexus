/**
 * \file            wait.c
 * \brief           Predicate wait with arm and recheck
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/wait.h"

/** \brief Wait without making notification the result authority. */
nx_result_t nx_wait_until(const nx_wait_port_t* port, bool (*ready)(void*),
                          void* context, uint64_t deadline_us) {
    if (port == NULL || port->arm == NULL || port->wait == NULL ||
        ready == NULL) {
        return NX_ERROR_INVALID;
    }
    for (;;) {
        if (ready(context)) {
            return NX_SUCCESS;
        }
        uint32_t sequence = port->arm(port->context);
        if (ready(context)) {
            return NX_SUCCESS;
        }
        nx_result_t result = port->wait(port->context, sequence, deadline_us);
        if (result != NX_SUCCESS) {
            /* Completion concurrent with a timeout still wins observation. */
            return ready(context) ? NX_SUCCESS : result;
        }
    }
}
