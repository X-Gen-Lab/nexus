/**
 * \file            modbus_registers.h
 * \brief           Shared bounded Modbus register and authorization contract
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_MODBUS_REGISTERS_H
#define NEXUS_COMPONENTS_MODBUS_REGISTERS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NX_MODBUS_REGISTER_MAX 125u
typedef enum {
    NX_MODBUS_OK = 0,
    NX_MODBUS_ILLEGAL_ADDRESS = 2,
    NX_MODBUS_ILLEGAL_VALUE = 3,
    NX_MODBUS_DEVICE_FAILURE = 4,
    NX_MODBUS_DEVICE_BUSY = 6
} nx_modbus_register_result_t;

/* Task context only. The caller serializes feed/poll/recover. No allocations.
 * read returns a coherent snapshot; write_atomic either commits EVERY value or
 * changes NONE. authorize is required and applies to broadcasts as well.
 * Critical writes check product authorization/interlocks here and recheck them
 * atomically inside write_atomic if product state has concurrent owners. RTU
 * itself authenticates neither a peer nor a request. All callbacks are bounded.
 */
typedef struct {
    void* context;
    nx_modbus_register_result_t (*read)(void*, uint16_t, uint16_t, uint16_t*);
    nx_modbus_register_result_t (*authorize_write)(void*, uint16_t, uint16_t,
                                                   const uint16_t*);
    nx_modbus_register_result_t (*write_atomic)(void*, uint16_t, uint16_t,
                                                const uint16_t*);
} nx_modbus_registers_t;

#ifdef __cplusplus
}
#endif
#endif
