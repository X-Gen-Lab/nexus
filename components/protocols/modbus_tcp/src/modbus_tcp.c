/**
 * \file            modbus_tcp.c
 * \brief           Bounded MBAP sessions with explicit register authorization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/modbus_tcp.h"
#include "register_pdu.h"
#include <string.h>

/** \brief Reject invalid extents and any metadata/data overlap. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (an != 0 && bn != 0 && x < y + bn && y < x + an);
}
/** \brief Decode wire big endian without alignment assumptions. */
static uint16_t get16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
/** \brief Write one wire big-endian field. */
static void put16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}
/** \brief Execute the shared register engine in the reserved response slot. */
static void execute(nx_modbus_tcp_t* s) {
    memcpy(s->tx, s->rx, 7);
    size_t response = nx_modbus_register_pdu(
        &s->registers, s->rx + 7, s->expected - 7, s->tx + 7, s->values);
    put16(s->tx + 4, (uint16_t)(response + 1));
    s->response_size = response + 7;
}
/** \brief Bind callbacks without acquiring a socket or registering policy. */
nx_result_t nx_modbus_tcp_init(nx_modbus_tcp_t* s,
                               const nx_modbus_registers_t* registers,
                               uint8_t unit) {
    if (s == NULL || registers == NULL || unit == 0 || unit > 247 ||
        registers->read == NULL || registers->authorize_write == NULL ||
        registers->write_atomic == NULL ||
        overlap(s, sizeof(*s), registers, sizeof(*registers))) {
        return NX_ERROR_INVALID;
    }
    memset(s, 0, sizeof(*s));
    s->registers = *registers;
    s->unit = unit;
    return NX_SUCCESS;
}
/** \brief Preserve partial input and stop at one pending response boundary. */
nx_result_t nx_modbus_tcp_feed(nx_modbus_tcp_t* s, const uint8_t* data,
                               size_t size, size_t* consumed) {
    if (s == NULL || consumed == NULL || (data == NULL && size != 0) ||
        overlap(s, sizeof(*s), data, size) ||
        overlap(s, sizeof(*s), consumed, sizeof(*consumed)) ||
        overlap(data, size, consumed, sizeof(*consumed))) {
        return NX_ERROR_INVALID;
    }
    *consumed = 0;
    if (s->registers.read == NULL || s->failed) {
        return NX_ERROR_STATE;
    }
    if (s->active || s->response_size != 0) {
        return NX_ERROR_BUSY;
    }
    s->active = true;
    while (*consumed < size) {
        s->rx[s->received++] = data[(*consumed)++];
        if (s->received == 7) {
            const uint16_t length = get16(s->rx + 4);
            if (get16(s->rx + 2) != 0 || length < 2 || length > 254 ||
                s->rx[6] != s->unit) {
                s->failed = true;
                s->active = false;
                return NX_ERROR_INVALID;
            }
            s->expected = (size_t)length + 6;
        }
        if (s->expected != 0 && s->received == s->expected) {
            execute(s);
            s->active = false;
            return NX_SUCCESS;
        }
    }
    s->active = false;
    return NX_ERROR_BUSY;
}
/** \brief Publish immutable pending response storage without consuming it. */
nx_result_t nx_modbus_tcp_response(const nx_modbus_tcp_t* s,
                                   const uint8_t** data, size_t* size) {
    if (s == NULL || data == NULL || size == NULL ||
        overlap(s, sizeof(*s), data, sizeof(*data)) ||
        overlap(s, sizeof(*s), size, sizeof(*size)) ||
        overlap(data, sizeof(*data), size, sizeof(*size))) {
        return NX_ERROR_INVALID;
    }
    if (s->registers.read == NULL || s->failed || s->active) {
        return NX_ERROR_STATE;
    }
    if (s->response_size == 0) {
        return NX_ERROR_EMPTY;
    }
    *data = s->tx;
    *size = s->response_size;
    return NX_SUCCESS;
}
/** \brief Retire one copied/settled response before accepting another request.
 */
nx_result_t nx_modbus_tcp_release(nx_modbus_tcp_t* s) {
    if (s == NULL) {
        return NX_ERROR_INVALID;
    }
    if (s->failed || s->active || s->registers.read == NULL) {
        return NX_ERROR_STATE;
    }
    if (s->response_size == 0) {
        return NX_ERROR_EMPTY;
    }
    s->response_size = 0;
    s->received = 0;
    s->expected = 0;
    return NX_SUCCESS;
}
