/**
 * \file            exti.c
 *
 * \brief           Fixed EXTI bounded event/overflow and source drain model.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/native/model.h"

struct nx_exti_port {
    nx_exti_event_t* events;
    size_t capacity;
    size_t head;
    size_t count;
    uint8_t line;
    nx_exti_edge_t edge;
    bool opened;
    bool loss;
    nx_time_us_t loss_timestamp;
};
nx_exti_port_t g_nx_native_exti;
nx_exti_port_t* const nx_native_exti = &g_nx_native_exti;

/** \brief Configure one explicit fixture line and exactly allocated queue. */
nx_result_t nx_native_exti_configure(uint8_t line, nx_exti_edge_t edge,
                                     nx_exti_event_t* events, size_t capacity) {
    if (line > 15 || edge < NX_EXTI_RISING || edge > NX_EXTI_BOTH ||
        events == NULL || capacity == 0 || capacity > SIZE_MAX / 2 ||
        capacity > SIZE_MAX / sizeof(nx_exti_event_t) ||
        (uintptr_t)events % _Alignof(nx_exti_event_t) != 0) {
        return NX_ERROR_INVALID;
    }
    g_nx_native_exti = (nx_exti_port_t){.events = events,
                                        .capacity = capacity,
                                        .line = line,
                                        .edge = edge,
                                        .opened = true};
    return NX_SUCCESS;
}

/** \brief Add one event, preserving FIFO order and explicit dropped-edge loss.
 */
nx_result_t nx_native_exti_emit(uint8_t line, nx_exti_edge_t edge) {
    nx_arch_irq_state_t token = nx_arch_irq_save();
    nx_exti_port_t* port = &g_nx_native_exti;
    if (!port->opened) {
        nx_arch_irq_restore(token);
        return NX_ERROR_STATE;
    }
    if (line != port->line ||
        (edge != NX_EXTI_RISING && edge != NX_EXTI_FALLING)) {
        nx_arch_irq_restore(token);
        return NX_ERROR_INVALID;
    }
    if ((edge & port->edge) == 0) {
        nx_arch_irq_restore(token);
        return NX_SUCCESS;
    }
    if (port->count == port->capacity || port->loss) {
        if (!port->loss) {
            port->loss_timestamp = nx_time_now_us();
        }
        port->loss = true;
        nx_arch_irq_restore(token);
        return NX_ERROR_OVERFLOW;
    }
    size_t tail = port->head + port->count;
    if (tail >= port->capacity) {
        tail -= port->capacity;
    }
    port->events[tail] = (nx_exti_event_t){nx_time_now_us(), 0, line, edge};
    port->count++;
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}

/** \brief Copy one fact at a time under bounded IRQ metadata exclusion. */
nx_result_t nx_exti_port_read(nx_exti_port_t* port, nx_exti_event_t* events,
                              size_t capacity, size_t* count) {
    if (port == NULL || events == NULL || capacity == 0 || count == NULL) {
        return NX_ERROR_INVALID;
    }
    *count = 0;
    while (*count < capacity) {
        nx_arch_irq_state_t token = nx_arch_irq_save();
        if (port->count == 0) {
            if (port->loss) {
                events[*count] =
                    (nx_exti_event_t){port->loss_timestamp, NX_EXTI_EVENT_LOSS,
                                      port->line, NX_EXTI_BOTH};
                port->loss = false;
                nx_arch_irq_restore(token);
                (*count)++;
            } else {
                nx_arch_irq_restore(token);
            }
            break;
        }
        events[*count] = port->events[port->head++];
        if (port->head == port->capacity) {
            port->head = 0;
        }
        port->count--;
        nx_arch_irq_restore(token);
        (*count)++;
    }
    return *count == 0 ? NX_ERROR_EMPTY : NX_SUCCESS;
}

/** \brief Disable model source and remove all future storage accesses. */
nx_result_t nx_exti_port_stop(nx_exti_port_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    port->opened = false;
    port->count = 0;
    port->loss = false;
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}
