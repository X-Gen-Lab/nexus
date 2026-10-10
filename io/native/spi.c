/**
 * \file            spi.c
 *
 * \brief           Settled polling SPI echo/register-file host model.
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

static nx_native_spi_state_t s_spi = {.fail_after = SIZE_MAX};
static nx_native_spi_endpoint_state_t s_endpoint = {.port = &s_spi};
const nx_spi_port_t g_nx_native_spi_port = {&nx_native_spi_ops, &s_spi};
const nx_spi_endpoint_t g_nx_native_spi = {&nx_native_spi_endpoint_ops,
                                           &s_endpoint};
const nx_spi_endpoint_t* const nx_native_spi = &g_nx_native_spi;

/** \brief Prepare independent arbitration state without selecting a device. */
nx_result_t nx_native_spi_port_configure_instance(nx_native_spi_state_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    *port = (nx_native_spi_state_t){
        .fail_after = SIZE_MAX, .opened = true, .automatic_irq = true};
    return NX_SUCCESS;
}

/** \brief Stop admits no new transaction after proving controller quiescence.
 */
nx_result_t nx_native_spi_stop_instance(nx_native_spi_state_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->active || port->busy) {
        return NX_ERROR_BUSY;
    }
    port->opened = false;
    return NX_SUCCESS;
}

/** \brief Configure only an explicit fixture's memory-backed device behavior.
 */
nx_result_t
nx_native_spi_configure_instance(nx_native_spi_endpoint_state_t* endpoint,
                                 nx_native_spi_state_t* port,
                                 uint8_t* registers, size_t length) {
    if (endpoint == NULL || port == NULL ||
        (registers == NULL && length != 0) ||
        (registers != NULL && (length == 0 || length > 256))) {
        return NX_ERROR_INVALID;
    }
    if (port->active || endpoint->cs) {
        return NX_ERROR_BUSY;
    }
    *endpoint = (nx_native_spi_endpoint_state_t){
        .port = port, .registers = registers, .register_count = length};
    return NX_SUCCESS;
}

/** \brief Prepare the default bus and explicit default device fixture. */
nx_result_t nx_native_spi_configure(uint8_t* registers, size_t length) {
    nx_result_t result = nx_native_spi_configure_instance(&s_endpoint, &s_spi,
                                                          registers, length);
    if (result != NX_SUCCESS) {
        return result;
    }
    return nx_native_spi_port_configure_instance(&s_spi);
}

/** \brief Reject before CS, execute whole transaction, then settle CS/buffers.
 */
static nx_result_t native_spi_transfer(void* context, const uint8_t* tx,
                                       uint8_t* rx, size_t length,
                                       nx_time_us_t deadline,
                                       size_t* transferred) {
    nx_native_spi_endpoint_state_t* endpoint = context;
    if (endpoint == NULL || endpoint->port == NULL || transferred == NULL ||
        length == 0 || (tx == NULL && rx == NULL)) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_native_spi_state_t* port = endpoint->port;
    if (!port->opened || port->stopping) {
        return NX_ERROR_STATE;
    }
    if (port->busy || port->active) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    endpoint->cs = true;
    nx_result_t result = NX_SUCCESS;
    uint8_t address = 0;
    bool read = false;
    for (size_t i = 0; i < length; i++) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (i == port->fail_after) {
            result = NX_ERROR_IO;
            break;
        }
        uint8_t value = tx == NULL ? 0xff : tx[i];
        uint8_t received = value;
        if (endpoint->registers != NULL) {
            if (i == 0) {
                read = (value & 0x80u) != 0;
                address = (uint8_t)(value & 0x7fu);
                received = 0;
            } else if (address >= endpoint->register_count) {
                result = NX_ERROR_IO;
                break;
            } else if (read) {
                received = endpoint->registers[address++];
            } else {
                endpoint->registers[address++] = value;
                received = 0;
            }
        }
        if (rx != NULL) {
            rx[i] = received;
        }
        (*transferred)++;
        (void)nx_native_clock_advance(1);
    }
    endpoint->cs = false;
    port->active = false;
    return result;
}

/** \brief Inject model faults without installing application callbacks. */
void nx_native_spi_fault_instance(nx_native_spi_state_t* port,
                                  size_t fail_after, bool busy) {
    if (port != NULL) {
        port->fail_after = fail_after;
        port->busy = busy;
    }
}

/** \brief Apply a fault to the explicit default bus fixture only. */
void nx_native_spi_fault(size_t fail_after, bool busy) {
    nx_native_spi_fault_instance(&s_spi, fail_after, busy);
}

/** \brief Observe exactly the selected endpoint's CS ownership. */
bool nx_native_spi_cs_active_instance(
    const nx_native_spi_endpoint_state_t* endpoint) {
    return endpoint != NULL && endpoint->cs;
}

/** \brief Observe the explicit default endpoint fixture only. */
bool nx_native_spi_cs_active(void) {
    return nx_native_spi_cs_active_instance(&s_endpoint);
}

/** \brief Clear model fault only after controller occupancy is released. */
static nx_result_t native_spi_recover(void* context) {
    nx_native_spi_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->busy || port->active) {
        return NX_ERROR_BUSY;
    }
    port->fail_after = SIZE_MAX;
    return NX_SUCCESS;
}

/** \brief One readonly controller operation table serves all static buses. */
const nx_spi_ops_t nx_native_spi_ops = {
    .recover = native_spi_recover,
    .cancel = nx_native_spi_cancel,
    .service = nx_native_spi_service,
    .stop = nx_native_spi_stop,
    .attach_wake = nx_native_spi_attach_wake,
};

/** \brief One readonly endpoint table dispatches to independent device state.
 */
const nx_spi_endpoint_ops_t nx_native_spi_endpoint_ops = {
    .transfer = native_spi_transfer,
    .submit = nx_native_spi_submit,
    .start_admitted = nx_native_spi_start_admitted,
    .on_port = nx_native_spi_on_port,
};

/** \brief Select exactly one Native face without affecting default fixtures. */
nx_result_t nx_native_spi_model_configure(const nx_spi_endpoint_t* binding,
                                          uint8_t* registers, size_t length) {
    if (binding == NULL || binding->ops != &nx_native_spi_endpoint_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_native_spi_endpoint_state_t* state = binding->context;
    return nx_native_spi_configure_instance(state, state->port, registers,
                                            length);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
void nx_native_spi_model_fault(const nx_spi_port_t* binding, size_t fail_after,
                               bool busy) {
    if (binding == NULL || binding->ops != &nx_native_spi_ops ||
        binding->context == NULL) {
        return;
    }
    nx_native_spi_fault_instance(binding->context, fail_after, busy);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
bool nx_native_spi_model_cs_active(const nx_spi_endpoint_t* binding) {
    if (binding == NULL || binding->ops != &nx_native_spi_endpoint_ops ||
        binding->context == NULL) {
        return false;
    }
    return nx_native_spi_cs_active_instance(binding->context);
}
