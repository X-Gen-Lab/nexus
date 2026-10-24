/**
 * \file            pin_state.c
 * \brief           Checked dispatch of explicit static GPIO states
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/pin_state.h"

/** \brief Check descriptor arithmetic before reading an immutable face. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}
nx_result_t nx_pin_state_select(const nx_pin_state_port_t* port,
                                nx_pin_state_selection_t state) {
    if (!span(port, sizeof(*port), _Alignof(nx_pin_state_port_t)) ||
        !span(port->ops, sizeof(*port->ops), _Alignof(nx_pin_state_ops_t)) ||
        port->context == NULL ||
        (state != NX_PIN_STATE_DEFAULT && state != NX_PIN_STATE_SLEEP))
        return NX_ERROR_INVALID;
    return port->ops->select == NULL ? NX_ERROR_UNSUPPORTED
                                     : port->ops->select(port->context, state);
}
