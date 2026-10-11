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
#include "provider.h"

static nx_native_exti_state_t s_exti;
const nx_exti_port_t g_nx_native_exti = {&nx_native_exti_ops, &s_exti};
const nx_exti_port_t* const nx_native_exti = &g_nx_native_exti;

/** \brief Configure one explicit fixture line and exactly allocated queue. */
nx_result_t nx_native_exti_configure_instance(nx_native_exti_state_t* port,
                                              uint8_t line, nx_exti_edge_t edge,
                                              nx_exti_event_t* events,
                                              size_t capacity) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (line > 15 || edge < NX_EXTI_RISING || edge > NX_EXTI_BOTH ||
        events == NULL || capacity == 0 || capacity > SIZE_MAX / 2 ||
        capacity > SIZE_MAX / sizeof(nx_exti_event_t) ||
        (uintptr_t)events % _Alignof(nx_exti_event_t) != 0) {
        return NX_ERROR_INVALID;
    }
    *port = (nx_native_exti_state_t){.events = events,
                                     .capacity = capacity,
                                     .line = line,
                                     .edge = edge,
                                     .opened = true};
    return NX_SUCCESS;
}

/** \brief Operate on the explicit default fixture only. */
nx_result_t nx_native_exti_configure(uint8_t line, nx_exti_edge_t edge,
                                     nx_exti_event_t* events, size_t capacity) {
    return nx_native_exti_configure_instance(&s_exti, line, edge, events,
                                             capacity);
}

/** \brief Add one event, preserving FIFO order and explicit dropped-edge loss.
 */
nx_result_t nx_native_exti_emit_instance(nx_native_exti_state_t* port,
                                         uint8_t line, nx_exti_edge_t edge) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();

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
        const nx_irq_wake_t* wake = port->wake;
        nx_arch_irq_restore(token);
        (void)nx_irq_wake_signal(wake);
        return NX_ERROR_OVERFLOW;
    }
    size_t tail = port->head + port->count;
    if (tail >= port->capacity) {
        tail -= port->capacity;
    }
    port->events[tail] = (nx_exti_event_t){nx_time_now_us(), 0, line, edge};
    port->count++;
    const nx_irq_wake_t* wake = port->wake;
    nx_arch_irq_restore(token);
    (void)nx_irq_wake_signal(wake);
    return NX_SUCCESS;
}

/** \brief Operate on the explicit default fixture only. */
nx_result_t nx_native_exti_emit(uint8_t line, nx_exti_edge_t edge) {
    return nx_native_exti_emit_instance(&s_exti, line, edge);
}

/** \brief Copy one fact at a time under bounded IRQ metadata exclusion. */
static nx_result_t native_exti_read(void* context, nx_exti_event_t* events,
                                    size_t capacity, size_t* count) {
    nx_native_exti_state_t* port = context;
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
static nx_result_t native_exti_stop(void* context) {
    nx_native_exti_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t token = nx_arch_irq_save();
    port->wake = NULL;
    port->opened = false;
    port->count = 0;
    port->loss = false;
    nx_arch_irq_restore(token);
    return NX_SUCCESS;
}

/** \brief Bind an explicit sink using the maintained model IRQ priority. */
static nx_result_t native_exti_attach_wake(void* context,
                                           const nx_irq_wake_t* wake,
                                           uint8_t syscall_ceiling) {
    nx_native_exti_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!port->opened) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 240U, 4U,
                                    syscall_ceiling > 5U ? syscall_ceiling
                                                         : 5U};
    const nx_irq_source_t source = {0, 5U, 0U};
    nx_result_t result =
        nx_native_irq_wake_validate(wake, &policy, &source, syscall_ceiling);
    if (result == NX_SUCCESS) {
        port->wake = wake;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief One readonly operation table is shared by every Native instance. */
const nx_exti_ops_t nx_native_exti_ops = {
    .read = native_exti_read,
    .stop = native_exti_stop,
    .attach_wake = native_exti_attach_wake,
};

/** \brief Select exactly one Native face without affecting default fixtures. */
nx_result_t nx_native_exti_model_configure(const nx_exti_port_t* binding,
                                           uint8_t line, nx_exti_edge_t edge,
                                           nx_exti_event_t* events,
                                           size_t capacity) {
    if (binding == NULL || binding->ops != &nx_native_exti_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_native_exti_configure_instance(binding->context, line, edge,
                                             events, capacity);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
nx_result_t nx_native_exti_model_emit(const nx_exti_port_t* binding,
                                      uint8_t line, nx_exti_edge_t edge) {
    if (binding == NULL || binding->ops != &nx_native_exti_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_native_exti_emit_instance(binding->context, line, edge);
}
