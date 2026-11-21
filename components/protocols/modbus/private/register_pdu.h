/**
 * \file            register_pdu.h
 * \brief           Shared private holding-register PDU execution
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_PRIVATE_MODBUS_REGISTER_PDU_H
#define NEXUS_PRIVATE_MODBUS_REGISTER_PDU_H
#include "nexus/components/modbus_registers.h"
#include <stddef.h>
/** \brief Execute one validated nonempty PDU in a serialized task.
 * \note Caller provides 253 writable response bytes and 125 scratch registers,
 *       all disjoint from input/callback state. Callback ports are validated
 *       at owner initialization. Returns 2..252 PDU response bytes. Framing,
 *       broadcast silence, CRC, MBAP and transport ownership stay with caller.
 */
size_t nx_modbus_register_pdu(const nx_modbus_registers_t* registers,
                              const uint8_t* pdu, size_t bytes,
                              uint8_t* response, uint16_t* values);
#endif
