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
#include "nexus/io/native/model.h"

struct nx_spi_port {
    uint8_t* registers;
    size_t register_count;
    size_t fail_after;
    bool busy;
    bool cs;
};
struct nx_spi_endpoint {
    nx_spi_port_t* port;
};
static nx_spi_port_t s_spi = {.fail_after = SIZE_MAX};
nx_spi_endpoint_t g_nx_native_spi = {&s_spi};
const nx_spi_endpoint_t* const nx_native_spi = &g_nx_native_spi;

/** \brief Configure only an explicit fixture's memory-backed device behavior.
 */
nx_result_t nx_native_spi_configure(uint8_t* registers, size_t length) {
    if ((registers == NULL && length != 0) ||
        (registers != NULL && (length == 0 || length > 256))) {
        return NX_ERROR_INVALID;
    }
    s_spi = (nx_spi_port_t){.registers = registers,
                            .register_count = length,
                            .fail_after = SIZE_MAX};
    return NX_SUCCESS;
}

/** \brief Reject before CS, execute whole transaction, then settle CS/buffers.
 */
nx_result_t nx_spi_endpoint_transfer(const nx_spi_endpoint_t* endpoint,
                                     const uint8_t* tx, uint8_t* rx,
                                     size_t length, nx_time_us_t deadline,
                                     size_t* transferred) {
    if (endpoint == NULL || endpoint->port == NULL || transferred == NULL ||
        length == 0 || (tx == NULL && rx == NULL)) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_spi_port_t* port = endpoint->port;
    if (port->busy || port->cs) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->cs = true;
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
        if (port->registers != NULL) {
            if (i == 0) {
                read = (value & 0x80u) != 0;
                address = (uint8_t)(value & 0x7fu);
                received = 0;
            } else if (address >= port->register_count) {
                result = NX_ERROR_IO;
                break;
            } else if (read) {
                received = port->registers[address++];
            } else {
                port->registers[address++] = value;
                received = 0;
            }
        }
        if (rx != NULL) {
            rx[i] = received;
        }
        (*transferred)++;
        (void)nx_native_clock_advance(1);
    }
    port->cs = false;
    return result;
}

/** \brief Inject model faults without installing application callbacks. */
void nx_native_spi_fault(size_t fail_after, bool busy) {
    s_spi.fail_after = fail_after;
    s_spi.busy = busy;
}

/** \brief Expose modeled transaction boundary for ownership assertions. */
bool nx_native_spi_cs_active(void) {
    return s_spi.cs;
}
