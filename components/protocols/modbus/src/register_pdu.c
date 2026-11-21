/**
 * \file            register_pdu.c
 * \brief           One holding-register PDU implementation for RTU and TCP
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "register_pdu.h"
#include <string.h>

/** \brief Decode one unaligned big-endian wire value. */
static uint16_t get16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
/** \brief Emit one unaligned big-endian wire value. */
static void put16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}
/** \brief Keep unknown product errors within the Modbus exception vocabulary.
 */
static uint8_t exception(nx_modbus_register_result_t status) {
    return status == NX_MODBUS_ILLEGAL_ADDRESS ||
                   status == NX_MODBUS_ILLEGAL_VALUE ||
                   status == NX_MODBUS_DEVICE_BUSY
               ? (uint8_t)status
               : (uint8_t)NX_MODBUS_DEVICE_FAILURE;
}
/** \brief Validate the full request before invoking bounded product callbacks.
 */
size_t nx_modbus_register_pdu(const nx_modbus_registers_t* r, const uint8_t* p,
                              size_t bytes, uint8_t* out, uint16_t* values) {
    const uint8_t function = p[0];
    uint8_t error = NX_MODBUS_ILLEGAL_VALUE;
    if (function != 3 && function != 6 && function != 16) {
        error = 1;
        goto reject;
    }
    if (bytes < 5) {
        goto reject;
    }
    uint16_t start = get16(p + 1);
    uint16_t count = function == 6 ? 1 : get16(p + 3);
    if (count == 0 || (uint32_t)start + count > UINT32_C(65536) ||
        count > (function == 16 ? 123 : NX_MODBUS_REGISTER_MAX)) {
        goto reject;
    }
    if (function == 16) {
        if (bytes != 6U + (size_t)count * 2 || p[5] != (uint8_t)(count * 2U)) {
            goto reject;
        }
        for (uint16_t i = 0; i < count; ++i) {
            values[i] = get16(p + 6 + (size_t)i * 2);
        }
    } else if (bytes != 5) {
        goto reject;
    } else if (function == 6) {
        values[0] = get16(p + 3);
    }
    nx_modbus_register_result_t result;
    if (function == 3) {
        result = r->read(r->context, start, count, values);
        if (result == NX_MODBUS_OK) {
            out[0] = function;
            out[1] = (uint8_t)(count * 2U);
            for (uint16_t i = 0; i < count; ++i) {
                put16(out + 2 + (size_t)i * 2, values[i]);
            }
            return 2U + (size_t)count * 2;
        }
    } else {
        result = r->authorize_write(r->context, start, count, values);
        if (result == NX_MODBUS_OK) {
            result = r->write_atomic(r->context, start, count, values);
        }
        if (result == NX_MODBUS_OK) {
            memcpy(out, p, 5);
            return 5;
        }
    }
    error = exception(result);
reject:
    out[0] = (uint8_t)(function | 0x80U);
    out[1] = error;
    return 2;
}
