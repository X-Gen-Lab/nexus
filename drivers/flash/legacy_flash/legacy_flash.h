/**
 * \file            legacy_flash.h
 * \brief           Reviewed F103RB/GD303RC legacy physical Flash engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_LEGACY_FLASH_H
#define NX_LEGACY_FLASH_H
#include "nexus/arch/arch.h"
#include "nexus/io/flash.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact two-byte program ABI shared only after register review. */
typedef struct {
    volatile uint32_t access, key, option_key, status, control, address;
} nx_legacy_flash_registers_t;
/** \brief Full physical geometry; callers explicitly reserve their regions. */
typedef struct {
    nx_legacy_flash_registers_t* registers;
    volatile uint8_t* memory;
    const nx_flash_geometry_t* geometry;
    uint32_t supply_mv;
    bool initialized, active, fault;
} nx_legacy_flash_state_t;
extern const nx_flash_ops_t nx_legacy_flash_ops;
/** \brief Bind exact full geometry without erase/program side effects.
 * \param[in,out] port: Static task owner; supply declared 2.7..3.6V.
 * \return SUCCESS or INVALID/BUSY/CONTEXT; no product partition inferred.
 */
nx_result_t nx_legacy_flash_initialize(nx_legacy_flash_state_t* port);
/** \brief Prove no pulse, clear only owned operation modes, relock.
 * \param[in,out] port: Same owner, all reads/writes already joined.
 * \return SUCCESS or BUSY/IO; failed relock retains initialized state.
 */
nx_result_t nx_legacy_flash_close(nx_legacy_flash_state_t* port);
#ifndef NX_LEGACY_READ
#define NX_LEGACY_READ(reg) (*(reg))
#endif
#ifndef NX_LEGACY_WRITE
#define NX_LEGACY_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_LEGACY_POLL
#define NX_LEGACY_POLL(kind, port) ((void)(kind), (void)(port))
#endif
#ifndef NX_LEGACY_PROGRAM
#define NX_LEGACY_PROGRAM(pointer, value) (*(pointer) = (value))
#endif
#ifdef __cplusplus
}
#endif
#endif
