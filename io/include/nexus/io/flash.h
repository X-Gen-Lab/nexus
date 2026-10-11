/**
 * \file            flash.h
 *
 * \brief           Full physical Flash geometry and explicit bounded regions.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_FLASH_H
#define NEXUS_FLASH_H

#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_flash_port nx_flash_port_t;
/**
 * \brief           Erase sector geometry, with offset relative to full
 *                  physical Flash.
 */
typedef struct {
    uint32_t offset;
    uint32_t size;
} nx_flash_sector_t;
/**
 * \brief           Read-only physical geometry; platform reserves no product
 *                  storage.
 */
typedef struct {
    uint32_t base_address;
    uint32_t size;
    uint32_t program_unit;
    const nx_flash_sector_t* sectors;
    size_t sector_count;
} nx_flash_geometry_t;
/**
 * \brief           Explicit consumer-selected bounds; no implicit product
 *                  partition.
 */
typedef struct {
    const nx_flash_port_t* port;
    uint32_t offset;
    uint32_t size;
    bool writable;
} nx_flash_region_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    const nx_flash_geometry_t* (*geometry)(const void* context);
    nx_result_t (*read)(const void* context, uint32_t offset, void* data,
                        size_t length);
    nx_result_t (*program)(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline);
    nx_result_t (*erase)(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline);
} nx_flash_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_flash_port {
    const nx_flash_ops_t* ops;
    void* context;
};

/**
 * \brief           Query exact physical Flash geometry without side effects.
 *
 * \param[in]       port: Selected exact-density physical Flash provider.
 *
 * \return          Immutable geometry, or NULL for invalid port.
 */
const nx_flash_geometry_t* nx_flash_port_geometry(const nx_flash_port_t* port);
/**
 * \brief           Read bounded physical Flash without retaining destination.
 *
 * \param[in]       port: Fixed provider; no concurrent erase/program.
 *
 * \param[in]       offset: Physical relative offset.
 *
 * \param[out]      data: Destination not retained beyond return.
 *
 * \param[in]       length: Byte count; zero permitted with NULL data.
 *
 * \return          Success or INVALID; no access on rejected range/overflow.
 */
nx_result_t nx_flash_port_read(const nx_flash_port_t* port, uint32_t offset,
                               void* data, size_t length);
/**
 * \brief           Program aligned physical Flash and settle all hardware
 *                  pulses.
 *
 * \param[in,out]   port: Single execution owner with valid voltage/clock
 *                  policy.
 *
 * \param[in]       offset: Program-unit aligned physical offset.
 *
 * \param[in]       data: Aligned-length input retained only until return.
 *
 * \param[in]       length: Positive program-unit multiple.
 *
 * \param[in]       deadline: Absolute deadline checked between hardware
 *                  pulses.
 *
 * \return          Success or INVALID/TIMEOUT/IO. Return proves no remaining
 *                  buffer/hardware access; an active pulse cannot be
 *                  preempted.
 *
 * \note            Task-only CPU-active operation. Product owns
 *                  execution-from- Flash stall, watchdog and power-loss
 *                  policy. No RAM fake on MCU.
 */
nx_result_t nx_flash_port_program(const nx_flash_port_t* port, uint32_t offset,
                                  const void* data, size_t length,
                                  nx_time_us_t deadline);
/**
 * \brief           Erase a whole physical sector range with pulse settlement.
 *
 * \param[in,out]   port: Single execution owner.
 *
 * \param[in]       offset: Exact sector-start physical offset.
 *
 * \param[in]       length: Positive sum of whole sectors.
 *
 * \param[in]       deadline: Absolute deadline checked between erase pulses.
 *
 * \return          Success or INVALID/TIMEOUT/IO; partial erase is possible on
 *                  timeout/error. Return waits for current pulse to become
 *                  idle.
 */
nx_result_t nx_flash_port_erase(const nx_flash_port_t* port, uint32_t offset,
                                size_t length, nx_time_us_t deadline);
/**
 * \brief           Validate region bounds against physical geometry.
 *
 * \param[in]       region: Explicit caller-owned region description.
 *
 * \return          Success or INVALID; erase geometry is checked per
 *                  operation.
 */
nx_result_t nx_flash_region_validate(const nx_flash_region_t* region);
/**
 * \brief           Read within an explicit physical region.
 *
 * \param[in]       region: Valid region.
 *
 * \param[in]       offset: Region-relative byte offset.
 *
 * \param[out]      data: Destination buffer.
 *
 * \param[in]       length: Byte count.
 *
 * \return          Success or INVALID without access outside the region.
 */
nx_result_t nx_flash_region_read(const nx_flash_region_t* region,
                                 uint32_t offset, void* data, size_t length);
/**
 * \brief           Program within an explicitly writable physical region.
 *
 * \param[in]       region: Valid writable region.
 *
 * \param[in]       offset: Region-relative aligned offset.
 *
 * \param[in]       data: Input buffer; borrowed until return only.
 *
 * \param[in]       length: Aligned positive length.
 *
 * \param[in]       deadline: Absolute pulse deadline.
 *
 * \return          Success, INVALID/PERMISSION or underlying pulse result.
 */
nx_result_t nx_flash_region_program(const nx_flash_region_t* region,
                                    uint32_t offset, const void* data,
                                    size_t length, nx_time_us_t deadline);
/**
 * \brief           Erase whole physical sectors within a writable region.
 *
 * \param[in]       region: Explicit valid writable region.
 *
 * \param[in]       offset: Region-relative sector boundary.
 *
 * \param[in]       length: Sum of whole sector lengths.
 *
 * \param[in]       deadline: Absolute pulse deadline.
 *
 * \return          Success, INVALID/PERMISSION or underlying pulse result.
 */
nx_result_t nx_flash_region_erase(const nx_flash_region_t* region,
                                  uint32_t offset, size_t length,
                                  nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif

#endif
