/**
 * \file            CO_driver_target.h
 * \brief           Single-executor private CANopenNode classic-CAN profile
 * \author          Nexus Team
 */
#ifndef NEXUS_CANOPEN_DRIVER_TARGET_H
#define NEXUS_CANOPEN_DRIVER_TARGET_H
#include "nexus/io/can.h"
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(value)                    (value)
#define CO_SWAP_32(value)                    (value)
#define CO_SWAP_64(value)                    (value)
#define CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC     0
#define CO_CONFIG_NMT                        0
#define CO_CONFIG_HB_CONS                    CO_CONFIG_HB_CONS_ENABLE
#define CO_CONFIG_EM                         0
#define CO_CONFIG_SDO_SRV                    0
#define CO_CONFIG_PDO                        (CO_CONFIG_RPDO_ENABLE | CO_CONFIG_TPDO_ENABLE)
#define CO_CONFIG_ERR_CONDITION_MANUFACTURER 0
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Selected CANopen profile requires a little-endian target"
#endif
typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;
#define CO_CANrxMsg_readIdent(message)                                         \
    ((uint16_t)((nx_can_frame_t*)(message))->id)
#define CO_CANrxMsg_readDLC(message)  (((nx_can_frame_t*)(message))->dlc)
#define CO_CANrxMsg_readData(message) (((nx_can_frame_t*)(message))->data)
typedef struct {
    uint16_t ident;
    uint16_t mask;
    void* object;
    void (*CANrx_callback)(void*, void*);
} CO_CANrx_t;
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t data[8];
    bool_t bufferFull;
    bool_t syncFlag;
} CO_CANtx_t;
typedef struct {
    void* CANptr;
    CO_CANrx_t* rxArray;
    uint16_t rxSize;
    CO_CANtx_t* txArray;
    uint16_t txSize;
    uint16_t CANerrorStatus;
    bool_t CANnormal;
} CO_CANmodule_t;
/* All upstream RX callbacks and OD access run inside the same public step. */
#define CO_LOCK_CAN_SEND(module)   ((void)(module))
#define CO_UNLOCK_CAN_SEND(module) ((void)(module))
#define CO_LOCK_EMCY(module)       ((void)(module))
#define CO_UNLOCK_EMCY(module)     ((void)(module))
#define CO_LOCK_OD(module)         ((void)(module))
#define CO_UNLOCK_OD(module)       ((void)(module))
#define CO_MemoryBarrier()         ((void)0)
#define CO_FLAG_READ(flag)         ((flag) != NULL)
#define CO_FLAG_SET(flag)          ((flag) = (void*)1)
#define CO_FLAG_CLEAR(flag)        ((flag) = NULL)
#endif
