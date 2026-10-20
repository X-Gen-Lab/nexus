/**
 * \file            pin-states.c
 * \brief           Public installed-SDK named pin-state linking
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

/** \brief Keep the generated owner alive if withdrawal cannot complete. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        for (;;) {
        }
    }
    return result;
}

/** \brief Linking does not execute pad transitions or qualify board wiring. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_pin_state_port_t* status =
        nx_factory_pin_state(NX_PIN_STATE_ID_STATUS);
    if (status == NULL ||
        nx_pin_state_select(status, NX_PIN_STATE_SLEEP) != NX_SUCCESS ||
        nx_pin_state_select(status, NX_PIN_STATE_DEFAULT) != NX_SUCCESS) {
        return close_platform(2);
    }
    return close_platform(0);
}
