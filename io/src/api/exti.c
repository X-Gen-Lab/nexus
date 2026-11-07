/**
 * \file            exti.c
 *
 * \brief           Checked typed dispatch through shared read-only provider
 *                  methods
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/exti.h"

/** \brief Reject one output byte that aliases an immutable interface object. */
static bool level_aliases(const bool* output, const void* object, size_t size) {
    const uintptr_t value = (uintptr_t)output;
    const uintptr_t first = (uintptr_t)object;
    return value >= first && value - first < size;
}

/** \brief Dispatch optional raw level without consuming any queued event. */
nx_result_t nx_exti_port_level(const nx_exti_port_t* port, bool* high) {
    if (port == NULL || port->ops == NULL || port->context == NULL ||
        high == NULL || level_aliases(high, port, sizeof(*port)) ||
        level_aliases(high, port->ops, sizeof(*port->ops))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->level != NULL ? port->ops->level(port->context, high)
                                    : NX_ERROR_UNSUPPORTED;
}

/** \brief Optional explicit hint target, with provider priority validation. */
nx_result_t nx_exti_port_attach_wake(const nx_exti_port_t* port,
                                     const nx_irq_wake_t* wake,
                                     uint8_t syscall_ceiling) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake != NULL
               ? port->ops->attach_wake(port->context, wake, syscall_ceiling)
               : NX_ERROR_UNSUPPORTED;
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_exti_port_read(const nx_exti_port_t* port,
                              nx_exti_event_t* events, size_t capacity,
                              size_t* count) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->read == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->read(port->context, events, capacity, count);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_exti_port_stop(const nx_exti_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->stop == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->stop(port->context);
}
