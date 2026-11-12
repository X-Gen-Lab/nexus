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
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Erase sector geometry, with offset relative to full
 *                  physical Flash.
 */
typedef struct {
    uint32_t offset;
    uint32_t size;
} nx_flash_sector_t;
/** \brief Consecutive equal erase sectors, relative to one physical device.
 * \note Positive erase_bytes/count; ordered spans exactly cover size with no
 *       gaps, overlap, overflow or CPU mapping/partition authorization.
 */
typedef struct {
    uint32_t offset;
    uint32_t erase_bytes;
    uint32_t count;
} nx_flash_erase_span_t;
/**
 * \brief           Physical programming requirements beyond unit alignment.
 */
enum nx_flash_program_flags {
    NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE = 1U << 0U
};
/**
 * \brief           Read-only physical geometry; platform reserves no product
 *                  storage.
 * \note            program_flags states additional unit requirements. Zero
 *                  does not waive the selected provider's programming rules.
 * \note            base_address is descriptive metadata, not a CPU mapping
 *                  or permission to dereference Flash. All I/O offsets remain
 *                  relative to this physical device; products own regions.
 */
typedef struct {
    uint32_t base_address;
    uint32_t size;
    uint32_t program_unit;
    const nx_flash_erase_span_t* erase_spans;
    size_t erase_span_count;
    uint32_t program_flags;
} nx_flash_geometry_t;
/** \brief Check complete immutable spans and derive their sector count.
 * \param[in] geometry: Stable readable geometry and complete readable spans.
 * \param[out] count: Aligned stable output disjoint from geometry and spans.
 * \return SUCCESS; INVALID leaves output and all inputs unchanged.
 * \note No MMIO, allocation, mapping permission or physical qualification.
 */
nx_result_t nx_flash_geometry_sector_count(const nx_flash_geometry_t* geometry,
                                           uint32_t* count);
/** \brief Resolve a logical sector ordinal to a physical relative value.
 * \param[in] geometry: Stable immutable complete physical description.
 * \param[in] ordinal: Zero-based sector index, strictly below derived count.
 * \param[out] sector: Aligned output disjoint from geometry and spans.
 * \return SUCCESS; INVALID leaves output and all inputs unchanged.
 * \note No pointer into provider metadata is returned.
 */
nx_result_t nx_flash_geometry_sector_at(const nx_flash_geometry_t* geometry,
                                        uint32_t ordinal,
                                        nx_flash_sector_t* sector);
/** \brief Find an exact sector start without rounding a physical offset.
 * \param[in] geometry: Stable immutable complete physical description.
 * \param[in] offset: Exact sector start; physical end is not a sector.
 * \param[out] ordinal: Aligned output disjoint from geometry and spans.
 * \return SUCCESS; INVALID leaves output and all inputs unchanged.
 */
nx_result_t nx_flash_geometry_sector_index(const nx_flash_geometry_t* geometry,
                                           uint32_t offset, uint32_t* ordinal);

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
    nx_result_t (*recover)(void* context);
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
 *
 * \note            With NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE, each programmed
 *                  unit needs a successful erase and no later program pulse.
 *                  Reading all 0xff cannot prove this on ECC Flash. A failed
 *                  or interrupted pulse consumes its unit until the next
 *                  successful sector erase. The caller owns this provenance;
 *                  providers keep no implicit erase-history bitmap.
 *
 * \note            All-0xff input units must not issue a program pulse. A
 *                  provider may reject a visibly non-erased destination.
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
 * \brief           Retry retained physical cleanup without another pulse.
 *
 * \param[in]       port: Same serialized owner as the failed operation.
 *
 * \return          SUCCESS after actual cleanup; BUSY while hardware remains
 *                  active, IO while cleanup is refused, or UNSUPPORTED when
 *                  this optional provider operation is absent.
 *
 * \note            Privileged unmasked Thread on MCU. This bounded attempt
 *                  never waits for a busy pulse, replays data, rolls back an
 *                  irreversible effect or substitutes for a required POR.
 *                  Preserve the owner until recovery succeeds. Recovery does
 *                  not turn the original failed operation into success.
 */
nx_result_t nx_flash_port_recover(const nx_flash_port_t* port);
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
