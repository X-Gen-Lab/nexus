/**
 * \file            flash_maintenance.h
 * \brief           Explicit physical OTP and option maintenance contract
 * \author          Nexus Team
 */
#ifndef NEXUS_FLASH_MAINTENANCE_H
#define NEXUS_FLASH_MAINTENANCE_H

#include "nexus/io/flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Destructive effects must each be acknowledged by the caller. */
enum nx_flash_maintenance_effects {
    NX_FLASH_MAINTAIN_OTP_PROGRAM = 1U,
    NX_FLASH_MAINTAIN_OTP_LOCK = 2U,
    NX_FLASH_MAINTAIN_OPTIONS = 4U,
    NX_FLASH_MAINTAIN_MASS_ERASE = 8U,
    NX_FLASH_MAINTAIN_PERMANENT = 16U
};

/** \brief Protection levels are semantic values, never vendor encodings. */
typedef enum {
    NX_FLASH_READ_OPEN,
    NX_FLASH_READ_PROTECTED,
    NX_FLASH_READ_PERMANENT
} nx_flash_read_protection_t;

/** \brief Complete desired physical options; no product layout is implied. */
typedef struct {
    nx_flash_read_protection_t read_protection;
    uint32_t write_protected_sectors;
    uint8_t brownout_level; /**< Encoded BOR field: 0=level3 .. 3=disabled. */
    bool hardware_watchdog;
    bool reset_on_stop;
    bool reset_on_standby;
} nx_flash_options_t;

/** \brief Copied hardware facts; options take effect only after POR. */
typedef struct {
    nx_flash_options_t options;
    uint32_t otp_bytes;
    uint32_t otp_block_bytes;
    uint32_t otp_locked_blocks;
    uint32_t sector_count;
    bool power_on_reset_required;
} nx_flash_maintenance_snapshot_t;

/** \brief Exact maintenance actions share the ordinary physical owner. */
typedef struct {
    nx_result_t (*snapshot)(void* context,
                            nx_flash_maintenance_snapshot_t* snapshot);
    nx_result_t (*otp_read)(void* context, uint32_t offset, void* data,
                            size_t length);
    nx_result_t (*otp_program)(void* context, uint32_t offset, const void* data,
                               size_t length, uint32_t acknowledged_effects,
                               nx_time_us_t deadline);
    nx_result_t (*otp_lock)(void* context, uint32_t block,
                            uint32_t acknowledged_effects,
                            nx_time_us_t deadline);
    nx_result_t (*set_options)(void* context, const nx_flash_options_t* desired,
                               uint32_t acknowledged_effects,
                               nx_time_us_t deadline);
} nx_flash_maintenance_ops_t;

/** \brief Immutable optional face, absent unless explicitly assembled. */
typedef struct {
    const nx_flash_maintenance_ops_t* ops;
    void* context;
} nx_flash_maintenance_port_t;

/**
 * \brief Copy physical maintenance state without retaining output storage.
 * \param[in] port: Same exclusive owner as the ordinary Flash face.
 * \param[out] snapshot: Caller storage valid until this synchronous return.
 * \return Actual facts, or invalid/context/busy/unsupported status.
 * \note Privileged unmasked Thread, serialized with all Flash operations.
 */
nx_result_t
nx_flash_maintenance_snapshot(const nx_flash_maintenance_port_t* port,
                              nx_flash_maintenance_snapshot_t* snapshot);

/**
 * \brief Read OTP data without exposing lock bytes as writable storage.
 * \param[in] port: Same serialized physical owner.
 * \param[in] offset: Byte offset inside the exact OTP data geometry.
 * \param[out] data: Caller storage borrowed only during this call.
 * \param[in] length: Entire extent, checked without wrap or rounding.
 * \return Success or rejection before data access.
 */
nx_result_t
nx_flash_maintenance_otp_read(const nx_flash_maintenance_port_t* port,
                              uint32_t offset, void* data, size_t length);

/**
 * \brief Program one-way OTP bits; partial irreversible progress is possible.
 * \param[in] port: Exclusive privileged unmasked Thread owner.
 * \param[in] offset: Provider-aligned OTP data offset.
 * \param[in] data: Stable source until synchronous return; never aliases OTP.
 * \param[in] length: Positive aligned complete extent.
 * \param[in] acknowledged_effects: Explicit OTP_PROGRAM effect declaration.
 * \param[in] deadline: Finite absolute monotonic deadline.
 * \return Success or error after the last started physical pulse settles.
 * \note No erase exists. Timeout cannot abort a pulse. Single-bank Flash
 *       fetches and interrupts may stall; deadlines are not latency bounds.
 *       Effect declarations are not authentication or permission from tests.
 */
nx_result_t nx_flash_maintenance_otp_program(
    const nx_flash_maintenance_port_t* port, uint32_t offset, const void* data,
    size_t length, uint32_t acknowledged_effects, nx_time_us_t deadline);

/**
 * \brief Permanently lock one exact OTP block with no automatic reset.
 * \param[in] port: Exclusive privileged unmasked Thread owner.
 * \param[in] block: Physical block index, never a main-array sector.
 * \param[in] acknowledged_effects: Explicit OTP_LOCK effect declaration.
 * \param[in] deadline: Finite absolute monotonic deadline.
 * \return Settled status; lock is irreversible even on a late timeout.
 */
nx_result_t
nx_flash_maintenance_otp_lock(const nx_flash_maintenance_port_t* port,
                              uint32_t block, uint32_t acknowledged_effects,
                              nx_time_us_t deadline);

/**
 * \brief Program a complete option image; activation requires external POR.
 * \param[in] port: Exclusive privileged unmasked Thread maintenance owner.
 * \param[in] desired: Stable complete options copied during this call.
 * \param[in] acknowledged_effects: OPTIONS plus MASS_ERASE for protected to
 *           open, or PERMANENT for irreversible level two.
 * \param[in] deadline: Finite absolute monotonic deadline.
 * \return Settled status; a timeout never rolls back programmed options.
 * \note Protected to open erases all user Flash and backup SRAM. The caller
 *       must execute that transition from independent RAM, with its vectors,
 *       code, constants, stack and return destination surviving mass erase.
 *       Level two permanently disables debug and further option changes.
 *       No automatic reset, power cycle, product policy or authentication.
 */
nx_result_t nx_flash_maintenance_set_options(
    const nx_flash_maintenance_port_t* port, const nx_flash_options_t* desired,
    uint32_t acknowledged_effects, nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif
