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
#include "nexus/io/native/model.h"

struct nx_i2c_port {
    uint8_t* memory;
    size_t size;
    uint32_t cursor;
    nx_result_t fault;
    bool stuck;
    bool active;
};
struct nx_i2c_endpoint {
    nx_i2c_port_t* port;
    uint8_t address;
};
nx_i2c_port_t g_nx_native_i2c_port;
nx_i2c_endpoint_t g_nx_native_i2c = {&g_nx_native_i2c_port, 0x76};
nx_i2c_port_t* const nx_native_i2c_port = &g_nx_native_i2c_port;
const nx_i2c_endpoint_t* const nx_native_i2c = &g_nx_native_i2c;

/** \brief Configure one fixed device model with no address scanning. */
nx_result_t nx_native_i2c_configure(uint8_t* memory, size_t length,
                                    uint8_t address) {
    if (memory == NULL || length == 0 || length > 256 || address < 8 ||
        address >= 0x78) {
        return NX_ERROR_INVALID;
    }
    g_nx_native_i2c_port = (nx_i2c_port_t){.memory = memory, .size = length};
    g_nx_native_i2c.address = address;
    return NX_SUCCESS;
}

/** \brief Apply repeated message boundaries without retaining caller buffers.
 */
nx_result_t nx_i2c_endpoint_transaction(const nx_i2c_endpoint_t* endpoint,
                                        nx_i2c_message_t* messages,
                                        size_t count, nx_time_us_t deadline,
                                        size_t* transferred) {
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
    nx_i2c_port_t* port = endpoint->port;
    if (port->memory == NULL) {
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
                port->cursor = messages[i].data[j];
            } else if (port->cursor >= port->size) {
                result = NX_ERROR_IO;
                break;
            } else if (messages[i].read) {
                messages[i].data[j] = port->memory[port->cursor++];
            } else {
                port->memory[port->cursor++] = messages[i].data[j];
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
nx_result_t nx_i2c_port_recover(nx_i2c_port_t* port) {
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
void nx_native_i2c_fault(nx_result_t result, bool stuck) {
    g_nx_native_i2c_port.fault = result;
    g_nx_native_i2c_port.stuck = stuck;
}
