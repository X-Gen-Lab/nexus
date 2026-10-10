/**
 * \file            i2c.c
 *
 * \brief           Repeated START register-file I2C model and explicit
 *                  recovery.
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

static nx_native_i2c_state_t s_i2c;
static nx_native_i2c_endpoint_state_t s_endpoint = {.port = &s_i2c,
                                                    .address = 0x76};
const nx_i2c_port_t g_nx_native_i2c_port = {&nx_native_i2c_ops, &s_i2c};
const nx_i2c_endpoint_t g_nx_native_i2c = {&nx_native_i2c_endpoint_ops,
                                           &s_endpoint};
const nx_i2c_port_t* const nx_native_i2c_port = &g_nx_native_i2c_port;
const nx_i2c_endpoint_t* const nx_native_i2c = &g_nx_native_i2c;

/** \brief Prepare one idle bus independently of its addressed devices. */
nx_result_t nx_native_i2c_port_configure_instance(nx_native_i2c_state_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    *port = (nx_native_i2c_state_t){.opened = true};
    return NX_SUCCESS;
}

/** \brief Disable transaction admission only after the current executor exits.
 */
nx_result_t nx_native_i2c_stop_instance(nx_native_i2c_state_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    port->opened = false;
    return NX_SUCCESS;
}

/** \brief Configure one fixed device model with no address scanning. */
nx_result_t
nx_native_i2c_configure_instance(nx_native_i2c_endpoint_state_t* endpoint,
                                 nx_native_i2c_state_t* port, uint8_t* memory,
                                 size_t length, uint8_t address) {
    if (endpoint == NULL || port == NULL || memory == NULL || length == 0 ||
        length > 256 || address < 8 || address >= 0x78) {
        return NX_ERROR_INVALID;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    *endpoint = (nx_native_i2c_endpoint_state_t){
        .port = port, .memory = memory, .size = length, .address = address};
    return NX_SUCCESS;
}

/** \brief Prepare the default bus and explicit default address fixture. */
nx_result_t nx_native_i2c_configure(uint8_t* memory, size_t length,
                                    uint8_t address) {
    nx_result_t result = nx_native_i2c_configure_instance(
        &s_endpoint, &s_i2c, memory, length, address);
    if (result != NX_SUCCESS) {
        return result;
    }
    return nx_native_i2c_port_configure_instance(&s_i2c);
}

/** \brief Apply repeated message boundaries without retaining caller buffers.
 */
static nx_result_t native_i2c_transaction(void* context,
                                          nx_i2c_message_t* messages,
                                          size_t count, nx_time_us_t deadline,
                                          size_t* transferred) {
    nx_native_i2c_endpoint_state_t* endpoint = context;
    if (endpoint == NULL || endpoint->port == NULL || messages == NULL ||
        count == 0 || transferred == NULL) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0;
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (messages[i].data == NULL || messages[i].length == 0 ||
            messages[i].length > SIZE_MAX - total) {
            return NX_ERROR_INVALID;
        }
        total += messages[i].length;
    }
    *transferred = 0;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_native_i2c_state_t* port = endpoint->port;
    if (!port->opened || endpoint->memory == NULL) {
        return NX_ERROR_STATE;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (port->stuck || port->fault != NX_SUCCESS) {
        return port->stuck ? NX_ERROR_IO : port->fault;
    }
    port->active = true;
    nx_result_t result = NX_SUCCESS;
    for (size_t i = 0; i < count && result == NX_SUCCESS; i++) {
        for (size_t j = 0; j < messages[i].length; j++) {
            if (nx_deadline_expired(deadline, nx_time_now_us())) {
                result = NX_ERROR_TIMEOUT;
                break;
            }
            if (!messages[i].read && j == 0) {
                endpoint->cursor = messages[i].data[j];
            } else if (endpoint->cursor >= endpoint->size) {
                result = NX_ERROR_IO;
                break;
            } else if (messages[i].read) {
                messages[i].data[j] = endpoint->memory[endpoint->cursor++];
            } else {
                endpoint->memory[endpoint->cursor++] = messages[i].data[j];
            }
            (*transferred)++;
            (void)nx_native_clock_advance(1);
        }
    }
    port->active = false;
    return result;
}

/** \brief Distinguish a successful recovery from replaying a failed transfer.
 */
static nx_result_t native_i2c_recover(void* context) {
    nx_native_i2c_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (port->stuck) {
        return NX_ERROR_IO;
    }
    port->fault = NX_SUCCESS;
    return NX_SUCCESS;
}

/** \brief Inject explicit wire-model faults rather than returning fake success.
 */
void nx_native_i2c_fault_instance(nx_native_i2c_state_t* port,
                                  nx_result_t result, bool stuck) {
    if (port != NULL) {
        port->fault = result;
        port->stuck = stuck;
    }
}

/** \brief Apply explicit faults only to the default bus fixture. */
void nx_native_i2c_fault(nx_result_t result, bool stuck) {
    nx_native_i2c_fault_instance(&s_i2c, result, stuck);
}

/** \brief Controller operations share code while preserving arbitration state.
 */
const nx_i2c_ops_t nx_native_i2c_ops = {.recover = native_i2c_recover};

/** \brief Addressed devices share code without sharing their memory or cursor.
 */
const nx_i2c_endpoint_ops_t nx_native_i2c_endpoint_ops = {
    .transaction = native_i2c_transaction};

/** \brief Select exactly one Native face without affecting default fixtures. */
nx_result_t nx_native_i2c_model_configure(const nx_i2c_endpoint_t* binding,
                                          uint8_t* memory, size_t length,
                                          uint8_t address) {
    if (binding == NULL || binding->ops != &nx_native_i2c_endpoint_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_native_i2c_endpoint_state_t* state = binding->context;
    return nx_native_i2c_configure_instance(state, state->port, memory, length,
                                            address);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
void nx_native_i2c_model_fault(const nx_i2c_port_t* binding, nx_result_t result,
                               bool stuck) {
    if (binding == NULL || binding->ops != &nx_native_i2c_ops ||
        binding->context == NULL) {
        return;
    }
    nx_native_i2c_fault_instance(binding->context, result, stuck);
}
