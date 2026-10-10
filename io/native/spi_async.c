/**
 * \file            spi_async.c
 *
 * \brief           Deterministic SPI wire completion and retained async
 * borrows.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "provider.h"

/** \brief Check all rejection conditions before asserting CS or borrowing. */
static nx_result_t admit(nx_native_spi_endpoint_state_t* endpoint,
                         nx_spi_request_t* request,
                         nx_request_state_t required) {
    if (endpoint == NULL || endpoint->port == NULL || request == NULL ||
        request->length == 0 || (request->tx == NULL && request->rx == NULL)) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_native_spi_state_t* port = endpoint->port;
    if (!port->opened || port->stopping) {
        return NX_ERROR_STATE;
    }
    if (port->active || port->busy || endpoint->cs) {
        return NX_ERROR_BUSY;
    }
    if (nx_request_state(&request->base) != required) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result =
        required == NX_REQUEST_READY
            ? nx_request_admit(&request->base, NX_REQUEST_ACTIVE)
            : nx_request_transition(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        port->active_request = request;
        port->active_endpoint = endpoint;
        port->active = true;
        port->terminal = false;
        port->terminal_result = NX_SUCCESS;
        port->transferred = 0;
        port->register_address = 0;
        port->register_read = false;
        endpoint->cs = true;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Admit a READY descriptor once into this endpoint's controller. */
nx_result_t nx_native_spi_submit(void* context, nx_spi_request_t* request) {
    return admit(context, request, NX_REQUEST_READY);
}

/** \brief Execute an existing adapter borrow without readmitting its storage.
 */
nx_result_t nx_native_spi_start_admitted(void* context,
                                         nx_spi_request_t* request) {
    return admit(context, request, NX_REQUEST_QUEUED);
}

/** \brief Cancellation records a terminal intent while retaining live storage.
 */
nx_result_t nx_native_spi_cancel(void* context, nx_spi_request_t* request) {
    nx_native_spi_state_t* port = context;
    if (port == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (port->active_request != request) {
        result = nx_request_state(&request->base) == NX_REQUEST_SETTLED
                     ? NX_SUCCESS
                     : NX_ERROR_STATE;
    } else if (!port->terminal) {
        port->terminal = true;
        port->terminal_result = NX_ERROR_CANCELLED;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief One IRQ moves one modeled wire byte or observes distinct final idle.
 */
void nx_native_spi_model_irq_step(const nx_spi_port_t* binding) {
    if (binding == NULL || binding->ops != &nx_native_spi_ops ||
        binding->context == NULL) {
        return;
    }
    nx_native_spi_state_t* port = binding->context;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_spi_request_t* request = port->active_request;
    nx_native_spi_endpoint_state_t* endpoint = port->active_endpoint;
    const nx_irq_wake_t* wake = port->wake;
    if (request == NULL || endpoint == NULL || port->terminal) {
        nx_arch_irq_restore(saved);
        return;
    }
    size_t index = port->transferred;
    if (index == request->length) {
        port->terminal = true;
        port->terminal_result =
            nx_deadline_expired(request->base.deadline, nx_time_now_us())
                ? NX_ERROR_TIMEOUT
                : NX_SUCCESS;
        nx_arch_irq_restore(saved);
        nx_irq_wake_signal(wake);
        return;
    }
    if (index == port->fail_after) {
        port->terminal = true;
        port->terminal_result = NX_ERROR_IO;
        nx_arch_irq_restore(saved);
        nx_irq_wake_signal(wake);
        return;
    }
    uint8_t value = request->tx == NULL ? 0xff : request->tx[index];
    uint8_t received = value;
    if (endpoint->registers != NULL) {
        if (index == 0) {
            port->register_read = (value & 0x80u) != 0;
            port->register_address = (uint8_t)(value & 0x7fu);
            received = 0;
        } else if (port->register_address >= endpoint->register_count) {
            port->terminal = true;
            port->terminal_result = NX_ERROR_IO;
            nx_arch_irq_restore(saved);
            nx_irq_wake_signal(wake);
            return;
        } else if (port->register_read) {
            received = endpoint->registers[port->register_address++];
        } else {
            endpoint->registers[port->register_address++] = value;
            received = 0;
        }
    }
    if (request->rx != NULL) {
        request->rx[index] = received;
    }
    ++port->transferred;
    nx_arch_irq_restore(saved);
}

/** \brief Detach CS, endpoint and descriptor before publishing settlement. */
void nx_native_spi_service(void* context) {
    nx_native_spi_state_t* port = context;
    if (port == NULL || nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_spi_request_t* request = port->active_request;
    if (request != NULL && !port->terminal &&
        nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        port->terminal = true;
        port->terminal_result = NX_ERROR_TIMEOUT;
    }
    bool automatic = port->automatic_irq;
    nx_arch_irq_restore(saved);
    if (automatic) {
        const nx_spi_port_t face = {&nx_native_spi_ops, port};
        nx_native_spi_model_irq_step(&face);
    }
    saved = nx_arch_irq_save();
    request = port->active_request;
    if (request == NULL || !port->terminal) {
        nx_arch_irq_restore(saved);
        return;
    }
    if (nx_request_state(&request->base) == NX_REQUEST_ACTIVE &&
        port->terminal_result != NX_SUCCESS) {
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
    }
    if (port->hold_drain) {
        nx_request_state_t state = nx_request_state(&request->base);
        if (state == NX_REQUEST_ACTIVE || state == NX_REQUEST_DRAINING) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
        nx_arch_irq_restore(saved);
        return;
    }
    nx_result_t result = port->terminal_result;
    size_t transferred = port->transferred;
    port->active_endpoint->cs = false;
    port->active_endpoint = NULL;
    port->active_request = NULL;
    port->active = false;
    port->terminal = false;
    nx_arch_irq_restore(saved);
    nx_request_settle(&request->base, result, transferred);
}

/** \brief Stop cancels and continues drain while preserving quarantined loans.
 */
nx_result_t nx_native_spi_stop(void* context) {
    nx_native_spi_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    port->stopping = true;
    nx_spi_request_t* request = port->active_request;
    nx_arch_irq_restore(saved);
    if (request != NULL) {
        (void)nx_native_spi_cancel(port, request);
        nx_native_spi_service(port);
    }
    nx_result_t result = nx_native_spi_stop_instance(port);
    if (result == NX_SUCCESS) {
        port->wake = NULL;
    }
    return result;
}

/** \brief Validate and attach a borrowed target to the model's actual priority.
 */
nx_result_t nx_native_spi_attach_wake(void* context, const nx_irq_wake_t* wake,
                                      uint8_t syscall_ceiling) {
    nx_native_spi_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!port->opened || port->stopping) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_irq_wake_validate(wake, 5, 4, syscall_ceiling);
    if (result == NX_SUCCESS) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        port->wake = wake;
        nx_arch_irq_restore(saved);
    }
    return result;
}

/** \brief Exact controller identity is checked before an adapter admits work.
 */
bool nx_native_spi_on_port(const void* context, const nx_spi_port_t* binding) {
    const nx_native_spi_endpoint_state_t* endpoint = context;
    return endpoint != NULL && binding != NULL &&
           binding->ops == &nx_native_spi_ops &&
           endpoint->port == binding->context;
}

/** \brief Select explicit or automatic simulated IRQ stepping after quiescence.
 */
void nx_native_spi_model_async_configure(const nx_spi_port_t* binding,
                                         bool automatic_irq) {
    if (binding != NULL && binding->ops == &nx_native_spi_ops &&
        binding->context != NULL) {
        nx_native_spi_state_t* port = binding->context;
        if (!port->active) {
            port->automatic_irq = automatic_irq;
        }
    }
}

/** \brief Inject inability to prove drain without revoking retained requests.
 */
void nx_native_spi_model_drain_hold(const nx_spi_port_t* binding, bool hold) {
    if (binding != NULL && binding->ops == &nx_native_spi_ops &&
        binding->context != NULL) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        nx_native_spi_state_t* port = binding->context;
        port->hold_drain = hold;
        nx_arch_irq_restore(saved);
    }
}
